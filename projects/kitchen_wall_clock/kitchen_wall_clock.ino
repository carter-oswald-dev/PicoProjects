#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "LcdDriver.h"
#include "UiText.h"

#if __has_include("src/config_local.h")
#include "src/config_local.h"
#endif
#include "src/config.h"

namespace {

constexpr uint16_t kColorBg = 0x0000;
constexpr uint16_t kColorFg = 0xFFFF;
constexpr uint16_t kColorAccent = 0x07FF;
constexpr uint16_t kColorWarn = 0xFFE0;
constexpr uint16_t kColorError = 0xF800;

constexpr const char kTzPacific[] = "PST8PDT,M3.2.0/2,M11.1.0/2";

constexpr const char kWeatherUrl[] =
    "https://api.open-meteo.com/v1/forecast?latitude=" APP_LATITUDE
    "&longitude=" APP_LONGITUDE
    "&current=temperature_2m,weather_code"
    "&daily=temperature_2m_min,temperature_2m_max"
    "&forecast_days=1"
    "&timezone=America%2FVancouver";

struct WeatherState {
  bool valid;
  bool stale;
  float temp_c;
  float min_c;
  float max_c;
  int weather_code;
  time_t last_success_utc;
};

struct AppState {
  uint16_t* frame_buffer;
  bool ui_dirty;
  bool wifi_connected;
  bool ntp_started;
  unsigned long next_wifi_retry_ms;
  unsigned long next_weather_fetch_ms;
  unsigned long last_rendered_utc_second;
  WeatherState weather;
};

AppState g_app = {};

const char* weather_code_text(int code) {
  switch (code) {
    case 0:
      return "CLEAR";
    case 1:
    case 2:
      return "PARTLY CLOUDY";
    case 3:
      return "CLOUDY";
    case 45:
    case 48:
      return "FOG";
    case 51:
    case 53:
    case 55:
      return "DRIZZLE";
    case 61:
    case 63:
    case 65:
      return "RAIN";
    case 71:
    case 73:
    case 75:
      return "SNOW";
    case 80:
    case 81:
    case 82:
      return "SHOWERS";
    case 95:
      return "THUNDER";
    default:
      return "UNKNOWN";
  }
}

int rounded_temp(float t) {
  return (t >= 0.0f) ? static_cast<int>(t + 0.5f) : static_cast<int>(t - 0.5f);
}

bool wifi_credentials_configured() {
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 ||
      strcmp(WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0 ||
      strcmp(WIFI_SSID, "your-wifi-ssid") == 0 ||
      strcmp(WIFI_PASSWORD, "your-wifi-password") == 0) {
    return false;
  }
  return WIFI_SSID[0] != '\0' && WIFI_PASSWORD[0] != '\0';
}

const char* find_key_in_range(const char* start, const char* end, const char* key) {
  const size_t key_len = strlen(key);
  const char* cursor = start;
  while (cursor && cursor < end) {
    const char* found = strstr(cursor, key);
    if (!found || found >= end) {
      return nullptr;
    }
    if (found + key_len <= end) {
      return found;
    }
    cursor = found + 1;
  }
  return nullptr;
}

bool parse_number_for_key(const char* start, const char* end, const char* key, double* out_value) {
  if (!start || !end || !key || !out_value || end <= start) {
    return false;
  }

  const char* key_pos = find_key_in_range(start, end, key);
  if (!key_pos) {
    return false;
  }

  const char* colon = strchr(key_pos, ':');
  if (!colon || colon >= end) {
    return false;
  }

  const char* value_start = colon + 1;
  while (value_start < end && isspace(static_cast<unsigned char>(*value_start))) {
    ++value_start;
  }

  while (value_start < end) {
    const char c = *value_start;
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
      break;
    }
    ++value_start;
  }

  char* parse_end = nullptr;
  const double parsed = strtod(value_start, &parse_end);
  if (parse_end == value_start) {
    return false;
  }

  *out_value = parsed;
  return true;
}

