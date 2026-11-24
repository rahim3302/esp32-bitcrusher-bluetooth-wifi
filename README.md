# ESP32 Bitcrusher with Bluetooth Output and Wi-Fi Control

This project combines a PCM bitcrusher, Bluetooth audio output, and a local Wi-Fi control page. The current firmware uses a generated sine wave to exercise the audio path and effect controls.

## Why I built it

I started with two separate experiments: generating a sine wave on the ESP32 and playing it through the Logi Z207 over Bluetooth, and writing the bitcrusher DSP separately. I then brought those pieces together. My first control version used three analog potentiometers; I replaced them with a web interface hosted on the ESP32 so I could adjust the effect from a phone or computer.

My goal is to explore practical audio effects with wireless control and Bluetooth output, reducing the need for separate audio and control cables and moving toward compact, functional hardware.

## Main files

- `src/main.cpp` — generated tone, A2DP frame callback, and Wi-Fi control page.
- `src/bitcrusher.c` and `include/bitcrusher.h` — portable C DSP: bit-depth quantization and sample holding.
- `platformio.ini` — ESP32 board, framework, and pinned A2DP dependency.
- `include/wifi_config.example.h` — safe template for local Wi-Fi credentials.

## Hardware and build

This configuration targets PlatformIO's `esp32dev` board profile. Bluetooth output is configured for the Logitech Z207, advertised as `Logi Z207`.

1. Open this folder in VS Code with PlatformIO installed.
2. Copy `include/wifi_config.example.h` to `include/wifi_config.h` and enter the name and password of a 2.4 GHz Wi-Fi network. The real credentials file is ignored by Git.
3. Build and upload the project with PlatformIO.
4. Open the serial monitor at **115200 baud**. Pair the Z207 if it is not already paired. The ESP32 prints its Wi-Fi control URL after it joins the network.
5. Open that URL on a phone or computer connected to the same local network. Adjust bit depth, sample hold, output level, and bypass.

Without `wifi_config.h`, firmware still builds and Bluetooth can run, but the web controls remain unavailable.

## Signal path

```text
32-sample sine table
        ↓
bit-depth quantizer → sample-and-hold → output level
        ↓
stereo 16-bit PCM frames → ESP32-A2DP / Bluetooth Classic → Logitech Z207
                                                       ↑
                          browser controls → Wi-Fi STA → WebServer
```

The 32-sample table repeats at the A2DP output rate, producing about **1.38 kHz** when the stream is 44.1 kHz. This is a deliberately simple test signal, not a 440 Hz input and not a musical instrument input. Both stereo channels receive the same processed sample.

The bit-depth setting changes the PCM quantization step. The sample-hold setting repeats each processed sample for 1 to 8 output frames. The output-level slider scales the signal after the effect. Bypass sends the original generated sample while the effect state continues to update.

## Wi-Fi and Bluetooth behavior

The control page uses Wi-Fi station mode, so the ESP32 and browser must be on the same network. The ESP32's 2.4 GHz radio is shared between Wi-Fi and Bluetooth; Espressif documents time-division coexistence and lists Wi-Fi station plus Classic Bluetooth as a supported combination. Real audio quality and dropouts still need to be checked on the target board and network. See [Espressif's RF coexistence guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-guides/coexist.html).

The prototype page has no login; keep it on a trusted local network and do not expose it through router port forwarding.

The A2DP data callback does only bounded sample processing and frame writes. Wi-Fi serving, serial output, and waits stay outside that callback.

## Tested so far

- The ESP32 connected to Wi-Fi and printed its local control-page URL in the Serial Monitor.
- I opened the control page in a browser and captured a screenshot.
- I recorded a live video of the prototype.

## Demo

![Control page](docs/media/interface%20GUI.png)

[Watch the live demo video](docs/media/Test_MP4.mp4)

## Current limits

- The input is a generated 32-sample sine table, not live audio. At a 44.1 kHz output rate, it produces a tone of about 1.38 kHz.
- The firmware does not yet read a microphone, line input, or audio file.
- Audio latency and quality have not been measured systematically.
