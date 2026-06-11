"""
radio.py  –  AM radio engine for Pico 2 W web controller.
Imported by main.py. Not intended to be run directly.

Carrier: PIO square wave on GP0 (physical Pin 1, top-left corner).
Gating:  OOK at each note's audio frequency.
Control: main.py sets _stop_flag to True to interrupt playback,
         and calls set_carrier_freq() before starting a new transmission.
"""

import rp2
import machine
import utime
import struct
import _thread

# ---------------------------------------------------------------------------
# Runtime state  (read/written by both cores)
# ---------------------------------------------------------------------------
TONE_GPIO    = 0              # GP0 — corner pin, safe antenna attachment
_carrier_hz  = 1_000_000      # current carrier frequency, Hz
_stop_flag   = False          # set True from web thread to abort playback
_playing     = False          # True while playback loop is running
_sm          = None           # PIO state machine handle
_play_lock   = _thread.allocate_lock()

FREQ_MIN_HZ  = 530_000        # 530 kHz — AM broadcast band lower bound
FREQ_MAX_HZ  = 1_700_000     # 1700 kHz — AM broadcast band upper bound

ARTICULATION = 0.90

# ---------------------------------------------------------------------------
# PIO square-wave (two-instruction toggle loop)
# ---------------------------------------------------------------------------
@rp2.asm_pio(set_init=rp2.PIO.OUT_LOW)
def _squarewave():
    wrap_target()
    set(pins, 1)
    set(pins, 0)
    wrap()


def set_carrier_freq(hz):
    """
    Change the carrier frequency (Hz). Clamped to FREQ_MIN_HZ–FREQ_MAX_HZ.
    Must NOT be called while _playing is True.
    """
    global _carrier_hz
    _carrier_hz = max(FREQ_MIN_HZ, min(FREQ_MAX_HZ, int(hz)))


def get_carrier_freq():
    return _carrier_hz


def _carrier_init():
    """(Re)start the PIO state machine at the current _carrier_hz."""
    global _sm
    if _sm is not None:
        _sm.active(0)
    sys_clk = machine.freq()
    # The loop is 2 instructions; each runs at sys_clk/clkdiv.
    # To get carrier_hz toggles per second we need 2*carrier_hz instructions/sec.
    target_pio_hz = 2 * _carrier_hz
    # clamp so clkdiv >= 1
    target_pio_hz = min(target_pio_hz, sys_clk)
    _sm = rp2.StateMachine(
        0,
        _squarewave,
        freq=target_pio_hz,
        set_base=machine.Pin(TONE_GPIO),
    )
    _sm.active(1)
    # immediately hand pin to GPIO-low so carrier is masked until a note gates it
    machine.Pin(TONE_GPIO, machine.Pin.OUT, value=0)


def _gate_on():
    machine.Pin(TONE_GPIO).init(machine.Pin.ALT, alt=6)  # PIO0 on RP2040/RP2350

def _gate_off():
    machine.Pin(TONE_GPIO, machine.Pin.OUT, value=0)


# ---------------------------------------------------------------------------
# MIDI note → half-period (µs)
# ---------------------------------------------------------------------------
def _midi_to_half_period_us(note):
    if note < 0:
        return 0
    f = 440.0 * (2 ** ((note - 69) / 12))
    if f <= 0:
        return 0
    return max(int(500_000 / f + 0.5), 1)


# ---------------------------------------------------------------------------
# Single note player  (checks _stop_flag each half-cycle)
# ---------------------------------------------------------------------------
def _play_note(midi_note, dur_us):
    """Returns False if _stop_flag fired mid-note."""
    global _stop_flag
    active_us = int(dur_us * ARTICULATION)
    rest_us   = dur_us - active_us

    if _stop_flag:
        return False

    if midi_note < 0:
        _gate_off()
        t_end = utime.ticks_add(utime.ticks_us(), dur_us)
        while utime.ticks_diff(t_end, utime.ticks_us()) > 0:
            if _stop_flag:
                return False
            utime.sleep_us(500)
        return True

    hp = _midi_to_half_period_us(midi_note)
    if hp == 0:
        _gate_off()
        utime.sleep_us(dur_us)
        return not _stop_flag

    _gate_off()
    t_active   = utime.ticks_add(utime.ticks_us(), active_us)
    carrier_on = False
    next_toggle = utime.ticks_us()

    while utime.ticks_diff(t_active, utime.ticks_us()) > 0:
        if _stop_flag:
            _gate_off()
            return False
        now = utime.ticks_us()
        if utime.ticks_diff(now, next_toggle) >= 0:
            carrier_on = not carrier_on
            if carrier_on:
                _gate_on()
            else:
                _gate_off()
            next_toggle = utime.ticks_add(next_toggle, hp)
        else:
            remaining = utime.ticks_diff(next_toggle, now)
            if remaining > 200:
                utime.sleep_us(min(remaining - 100, 500))

    if carrier_on:
        _gate_off()

    if rest_us > 0 and not _stop_flag:
        utime.sleep_us(rest_us)

    return not _stop_flag