bool parse_open_meteo(const String& payload, float* temp_c, float* min_c, float* max_c, int* weather_code) {
  if (!temp_c || !min_c || !max_c || !weather_code) {
    return false;
  }

  const char* body = payload.c_str();
  const char* current_key = strstr(body, "\"current\"");
  if (!current_key) {
    return false;
  }

  const char* current_obj_start = strchr(current_key, '{');
  if (!current_obj_start) {
    return false;
  }

  const char* current_obj_end = strchr(current_obj_start, '}');
  if (!current_obj_end) {
    return false;
  }

  double parsed_temp = 0.0;
  double parsed_code = 0.0;
  if (!parse_number_for_key(current_obj_start, current_obj_end, "\"temperature_2m\"", &parsed_temp)) {
    return false;
  }
  if (!parse_number_for_key(current_obj_start, current_obj_end, "\"weather_code\"", &parsed_code)) {
    return false;
  }

  *temp_c = static_cast<float>(parsed_temp);
  *weather_code = static_cast<int>(parsed_code);

  const char* daily_key = strstr(body, "\"daily\"");
  if (!daily_key) {
    *min_c = *temp_c;
    *max_c = *temp_c;
    return true;
  }

  const char* daily_obj_start = strchr(daily_key, '{');
  if (!daily_obj_start) {
    *min_c = *temp_c;
    *max_c = *temp_c;
    return true;
  }

  const char* daily_obj_end = strchr(daily_obj_start, '}');
  if (!daily_obj_end) {
    *min_c = *temp_c;
    *max_c = *temp_c;
    return true;
  }

  double parsed_min = parsed_temp;
  double parsed_max = parsed_temp;
  (void)parse_number_for_key(daily_obj_start, daily_obj_end, "\"temperature_2m_min\"", &parsed_min);
  (void)parse_number_for_key(daily_obj_start, daily_obj_end, "\"temperature_2m_max\"", &parsed_max);
  *min_c = static_cast<float>(parsed_min);
  *max_c = static_cast<float>(parsed_max);
  return true;
}

time_t current_utc_epoch() {
  const time_t now = time(nullptr);
  if (now < 10000000) {
    return 0;
  }
  return now;
}

bool current_local_datetime(struct tm* out_local) {
  if (!out_local) {
    return false;
  }
  const time_t now = current_utc_epoch();
  if (now == 0) {
    return false;
  }
  return localtime_r(&now, out_local) != nullptr;
}

void schedule_weather_fetch_now() {
  g_app.next_weather_fetch_ms = millis();
}

void schedule_weather_fetch_later(unsigned long delay_ms) {
  g_app.next_weather_fetch_ms = millis() + delay_ms;
}

