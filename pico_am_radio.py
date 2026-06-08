"""
pico_am_radio.py  –  MicroPython port of PicoAM for Raspberry Pi Pico / Pico 2W
Plays MIDI files (type-0 or type-1) as OOK AM transmissions on GPIO 2.

Usage (Thonny / mpremote):
  1. Copy this file and your .mid files to the Pico's filesystem.
  2. Edit SONG_FILE below, or call play_midi("yourfile.mid") from the REPL.

Hardware:
  - GPIO 2  →  short wire antenna (tune your AM radio to ~1 MHz / 1000 kHz)
  - No other components needed.

Carrier: 1 MHz square wave via PIO state machine (same as the C original).
Gating:  GPIO alternates between PIO (carrier visible) and GPIO_OUT-low (silent)
         at each note's audio frequency to produce OOK-encoded melody.
"""

import rp2
import machine
import utime
import struct

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
TONE_GPIO       = 0          # RF output / antenna pin
CARRIER_FREQ_HZ = 1_000_000  # 1 MHz carrier
TEMPO_MS        = 500        # fallback ms-per-beat if MIDI has no tempo event
SONG_FILE       = "song.mid" # default file to play on boot

# Articulation: note sounds for this fraction of its duration, then rests.
ARTICULATION = 0.90

# ---------------------------------------------------------------------------
# PIO square-wave program (identical logic to squarewave.pio)
# ---------------------------------------------------------------------------
@rp2.asm_pio(set_init=rp2.PIO.OUT_LOW)
def squarewave():
    wrap_target()
    set(pins, 1)
    set(pins, 0)
    wrap()


_sm = None  # global state machine handle

def carrier_init():
    """Start the PIO carrier on TONE_GPIO."""
    global _sm
    # sys_clk / (2 * carrier_freq) gives the clkdiv for a 2-instruction loop
    sys_clk = machine.freq()
    clkdiv  = sys_clk / (2 * CARRIER_FREQ_HZ)
    _sm = rp2.StateMachine(
        0,
        squarewave,
        freq=int(sys_clk / clkdiv),  # effectively 2 * CARRIER_FREQ_HZ
        set_base=machine.Pin(TONE_GPIO),
    )
    _sm.active(1)
    _pin_out = machine.Pin(TONE_GPIO, machine.Pin.OUT, value=0)
    return _pin_out


_pin = None  # GPIO pin object for gating

def gate_on():
    """Hand the pin back to PIO so the carrier is visible."""
    machine.Pin(TONE_GPIO).init(machine.Pin.ALT, alt=6)  # ALT6 = PIO0 on RP2040/RP2350

def gate_off():
    """Drive the pin low via GPIO, masking the carrier."""
    p = machine.Pin(TONE_GPIO, machine.Pin.OUT, value=0)

# ---------------------------------------------------------------------------
# MIDI note → frequency → half-period (µs)
# ---------------------------------------------------------------------------
def midi_to_freq(note):
    """Return frequency in Hz for a MIDI note number (A4=69=440 Hz)."""
    return 440.0 * (2 ** ((note - 69) / 12))

def midi_to_half_period_us(note):
    """Return half-period in microseconds for OOK gating at the note frequency."""
    if note < 0:
        return 0
    f = midi_to_freq(note)
    if f <= 0:
        return 0
    hp = int(500_000 / f + 0.5)
    return max(hp, 1)

# ---------------------------------------------------------------------------
# Note player
# ---------------------------------------------------------------------------
def play_note(midi_note, dur_us):
    """
    Play a single note (or rest) for dur_us microseconds.
    midi_note < 0 → rest.
    """
    active_us = int(dur_us * ARTICULATION)
    rest_us   = dur_us - active_us

    if midi_note < 0:
        gate_off()
        utime.sleep_us(dur_us)
        return

    hp = midi_to_half_period_us(midi_note)
    if hp == 0:
        gate_off()
        utime.sleep_us(dur_us)
        return

    # OOK gating loop
    gate_off()
    t_start  = utime.ticks_us()
    t_active = utime.ticks_add(t_start, active_us)
    carrier_on = False
    next_toggle = t_start

    while utime.ticks_diff(t_active, utime.ticks_us()) > 0:
        now = utime.ticks_us()
        if utime.ticks_diff(utime.ticks_us(), next_toggle) >= 0:
            carrier_on = not carrier_on
            if carrier_on:
                gate_on()
            else:
                gate_off()
            next_toggle = utime.ticks_add(next_toggle, hp)
        else:
            remaining = utime.ticks_diff(next_toggle, now)
            if remaining > 200:
                utime.sleep_us(remaining - 100)

    if carrier_on:
        gate_off()

    # Articulation tail (silence)
    if rest_us > 0:
        utime.sleep_us(rest_us)


# ---------------------------------------------------------------------------
# Minimal MIDI parser  (type-0 and type-1, single-tempo)
# ---------------------------------------------------------------------------

class MidiParseError(Exception):
    pass


def _read_varlen(data, pos):
    """Read a MIDI variable-length integer. Returns (value, new_pos)."""
    val = 0
    while True:
        b = data[pos]; pos += 1
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return val, pos