# ---------------------------------------------------------------------------
# MIDI parser
# ---------------------------------------------------------------------------
class MidiParseError(Exception):
    pass

def _read_varlen(data, pos):
    val = 0
    while True:
        b = data[pos]; pos += 1
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return val, pos

def parse_midi(filename):
    with open(filename, "rb") as f:
        data = f.read()
    if data[0:4] != b"MThd":
        raise MidiParseError("Not a MIDI file")
    fmt      = struct.unpack(">H", data[8:10])[0]
    ntrks    = struct.unpack(">H", data[10:12])[0]
    division = struct.unpack(">H", data[12:14])[0]
    if division & 0x8000:
        raise MidiParseError("SMPTE timecode not supported; use PPQ")
    ticks_per_beat = division
    tempo_us = 500_000
    all_events = []
    pos = 14
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
            if b == 0xFF:
                pos += 1
                meta_type = data[pos]; pos += 1
                meta_len, pos = _read_varlen(data, pos)
                if meta_type == 0x51 and meta_len == 3:
                    tempo_us = (data[pos] << 16) | (data[pos+1] << 8) | data[pos+2]
                pos += meta_len
                running_status = 0
                continue
            if b in (0xF0, 0xF7):
                pos += 1
                slen, pos = _read_varlen(data, pos)
                pos += slen
                running_status = 0
                continue
            if b & 0x80:
                status = b; pos += 1
            else:
                status = running_status
            running_status = status
            msg_type = status & 0xF0
            if msg_type == 0x90:
                note = data[pos]; vel = data[pos+1]; pos += 2
                all_events.append((abs_tick, note, vel))
            elif msg_type == 0x80:
                note = data[pos]; pos += 2
                all_events.append((abs_tick, note, 0))
            elif msg_type in (0xA0, 0xB0, 0xE0):
                pos += 2
            elif msg_type in (0xC0, 0xD0):
                pos += 1
            else:
                pos += 1
        pos = trk_end
    if not all_events:
        raise MidiParseError("No note events found")
    all_events.sort(key=lambda e: e[0])
    us_per_tick = tempo_us / ticks_per_beat
    active = {}
    segments = []
    for (tick, note, vel) in all_events:
        if vel > 0:
            active[note] = tick
        else:
            if note in active:
                segments.append((active.pop(note), tick, note))
    last_tick = all_events[-1][0]
    for note, start in active.items():
        segments.append((start, last_tick, note))
    segments.sort(key=lambda s: s[0])
    if not segments:
        raise MidiParseError("No complete note on/off pairs found")
    result = []
    cursor = 0
    for (start, end, note) in segments:
        if start > cursor:
            result.append((-1, int((start - cursor) * us_per_tick)))
        if end > start:
            result.append((note, int((end - start) * us_per_tick)))
        cursor = max(cursor, end)
    return result


# ---------------------------------------------------------------------------
# Public playback control  (called from web thread via _thread)
# ---------------------------------------------------------------------------
def is_playing():
    return _playing

def stop():
    """Signal the playback thread to stop. Returns immediately."""
    global _stop_flag
    _stop_flag = True

def _playback_thread(filename):
    global _playing, _stop_flag
    _playing = True
    _stop_flag = False
    try:
        song = parse_midi(filename)
        _carrier_init()
        while not _stop_flag:
            for (note, dur_us) in song:
                if not _play_note(note, dur_us):
                    break
            break  # play once; main.py loops if desired
    except Exception as e:
        print("Playback error:", e)
    finally:
        _gate_off()
        if _sm is not None:
            _sm.active(0)
        _playing = False
        _stop_flag = False

def play(filename):
    """
    Start playback of filename on the second core.
    No-op if already playing.
    """
    if _playing:
        return False
    _thread.start_new_thread(_playback_thread, (filename,))
    return True
