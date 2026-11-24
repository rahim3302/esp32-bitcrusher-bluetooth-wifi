#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <WebServer.h>
#include <WiFi.h>

#include <atomic>
#include <math.h>
#include <string.h>

#include "bitcrusher.h"

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#define HAS_WIFI_CONFIG 1
#else
#define HAS_WIFI_CONFIG 0
#endif

namespace {

constexpr uint8_t kTableSize = 32;
constexpr int16_t kTonePeak = 28000;
constexpr uint8_t kInitialBitDepth = 8;
constexpr uint8_t kInitialHoldFactor = 1;
constexpr uint8_t kInitialVolumePercent = 50;

int16_t sine_table[kTableSize];
BitcrusherState crusher_state;
BluetoothA2DPSource a2dp_source;
WebServer web_server(80);
bool wifi_ready = false;

std::atomic<uint8_t> bit_depth{kInitialBitDepth};
std::atomic<uint8_t> hold_factor{kInitialHoldFactor};
std::atomic<uint8_t> volume_percent{kInitialVolumePercent};
std::atomic<bool> bypass_effect{false};

const char kControlPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="utf-8">
  <title>ESP32 Bitcrusher</title>
  <style>
    body { font: 16px system-ui, sans-serif; max-width: 34rem; margin: 2rem auto; padding: 0 1rem; }
    label { display: block; margin: 1.5rem 0 .35rem; }
    input[type=range] { width: 100%; }
    .value { font-variant-numeric: tabular-nums; }
  </style>
</head>
<body>
  <h1>ESP32 Bitcrusher</h1>
  <p>Controls the generated test tone sent to the Bluetooth speaker.</p>
  <label for="bits">Bit depth: <span class="value" id="bitsValue">8</span></label>
  <input id="bits" type="range" min="1" max="16" value="8">
  <label for="hold">Sample hold: <span class="value" id="holdValue">1</span> frame(s)</label>
  <input id="hold" type="range" min="1" max="8" value="1">
  <label for="volume">Output level: <span class="value" id="volumeValue">50</span>%</label>
  <input id="volume" type="range" min="0" max="100" value="50">
  <p><label><input id="bypass" type="checkbox"> Bypass bitcrusher</label></p>
  <script>
    const bits = document.querySelector('#bits');
    const hold = document.querySelector('#hold');
    const volume = document.querySelector('#volume');
    const bypass = document.querySelector('#bypass');
    let sendTimer;
    function sendControls() {
      document.querySelector('#bitsValue').textContent = bits.value;
      document.querySelector('#holdValue').textContent = hold.value;
      document.querySelector('#volumeValue').textContent = volume.value;
      clearTimeout(sendTimer);
      sendTimer = setTimeout(() => {
        const query = new URLSearchParams({bits: bits.value, hold: hold.value,
                                           volume: volume.value, bypass: bypass.checked ? '1' : '0'});
        fetch('/control?' + query.toString()).catch(() => {});
      }, 70);
    }
    [bits, hold, volume, bypass].forEach(control => control.addEventListener('input', sendControls));
    bypass.addEventListener('change', sendControls);
  </script>
</body>
</html>
)HTML";

int32_t get_audio_frames(Frame *frames, int32_t frame_count)
{
    static uint8_t sample_index = 0U;

    // Read each control once per callback so the audio loop stays predictable.
    const uint8_t selected_bit_depth = bit_depth.load(std::memory_order_relaxed);
    const uint8_t selected_hold_factor = hold_factor.load(std::memory_order_relaxed);
    const uint8_t selected_volume = volume_percent.load(std::memory_order_relaxed);
    const bool selected_bypass = bypass_effect.load(std::memory_order_relaxed);

    for (int32_t i = 0; i < frame_count; ++i) {
        const int16_t input_sample = sine_table[sample_index];
        sample_index = (uint8_t)((sample_index + 1U) % kTableSize);

        const int16_t crushed_sample = bitcrusher_process_sample(
            &crusher_state,
            input_sample,
            selected_bit_depth,
            selected_hold_factor
        );
        const int16_t effect_sample = selected_bypass ? input_sample : crushed_sample;
        const int16_t output_sample = (int16_t)(
            ((int32_t)effect_sample * selected_volume) / 100
        );

        frames[i].channel1 = output_sample;
        frames[i].channel2 = output_sample;
    }

    return frame_count;
}

void handle_control()
{
    if (web_server.hasArg("bits")) {
        const long value = web_server.arg("bits").toInt();
        bit_depth.store((uint8_t)constrain(value, 1L, 16L), std::memory_order_relaxed);
    }
    if (web_server.hasArg("hold")) {
        const long value = web_server.arg("hold").toInt();
        hold_factor.store((uint8_t)constrain(value, 1L, 8L), std::memory_order_relaxed);
    }
    if (web_server.hasArg("volume")) {
        const long value = web_server.arg("volume").toInt();
        volume_percent.store((uint8_t)constrain(value, 0L, 100L), std::memory_order_relaxed);
    }
    if (web_server.hasArg("bypass")) {
        bypass_effect.store(web_server.arg("bypass") == "1", std::memory_order_relaxed);
    }

    web_server.send(200, "application/json", "{\"ok\":true}");
}

void start_wifi_control()
{
#if HAS_WIFI_CONFIG
    if (strcmp(WIFI_SSID, "YOUR_2_4_GHZ_WIFI_NAME") == 0 || WIFI_SSID[0] == '\0') {
        Serial.println("Wi-Fi credentials are still examples; copy wifi_config.example.h to wifi_config.h and edit it.");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");

    const unsigned long started_at = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started_at < 12000UL) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi did not connect. Bluetooth audio will continue; check the credentials.");
        return;
    }

    web_server.on("/", HTTP_GET, []() {
        web_server.send_P(200, "text/html", kControlPage);
    });
    web_server.on("/control", HTTP_GET, handle_control);
    web_server.begin();
    wifi_ready = true;

    Serial.print("Wi-Fi controls: http://");
    Serial.println(WiFi.localIP());
#else
    Serial.println("Wi-Fi control is not configured. Copy wifi_config.example.h to wifi_config.h and add your network details.");
#endif
}

} // namespace

void setup()
{
    Serial.begin(115200);

    for (uint8_t i = 0; i < kTableSize; ++i) {
        const float angle = (2.0f * PI * (float)i) / (float)kTableSize;
        sine_table[i] = (int16_t)(sinf(angle) * (float)kTonePeak);
    }
    bitcrusher_init(&crusher_state);

    a2dp_source.set_data_callback_in_frames(get_audio_frames);
    a2dp_source.start("Logi Z207");
    Serial.println("Bluetooth source started; pair the Logi Z207 speaker if needed.");

    start_wifi_control();
}

void loop()
{
    if (wifi_ready) {
        web_server.handleClient();
    }
    delay(2);
}