def parse_midi(filename):
    """
    Parse a MIDI file and return a flat list of (midi_note, dur_us) tuples.
    Rests are represented as (-1, dur_us).
    Only channel 0 (or the first melodic track) note-on/off events are used.
    """
    with open(filename, "rb") as f:
        data = f.read()

    # --- Header chunk ---
    if data[0:4] != b"MThd":
        raise MidiParseError("Not a MIDI file")
    hdr_len = struct.unpack(">I", data[4:8])[0]
    fmt     = struct.unpack(">H", data[8:10])[0]
    ntrks   = struct.unpack(">H", data[10:12])[0]
    division= struct.unpack(">H", data[12:14])[0]

    if division & 0x8000:
        raise MidiParseError("SMPTE timecode MIDI not supported; use PPQ format")

    ticks_per_beat = division  # PPQ

    # --- Collect all events from all tracks into a merged timeline ---
    # We'll store (abs_tick, event_type, note, velocity) per track, then merge.

    tempo_us = 500_000  # default: 120 BPM

    all_events = []  # (abs_tick, note, velocity)

    pos = 14  # after header

    for _ in range(ntrks):
        if data[pos:pos+4] != b"MTrk":
            raise MidiParseError("Expected MTrk")
        trk_len = struct.unpack(">I", data[pos+4:pos+8])[0]
        trk_end = pos + 8 + trk_len
        pos += 8

        abs_tick = 0
        running_status = 0

        while pos < trk_end:
            delta, pos = _read_varlen(data, pos)
            abs_tick += delta

            b = data[pos]

            # Meta event
            if b == 0xFF:
                pos += 1
                meta_type = data[pos]; pos += 1
                meta_len, pos = _read_varlen(data, pos)
                if meta_type == 0x51 and meta_len == 3:
                    # Set tempo
                    tempo_us = (data[pos] << 16) | (data[pos+1] << 8) | data[pos+2]
                pos += meta_len
                running_status = 0
                continue

            # SysEx
            if b in (0xF0, 0xF7):
                pos += 1
                slen, pos = _read_varlen(data, pos)
                pos += slen
                running_status = 0
                continue

            # Status byte or running status
            if b & 0x80:
                status = b; pos += 1
            else:
                status = running_status

            running_status = status
            msg_type = status & 0xF0

            if msg_type == 0x90:  # Note On
                note = data[pos]; vel = data[pos+1]; pos += 2
                all_events.append((abs_tick, note, vel))
            elif msg_type == 0x80:  # Note Off
                note = data[pos]; pos += 2
                all_events.append((abs_tick, note, 0))
            elif msg_type in (0xA0, 0xB0, 0xE0):  # 2-byte data
                pos += 2
            elif msg_type in (0xC0, 0xD0):         # 1-byte data
                pos += 1
            else:
                pos += 1  # unknown, skip

        pos = trk_end

    if not all_events:
        raise MidiParseError("No note events found in MIDI file")

    # Sort by tick
    all_events.sort(key=lambda e: e[0])

    # --- Convert tick-based events to (note, dur_us) sequence ---
    # Strategy: track which note is "active" and emit it when it ends.
    # For polyphonic MIDI we just track the highest active note (melody).

    us_per_tick = tempo_us / ticks_per_beat

    active = {}   # note → start_tick
    segments = [] # (start_tick, end_tick, note)

    for (tick, note, vel) in all_events:
        if vel > 0:  # note on
            active[note] = tick
        else:        # note off
            if note in active:
                segments.append((active.pop(note), tick, note))

    # Close any still-active notes at last event tick
    last_tick = all_events[-1][0] if all_events else 0
    for note, start in active.items():
        segments.append((start, last_tick, note))

    segments.sort(key=lambda s: s[0])

    if not segments:
        raise MidiParseError("No complete note on/off pairs found")

    # Build flat (midi_note, dur_us) sequence with rests between notes
    result = []
    cursor = 0  # in ticks

    for (start, end, note) in segments:
        if start > cursor:
            # Gap → rest
            rest_ticks = start - cursor
            result.append((-1, int(rest_ticks * us_per_tick)))
        if end > start:
            result.append((note, int((end - start) * us_per_tick)))
        cursor = max(cursor, end)

    return result


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def play_midi(filename, loop=False):
    """
    Parse and play a MIDI file.
    Set loop=True to repeat forever (Ctrl-C to stop).
    """
    print(f"Parsing {filename} …")
    try:
        song = parse_midi(filename)
    except Exception as e:
        print(f"MIDI parse error: {e}")
        return

    total_notes = sum(1 for n, _ in song if n >= 0)
    total_s = sum(d for _, d in song) / 1_000_000
    print(f"  {total_notes} notes, {total_s:.1f}s duration")

    carrier_init()
    print("Playing (Ctrl-C to stop) …")

    try:
        while True:
            for (note, dur_us) in song:
                play_note(note, dur_us)
            if not loop:
                break
            utime.sleep_ms(1500)  # gap between repeats
    except KeyboardInterrupt:
        pass
    finally:
        gate_off()
        print("Done.")


def play_song(song, loop=False):
    """
    Play a hardcoded song given as a list of (midi_note, beats) tuples.
    midi_note = -1 for a rest.  beats is multiplied by TEMPO_MS.
    """
    carrier_init()
    try:
        while True:
            for (note, beats) in song:
                play_note(note, beats * TEMPO_MS * 1000)
            if not loop:
                break
            utime.sleep_ms(1500)
    except KeyboardInterrupt:
        pass
    finally:
        gate_off()


# ---------------------------------------------------------------------------
# Boot: play SONG_FILE if it exists, otherwise play built-in demo
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    try:
        open(SONG_FILE)
        play_midi(SONG_FILE, loop=True)
    except OSError:
        print(f"{SONG_FILE} not found – playing built-in demo")
        DEMO = [
            (72, 2), (67, 1), (67, 1), (68, 2),
            (67, 2), (-1, 2), (71, 2), (72, 2), (-1, 2),
        ]
        play_song(DEMO, loop=True)
