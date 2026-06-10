# G-Talk for MeshTastic V3.2

**Secure LoRa Mesh Communication with Post-Quantum Cryptography (Kyber-512) + AES-256-GCM**  
*Copyright (C) 2026 Emanuele Ferraro & GeoReport International Technologies*

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)
[![LoRa: SX1262](https://img.shields.io/badge/LoRa-SX1262-green)](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
[![Crypto: Kyber-512](https://img.shields.io/badge/Encryption-Kyber--512-red)](https://pq-crystals.org/kyber/)
[![License: MIT](https://img.shields.io/badge/License-GNUV3-yellow.svg)](LICENSE)

## Overview

G-Mesh is a robust, off-grid communication system for ESP32 microcontrollers equipped with SX1262 LoRa transceivers and OLED displays. It combines a **mesh‑capable LoRa protocol** with **post‑quantum cryptography** (Kyber‑512 for key exchange and AES‑256‑GCM for payload encryption), enabling secure, long‑range, low‑power messaging even in disaster scenarios or remote areas without cellular coverage.

The system supports:
- Point‑to‑point and broadcast messaging
- Automatic packet relaying (TTL‑based mesh)
- Fragmentation and reassembly of large messages
- Emergency SOS (medical alert)
- Heartbeat presence notification
- Web‑based control panel via built‑in WiFi AP
- Deep sleep with wake‑on‑LoRa or button

## Features

- ✅ **Secure by default** – Kyber‑512 (post‑quantum) key exchange + AES‑256‑GCM authenticated encryption
- ✅ **Mesh networking** – Packets are forwarded up to `DEFAULT_TTL=3` hops
- ✅ **Reliable delivery** – ACK/NACK for unicast messages; automatic retransmission timeout (120 s)
- ✅ **Large message support** – Automatic fragmentation (200 byte chunks) and reassembly
- ✅ **Emergency alerts** – Dedicated message types for medical SOS, lost person, low battery, etc.
- ✅ **OLED user interface** – Shows device ID, security status, last message, signal strength
- ✅ **WiFi configuration portal** – Access point with simple web form to send chat, SOS, or start Kyber handshake
- ✅ **Low‑power operation** – Deep sleep after 10 minutes inactivity; wake on LoRa packet or button press
- ✅ **Serial debug** – Commands to send test messages, start handshake, or encrypted test

## Hardware Requirements

| Component               | Recommended Model / Specification                        |
|-------------------------|-----------------------------------------------------------|
| Microcontroller         | ESP32 (any variant with sufficient RAM, e.g., ESP32‑WROOM) |
| LoRa Transceiver        | SX1262 (SPI interface)                                   |
| OLED Display            | SSD1306 (128×64, I2C)                                    |
| Power                   | 3.3 V – 5 V (battery with voltage divider for ADC reading) |
| Buttons                 | BOOT button (GPIO0) for wake‑up; LoRa DIO1 for wake‑on‑radio |
| Antenna                 | Suitable 868 MHz (or 915 MHz) LoRa antenna               |

### Pin Connections

| **LoRa (SX1262)** | ESP32 Pin |
|-------------------|-----------|
| NSS (CS)          | 8         |
| SCK               | 9         |
| MOSI              | 10        |
| MISO              | 11        |
| RESET             | 12        |
| BUSY              | 13        |
| DIO1              | 14        |

| **OLED (SSD1306)** | ESP32 Pin |
|--------------------|-----------|
| SDA                | 17        |
| SCL                | 18        |
| RST (display reset)| 21        |
| Power control      | 36        |

| **Other**          | ESP32 Pin |
|--------------------|-----------|
| Button (wake)      | 0 (BOOT)  |
| Battery ADC        | 1 (ADC1)  |

> **Note**: Adjust pins in the source code if your board has different assignments. The power control pin (`OLED_PWR`) drives a transistor or MOSFET that supplies VCC to the OLED. If not needed, tie it HIGH or remove the `digitalWrite(OLED_PWR, LOW)` lines.

## Software Dependencies

Install the following Arduino libraries (via Library Manager or manually):

| Library                                                                 | Purpose                          |
|-------------------------------------------------------------------------|----------------------------------|
| [RadioLib](https://github.com/jgromes/RadioLib)                         | SX1262 driver                    |
| [mbedtls](https://github.com/ARMmbed/mbedtls) (included in ESP32 core)  | AES‑GCM encryption               |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)        | OLED graphics                    |
| [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)        | OLED driver                      |
| [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)     | Async web server                 |
| [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) (ESP32)               | Required for AsyncWebServer      |
| [PQCrypto-Kyber](https://github.com/itzmeanjan/Arduino-PQCrypto-Kyber)  | Kyber‑512 implementation         |

The code also uses:
- `esp_mac.h` – for reading MAC address
- `WiFi.h` – for soft‑AP mode
- `SPI.h` – custom SPI bus for LoRa
- `Wire.h` – I2C for OLED

> **Important**: The Kyber library must provide `pqc_kyber.h` with functions `pqc_kyber512_keypair`, `pqc_kyber512_encapsulate`, and `pqc_kyber512_decapsulate`. Adjust the include path if needed.

## Installation & Setup

1. **Clone the repository** (or copy the main `.ino` file).
2. **Open with Arduino IDE** (or PlatformIO). Ensure you have the **ESP32 board package** installed (e.g., `esp32 by Espressif Systems`, version 2.0.x or later).
3. **Install required libraries** (see “Software Dependencies”).
4. **Verify pin assignments** in the code (search for `#define LORA_CS`, `OLED_SDA`, etc.) – update if your wiring differs.
5. **Select the correct COM port** and ESP32 board (e.g., “Node32s”, “LOLIN D32”, or generic “ESP32 Dev Module”).
6. **Upload** the sketch.

On first boot, the device will:
- Read its MAC address and generate a unique 32‑bit ID (shown on OLED and serial monitor).
- Initialize I2C, OLED, and the SX1262 radio.
- Start a WiFi access point named `G-TALK-XXXXXXXX` (where XXXXXXXX = your device ID). Password: `12345678`.
- The OLED shows “READY” and the security status (“SEC: OFF”).

## Usage

### Web Interface (WiFi AP)

1. Connect your phone/laptop to the WiFi AP `G-TALK-XXXXXXXX` (password `12345678`).
2. Open a browser and go to `http://192.168.4.1`.
3. You will see a simple control panel:
   - **Target ID**: Enter the recipient’s 8‑digit hex ID (e.g., `FFFFFFFF` for broadcast).
   - **Chat message**: Text to send (plain or encrypted).
   - Buttons:
     - `INVIA CHAT` – sends a message (automatically encrypted if a Kyber handshake has been completed with the target).
     - `RICHIESTA MEDICO` – sends an `MSG_SOS_MEDIC` emergency alert.
     - `HANDSHAKE KYBER` – initiates a Kyber‑512 key exchange with the target node.

### Serial Monitor Commands

Open the Serial Monitor at **115200 baud**. The following single‑character commands are available:

| Command | Action                                                              |
|---------|---------------------------------------------------------------------|
| `h`     | Send a plain text “Hello non‑Secure World!” as broadcast chat.     |
| `k`     | Start Kyber‑512 handshake with broadcast address (`0xFFFFFFFF`).    |
| `s`     | Send an encrypted test message (only works if `is_secure_ready`).   |

### Mesh Operation

- **Heartbeat**: Every 60 seconds the device broadcasts a `MSG_HEARTBEAT`. Other nodes with TTL > 0 will re‑broadcast it once (anti‑loop cache prevents infinite forwarding).
- **Unicast messages**: When sending to a specific target ID, the recipient replies with an `ACK` or `NACK`. If no ACK is received within 120 seconds, the sender’s reassembly buffer times out and sends a NACK.
- **Fragmentation**: Messages larger than 200 bytes are split into multiple frames. The receiver reassembles them using the `sessionID` and `frameIdx` fields.
- **TTL**: The `ttl` field is decremented each hop. When it reaches 0, the packet is not forwarded.

### OLED Display

The display shows:
- Line 0: Device ID (hex)
- Line 1: Security status (`KYBER+GCM` or `OFF`)
- Line 2: Current status (e.g., `READY`, `SLEEPING`, `RX DATA`)
- Line 3: Signal strength and SNR (when a packet is received)
- Line 4: Last message info (e.g., “Msg ricevuto”)
- Line 5: Additional info (e.g., heartbeat relay)

After 10 minutes of inactivity, the device enters deep sleep. Press the BOOT button (GPIO0) or send a LoRa packet (DIO1) to wake it up.

## Message Protocol

### `GPacket` Structure (packed)

| Field         | Size (bytes) | Description                                                       |
|---------------|--------------|-------------------------------------------------------------------|
| `stx`         | 1            | Start delimiter (0x02)                                            |
| `senderID`    | 4            | 32‑bit unique node ID                                             |
| `targetID`    | 4            | 0xFFFFFFFF = broadcast, otherwise unicast                         |
| `sessionID`   | 2            | Incremented per new message (used for reassembly and ACK matching)|
| `msgType`     | 1            | See `GMessageType` enumeration                                    |
| `ttl`         | 1            | Time‑to‑live for mesh forwarding                                  |
| `frameIdx`    | 1            | Fragment index (0‑based)                                          |
| `totalFrames` | 1            | Total fragments for this session                                  |
| `payloadLen`  | 2            | Length of `payload` (max 200 bytes)                               |
| `timestamp`   | 4            | Unix‑style timestamp (seconds since boot)                         |
| `nonce`       | 4            | Simple incrementing counter                                       |
| `payload`     | 128/200*     | Actual data (encrypted or plain)                                  |
| `signature`   | 32           | Reserved for future authentication (currently unused)             |
| `etx`         | 1            | End delimiter (0x03)                                              |

> *The code defines `payload[128]` in `GPacket` but when sending fragments uses `payload[200]` for the actual transmission. This is a known inconsistency – ensure both sides use the same size. In the current code, `sendPacket()` copies up to 200 bytes, while `GPacket` is only 128+overhead. **We recommend aligning to 200 bytes** or reducing the max fragment size to 128.*

### Message Types (`GMessageType`)

| Type                       | Value | Description                               |
|----------------------------|-------|-------------------------------------------|
| `MSG_NET_TEST`             | 0x00  | Network connectivity test                 |
| `MSG_HEARTBEAT`            | 0x01  | Presence announcement (relayed)           |
| `MSG_SOS_GENERIC`          | 0x02  | Generic emergency                         |
| `MSG_SOS_MEDIC`            | 0x03  | Medical emergency                         |
| `MSG_CRIT_INJURY`          | 0x04  | Critical injury                           |
| `MSG_SUPPLY_LOW`           | 0x05  | Low supplies                              |
| `MSG_STATUS_OK`            | 0x06  | Status OK                                 |
| `MSG_FOUND`                | 0x07  | Lost person found                         |
| `MSG_ACK`                  | 0x08  | Acknowledgment (payload carries sessionID)|
| `MSG_SAT_QUERY`            | 0x09  | Query satellite position (future)         |
| `MSG_NODE_QUERY`           | 0x0A  | Query node status                         |
| `MSG_BATT_LOW`             | 0x0B  | Battery low                               |
| `MSG_WEATHER_BAD`          | 0x0C  | Bad weather alert                         |
| `MSG_RETURNING`            | 0x0D  | Returning to base                         |
| `MSG_HOW_ARE_YOU` / `OK`   | 0x0E  | Status inquiry / OK response              |
| `MSG_YES`                  | 0x0F  | Yes                                       |
| `MSG_NO`                   | 0x10  | No                                        |
| `MSG_DONT_KNOW`            | 0x11  | Don't know                                |
| `MSG_GENERIC_PROBLEM`      | 0x12  | Generic problem                           |
| `MSG_PATH_BLOCKED`         | 0x13  | Path blocked                              |
| `MSG_RECEIVED`             | 0x14  | Reception confirmed                        |
| `MSG_LOST`                 | 0x15  | Lost person alert                         |
| `MSG_NO_SIGNAL`            | 0x16  | No signal                                 |
| `MSG_NET_TEST_ALT`         | 0x17  | Alternate net test                        |
| `MSG_STATUS_FINE`          | 0x18  | All fine                                  |
| `MSG_IS_NODE`              | 0x19  | Node identification                       |
| `MSG_AUTH_NOTIFIED`        | 0x1A  | Authorisation notification                |
| `MSG_NACK`                 | 0x1B  | Negative acknowledgement                  |
| `MSG_CHAT`                 | 0x1C  | Free‑text chat message                    |
| `MSG_KYBER_PUBKEY`         | 0x20  | Kyber‑512 public key (800 bytes)          |
| `MSG_KYBER_CIPHERTEXT`     | 0x21  | Kyber‑512 ciphertext (768 bytes)          |

## Cryptography

G‑Mesh uses a hybrid post‑quantum cryptosystem:

### 1. Key Exchange – Kyber‑512 (ML‑KEM)

- **Public key size**: 800 bytes  
- **Secret key size**: 1632 bytes  
- **Ciphertext size**: 768 bytes  
- **Shared secret**: 32 bytes  

When a node wants to establish a secure channel with another node, it sends a `MSG_KYBER_PUBKEY` containing its public key. The recipient generates a ciphertext and derives the shared secret, then returns `MSG_KYBER_CIPHERTEXT`. The initiator decapsulates to obtain the same shared secret. After that, the boolean `is_secure_ready` is set to `true`.

### 2. Encryption – AES‑256‑GCM

- **Key**: the 32‑byte shared secret from Kyber  
- **IV**: 12 random bytes generated per message  
- **Authentication tag**: 16 bytes appended after the IV  

The encrypted payload structure inside the `GPacket.payload` is:
```
[ 12 bytes IV ][ 16 bytes tag ][ ciphertext ]
```
Total encrypted overhead = 28 bytes. The plaintext length is recovered after decryption.

> **Note**: ACK, NACK, and HEARTBEAT messages are never encrypted – they are sent in the clear to maintain mesh reliability.

## Sleep Mode & Power Management

- **Inactivity timeout**: 10 minutes (600000 ms). After that, `SleepMode()` is called.
- **Deep sleep configuration**:
  - Wake‑up sources:
    - **EXT0**: BOOT button (GPIO0, low level)
    - **EXT1**: LoRa DIO1 pin (rising edge)
    - **Timer**: 60 seconds (for periodic wake‑up, currently not used for logic)
  - Before sleep:
    - OLED display is turned off via SSD1306 command.
    - Web server is stopped, WiFi disconnected and turned off.
- **On wake‑up**:
  - The code checks the wake‑up cause and re‑initialises the radio, OLED, and web server.
  - If woken by LoRa (EXT1), it immediately processes any incoming packet.

## Code Structure

| File / Section          | Description                                                       |
|-------------------------|-------------------------------------------------------------------|
| `setup()`               | Hardware init (I2C, OLED, radio, WiFi AP, web server)            |
| `loop()`                | Checks inactivity, calls `checkIncomingLora()`, handles serial   |
| `checkIncomingLora()`   | Receives raw `GPacket`, validates STX/ETX, caches duplicates, calls `handlePacketReassembly()` |
| `handlePacketReassembly()` | Assembles multi‑frame messages, then `processFinalMessage()`   |
| `processFinalMessage()` | Decrypts (if secure), handles message type, calls `handleKeyExchange()` for crypto |
| `sendPacket()`          | Fragments large payloads and sends each frame via `sendToRadio()` |
| `encryptGCM()` / `decryptGCM()` | AES‑256‑GCM wrapper using mbedtls                       |
| `startKyberHandshake()` | Generates keypair, sends public key                            |
| `setupUI()`             | Creates WiFi AP and async web server with REST endpoint `/send` |
| `SleepMode()` / `WakeUpReason()` | Power management                                      |

## Troubleshooting

### I2C / OLED not working
- Run the I2C scanner (already present in `setup()`) – it prints found addresses to Serial. The code uses `0x3C` by default; if your display uses `0x3D`, change the `begin()` call.
- Check pull‑up resistors on SDA/SCL (typically 4.7 kΩ).
- Verify that `OLED_PWR` pin (GPIO36) is set LOW to enable power (or modify the code if you don’t use that transistor).

### LoRa initialisation fails
- The code prints a diagnostic error code from `radio.begin()`. Common issues:
  - Wrong SPI pins or CS pin.
  - BUSY pin not connected or logic level mismatch.
  - Missing `spi.begin()` call (it is called in `setupRadio()`).
- If the test transmission fails, double‑check antenna connection and frequency (868 MHz for EU, 915 MHz for US – change in `radio.begin()`).

### WiFi AP doesn’t appear
- Check that `WiFi.softAP()` succeeds. Serial monitor will print the IP (usually 192.168.4.1).
- Some ESP32 boards have weak RF – try keeping the device close to your phone.

### Kyber handshake fails / shared secret not set
- Ensure both nodes call `startKyberHandshake()` to the correct target ID. The handshake messages are large (up to 800 bytes) and may be fragmented; check that the `payload` array in `GPacket` is large enough (`KYBER_PK_SIZE` = 800). The current `GPacket.payload` is 128 bytes – **this is a bug**. You must increase `payload` size in the struct to at least 800 bytes or use a different method (e.g., send key in multiple fragments). The example code sends it via `sendPacket()` which already fragments automatically (200 byte chunks), so it will work as long as the `GPacket` definition is not used directly for storage of the whole key. The current implementation copies the key into `sendPacket()` which then copies into its own local `GPacket` with 200‑byte payload – this should be fine.

### “Dimensione GPacket: xxx” serial output shows small size
- The struct is packed and has a 128‑byte payload, which is insufficient for Kyber keys. **We strongly recommend changing `payload[128]` to `payload[200]`** (or larger) to match the fragment size and avoid accidental truncation.

## Extending & Contributing

Feel free to fork and enhance G‑Mesh. Potential improvements:
- **True mesh routing** (dynamic path selection)
- **End‑to‑end encryption signatures** using the `signature` field
- **GPS integration** for location reporting
- **Store‑and‑forward** for offline nodes
- **Web dashboard with received messages history**

When contributing, please maintain the same code style and add detailed comments for any new cryptographic or mesh logic.

## License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.  
*Copyright (c) 2026 Emanuele Ferraro & GeoReport International Technologies*

---

**Disclaimer**: This software is provided “as is”, without warranty of any kind. Use in critical life‑safety situations only after thorough testing. The cryptographic implementation has not been formally audited.