void maybe_start_ntp() {
  if (g_app.ntp_started || !g_app.wifi_connected) {
    return;
  }
  NTP.begin(NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
  g_app.ntp_started = true;
}

void maybe_reconnect_wifi() {
  const bool now_connected = (WiFi.status() == WL_CONNECTED);
  if (now_connected != g_app.wifi_connected) {
    g_app.wifi_connected = now_connected;
    g_app.ui_dirty = true;
    if (now_connected) {
      schedule_weather_fetch_now();
    }
  }

  if (now_connected || !wifi_credentials_configured()) {
    return;
  }

  const unsigned long now_ms = millis();
  if (now_ms < g_app.next_wifi_retry_ms) {
    return;
  }

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  g_app.next_wifi_retry_ms = now_ms + WIFI_RETRY_INTERVAL_MS;
}

void fetch_weather_now() {
  WiFiClientSecure tls_client;
  tls_client.setInsecure();

  HTTPClient https;
  if (!https.begin(tls_client, kWeatherUrl)) {
    schedule_weather_fetch_later(WEATHER_RETRY_INTERVAL_MS);
    g_app.ui_dirty = true;
    return;
  }

  const int http_code = https.GET();
  if (http_code == HTTP_CODE_OK) {
    const String payload = https.getString();
    float temp_c = 0.0f;
    float min_c = 0.0f;
    float max_c = 0.0f;
    int weather_code = 0;
    if (parse_open_meteo(payload, &temp_c, &min_c, &max_c, &weather_code)) {
      g_app.weather.valid = true;
      g_app.weather.stale = false;
      g_app.weather.temp_c = temp_c;
      g_app.weather.min_c = min_c;
      g_app.weather.max_c = max_c;
      g_app.weather.weather_code = weather_code;
      g_app.weather.last_success_utc = current_utc_epoch();
      schedule_weather_fetch_later(WEATHER_UPDATE_INTERVAL_MS);
      g_app.ui_dirty = true;
      https.end();
      return;
    }
  }

  schedule_weather_fetch_later(WEATHER_RETRY_INTERVAL_MS);
  g_app.ui_dirty = true;
  https.end();
}

void maybe_fetch_weather() {
  if (!g_app.wifi_connected) {
    return;
  }
  if (millis() < g_app.next_weather_fetch_ms) {
    return;
  }
  fetch_weather_now();
}

void update_weather_staleness() {
  if (!g_app.weather.valid) {
    return;
  }

  const time_t now = current_utc_epoch();
  if (now == 0) {
    return;
  }

  const bool stale = (now - g_app.weather.last_success_utc) > WEATHER_STALE_AFTER_SEC;
  if (stale != g_app.weather.stale) {
    g_app.weather.stale = stale;
    g_app.ui_dirty = true;
  }
}

void maybe_mark_time_dirty() {
  const time_t now = current_utc_epoch();
  if (now == 0) {
    return;
  }
  const unsigned long sec = static_cast<unsigned long>(now);
  if (sec != g_app.last_rendered_utc_second) {
    g_app.last_rendered_utc_second = sec;
    g_app.ui_dirty = true;
  }
}

void draw_screen() {
  if (!g_app.ui_dirty || !g_app.frame_buffer) {
    return;
  }

  g_app.ui_dirty = false;
  UiClear(kColorBg);

  char line[64];

  struct tm now_local = {};
  if (current_local_datetime(&now_local)) {
    strftime(line, sizeof(line), "%H:%M", &now_local);
    UiDrawTextGiant(15, 8, line, kColorAccent, kColorBg);

    strftime(line, sizeof(line), "%b %d, %Y", &now_local);
    UiDrawTextMedium(12, 96, line, kColorFg, kColorBg);
  } else {
    UiDrawTextMedium(12, 96, "TIME SYNCING", kColorWarn, kColorBg);
  }

  if (g_app.weather.valid) {
    snprintf(line, sizeof(line), "%dC  %d/%dC",
             rounded_temp(g_app.weather.temp_c),
             rounded_temp(g_app.weather.min_c),
             rounded_temp(g_app.weather.max_c));
    UiDrawTextMedium(12, 150, line, kColorFg, kColorBg);

    snprintf(line, sizeof(line), "%s", weather_code_text(g_app.weather.weather_code));
    UiDrawTextLarge(12, 188, line, g_app.weather.stale ? kColorWarn : kColorFg, kColorBg);
  } else {
    UiDrawTextLarge(12, 188, "WX UNAVAILABLE", kColorWarn, kColorBg);
  }

  if (!wifi_credentials_configured()) {
    UiDrawText(4, 232, "WIFI: SET SRC/CONFIG_LOCAL.H", kColorWarn, kColorBg);
  } else if (g_app.wifi_connected) {
    UiDrawText(4, 232, "WIFI: OK", kColorFg, kColorBg);
  } else {
    UiDrawText(4, 232, "WIFI: RECONNECTING", kColorWarn, kColorBg);
  }

  LcdWriteToScreen();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  setenv("TZ", kTzPacific, 1);
  tzset();

  LcdModuleInit();
  LcdBacklightPercent(80);
  g_app.frame_buffer = LcdDisplayInit(SCAN_DIR_HORIZONTAL);
  if (g_app.frame_buffer) {
    UiInit(g_app.frame_buffer, SCREEN_WIDTH_HEIGHT, SCREEN_WIDTH_HEIGHT);
  }

  WiFi.mode(WIFI_STA);

  g_app.ui_dirty = true;
  g_app.wifi_connected = false;
  g_app.ntp_started = false;
  g_app.next_wifi_retry_ms = 0;
  g_app.last_rendered_utc_second = static_cast<unsigned long>(-1);
  g_app.weather.valid = false;
  g_app.weather.stale = false;
  g_app.weather.temp_c = 0.0f;
  g_app.weather.min_c = 0.0f;
  g_app.weather.max_c = 0.0f;
  g_app.weather.weather_code = 0;
  g_app.weather.last_success_utc = 0;
  schedule_weather_fetch_later(WEATHER_RETRY_INTERVAL_MS);
}

void loop() {
  maybe_reconnect_wifi();
  maybe_start_ntp();
  maybe_fetch_weather();
  update_weather_staleness();
  maybe_mark_time_dirty();
  draw_screen();
  delay(100);
}
