# G-Mesh Base Firmware

**Professional Firmware Foundation for Mesh Networking**

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/georeport-international/g-mesh/firmware/base-firmware)
[![License](https://img.shields.io/badge/license-GPLV3-green.svg)](/LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-orange.svg)](https://www.espressif.com/en/products/socs/esp32)

---

## 📋 Table of Contents

- [Introduction](#introduction)
- [Architecture Overview](#architecture-overview)
- [Getting Started](#getting-started)
- [Configuration Guide](#configuration-guide)
  - [Basic Setup](#basic-setup)
  - [Network Configuration](#network-configuration)
  - [Security Configuration](#security-configuration)
  - [Power Management](#power-management)
  - [Display Configuration](#display-configuration)
  - [Identity Configuration](#identity-configuration)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Introduction

G-Mesh Base Firmware provides a robust foundation for developing mesh-enabled IoT devices. Whether you're building with **WiFi**, **Bluetooth**, **LoRa**, or any other wireless protocol, this firmware seamlessly integrates your hardware into the G-Mesh network ecosystem.

### Key Features

- ✅ **Multi-Protocol Support** – WiFi, Bluetooth, LoRa, and extensible architecture
- ✅ **Secure Communication** – Kyber post-quantum cryptography + AES-GCM encryption
- ✅ **Intelligent Routing** – Configurable flooding with TTL management
- ✅ **Power Management** – Deep sleep with multiple wake-up triggers
- ✅ **Web Interface** – Asynchronous web server for device management
- ✅ **Display Support** – OLED/LCD with configurable I2C parameters
- ✅ **Heartbeat System** – Network health monitoring and node discovery

---

## Architecture Overview

The firmware follows a layered architecture to ensure maintainability and flexibility:

```
┌─────────────────────────────────────────────────────────────┐
│                     User Application                        │
│          (setup, loop, custom callbacks)                    │
├─────────────────────────────────────────────────────────────┤
│                         GMLib                               │
│  • GMesh.begin(config)                                      │
│  • GMesh.sendMessage(target, type, data)                    │
│  • GMesh.onReceive(callback)                                │
│  • GMesh.onEvent(callback)                                  │
├─────────────────────────────────────────────────────────────┤
│            Mesh Core + Security Layer                       │
│  • Packet handling (framing, reassembly)                    │
│  • Routing / Flooding                                       │
│  • Kyber + AES-GCM Encryption                               │
│  • Buffer and Cache Management                              │
├─────────────────────────────────────────────────────────────┤
│                    Hardware Drivers                         │
│  • RadioDriver (SX1262, SX127x, ...)                        │
│  • DisplayDriver (OLED, LCD, None)                          │
│  • StorageDriver (EEPROM, SPIFFS)                           │
│  • InputDriver (buttons, interrupts)                        │
└─────────────────────────────────────────────────────────────┘
```

---

## Getting Started

### Prerequisites

- **ESP32** development board
- **Arduino IDE** or **PlatformIO**
- G-Mesh compatible radio module (LoRa, WiFi, Bluetooth)
- USB cable for programming

### Quick Start

```cpp
#include "GMesh.h"

void setup() {
    Serial.begin(115200);
    
    // Identity configuration (REQUIRED)
    GMesh.identity(0, "MyNode", 1);
    
    // Basic radio configuration
    GMesh.config(LORA, 868, 125, 9, 7, 22);
    GMesh.gpio(8, 14, 12, 13, 9, 11, 10);
    
    // Initialize the mesh
    GMesh.begin();
}

void loop() {
    GMesh.update();
    // Your application code here
}
```

---

## Configuration Guide

### Basic Setup

#### `GMesh.config()`
Configure the wireless protocol and radio parameters.

```cpp
GMesh.config(protocol, frequency, bandwidth, spreadingFactor, codingRate, txPower);
```

| Parameter | Description | Options |
|-----------|-------------|---------|
| `protocol` | Wireless protocol | `WIFI`, `BLUETOOTH`, `LORA` |
| `frequency` | Operating frequency (MHz) | e.g., 868, 915, 1500 |
| `bandwidth` | Bandwidth (kHz) | e.g., 125, 250, 500 |
| `spreadingFactor` | Spreading factor (LoRa) | 7–12 (higher = more range, lower speed) |
| `codingRate` | Coding rate | `7` = 4/5, `8` = 4/8, `0` = dynamic |
| `txPower` | Transmission power (dBm) | 2–22 |

**Example:**
```cpp
GMesh.config(LORA, 868, 125, 9, 7, 22);
```

---

#### `GMesh.gpio()`
Configure GPIO pins for radio module.

```cpp
GMesh.gpio(csPin, dio1Pin, rstPin, busyPin, sckPin, misoPin, mosiPin);
```

**Default values if not called:**
```cpp
// Default GPIO configuration
int csPin = 8;
int dio1Pin = 14;
int rstPin = 12;
int busyPin = 13;
int sckPin = 9;
int misoPin = 11;
int mosiPin = 10;
```

---

#### `GMesh.interfaceconfig()`
Configure the asynchronous web server interface.

```cpp
GMesh.interfaceconfig(enable, ssid, password);
```

| Parameter | Description |
|-----------|-------------|
| `enable` | `true` = enable web server, `false` = disable |
| `ssid` | WiFi network SSID |
| `password` | WiFi network password |

**Note:** The web interface can be toggled at runtime using:
```cpp
GMesh.enable(webinterface);
GMesh.disable(webinterface);
```

---

#### `GMesh.loraconfig()`
Configure LoRa-specific hardware settings.

```cpp
GMesh.loraconfig(model, useInternalAntenna);
```

| Parameter | Description |
|-----------|-------------|
| `model` | LoRa antenna model identifier |
| `useInternalAntenna` | `true` = internal switch, `false` = external |

---

### Network Configuration

#### `GMesh.netconfig()`
Configure mesh networking parameters.

```cpp
GMesh.netconfig(TTL, HBI, ackTimeoutMs, rxAssemblyTimeout, maxCacheSize, enableFlooding, requireAck);
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `TTL` | Maximum hops for packet propagation | 3 |
| `HBI` | Heartbeat interval (ms) | 60000 |
| `ackTimeoutMs` | ACK wait timeout (ms) | 5000 |
| `rxAssemblyTimeout` | Packet reassembly timeout (ms) | 120000 |
| `maxCacheSize` | Anti-duplicate cache size | 10 |
| `enableFlooding` | `true` = forward packets, `false` = leaf node | `true` |
| `requireAck` | `true` = mandatory ACK, `false` = optional | `false` |

**Default configuration (auto-applied if not called):**
```cpp
uint8_t defaultTTL = 3;
uint16_t heartbeatInterval = 60000;
uint32_t ackTimeoutMs = 5000;
uint32_t rxAssemblyTimeout = 120000;
uint8_t maxCacheSize = 10;
bool enableFlooding = true;
bool requireAck = false;
```

---

### Security Configuration

#### `GMesh.securityconfig()`
Configure encryption and cryptographic parameters.

```cpp
GMesh.securityconfig(enableEncryption, enableKyber, kyberVariant, handshakeTimeoutMs);
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `enableEncryption` | Enable encryption | `true` |
| `enableKyber` | `0` = Kyber, `1` = AES | `0` |
| `kyberVariant` | Kyber key size | `512` |
| `handshakeTimeoutMs` | Handshake timeout (ms) | 30000 |

**Note:** Security can be toggled at runtime:
```cpp
GMesh.enable(encryption);
GMesh.disable(encryption);
```

---

### Power Management

#### `GMesh.powerconfig()`
Configure deep sleep and wake-up parameters.

```cpp
GMesh.powerconfig(enableDeepSleep, deepSleepTimeoutMs, wakeButtonPin, wakeRadioPin, wakeOnRadio, wakeOnTimer);
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `enableDeepSleep` | Enable deep sleep | `false` |
| `deepSleepTimeoutMs` | Inactivity timeout (ms) | – |
| `wakeButtonPin` | GPIO for wake button | 0 (BOOT) |
| `wakeRadioPin` | GPIO for radio wake | 14 (DIO1) |
| `wakeOnRadio` | Wake on radio activity | `true` |
| `wakeOnTimer` | Wake on timer | `true` |

**Default values:**
```cpp
int wakeButtonPin = 0;
int wakeRadioPin = 14;
bool wakeOnRadio = true;
bool wakeOnTimer = true;
```

---

### Display Configuration

#### `GMesh.displayconfig()`
Configure optional OLED/LCD display.

```cpp
GMesh.displayconfig(enabled, i2cAddress, sdaPin, sclPin, rstPin, powerPin, width, height);
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `enabled` | Enable display | `false` |
| `i2cAddress` | I2C address | 0x3C |
| `sdaPin` | SDA pin | – |
| `sclPin` | SCL pin | – |
| `rstPin` | Reset pin | – |
| `powerPin` | Power pin | – |
| `width` | Display width (pixels) | – |
| `height` | Display height (pixels) | – |

**Note:** Display can be toggled at runtime:
```cpp
GMesh.enable(display);
GMesh.disable(display);
```

---

### Identity Configuration

#### `GMesh.identity()` ⚠️ **REQUIRED**

```cpp
GMesh.identity(nodeID, nodeName, meshVersion);
```

| Parameter | Description |
|-----------|-------------|
| `nodeID` | `0` = auto-generate from ESP32 MAC, or custom 6-digit ID |
| `nodeName` | Human-readable node name (visible to other devices) |
| `meshVersion` | Mesh protocol version (use `1` for current) |

**Example:**
```cpp
GMesh.identity(0, "SensorNode_01", 1);
```

---

## API Reference

### Initialization

| Function | Description |
|----------|-------------|
| `GMesh.begin()` | Initialize all configured modules and start the mesh network |

### Messaging

| Function | Description |
|----------|-------------|
| `GMesh.sendMessage(target, type, data)` | Send a message to a specific node |
| `GMesh.broadcastMessage(type, data)` | Broadcast message to all nodes |
| `GMesh.onReceive(callback)` | Register callback for incoming messages |
| `GMesh.onEvent(callback)` | Register callback for system events |

### Runtime Control

| Function | Description |
|----------|-------------|
| `GMesh.enable(feature)` | Enable a feature (webinterface, encryption, display) |
| `GMesh.disable(feature)` | Disable a feature |
| `GMesh.update()` | Process network events (call in `loop()`) |
| `GMesh.getNodeID()` | Get current node ID |
| `GMesh.getNodeName()` | Get current node name |
| `GMesh.getNetworkStats()` | Get network statistics |

---

## Examples

### Basic Mesh Node

```cpp
#include "GMesh.h"

void setup() {
    Serial.begin(115200);
    
    // Identity
    GMesh.identity(123456, "WeatherStation", 1);
    
    // Radio (LoRa)
    GMesh.config(LORA, 868, 125, 9, 7, 22);
    GMesh.gpio(8, 14, 12, 13, 9, 11, 10);
    
    // Network
    GMesh.netconfig(3, 60000, 5000, 120000, 10, true, false);
    
    // Initialize
    GMesh.begin();
    
    Serial.println("G-Mesh Node Started!");
}

void loop() {
    GMesh.update();
    // Add sensor reading and transmission logic here
}
```

### Message Handler

```cpp
void onMessageReceived(uint32_t senderID, uint8_t type, const uint8_t* data, size_t len) {
    Serial.printf("Message from %d, type: %d, len: %d\n", senderID, type, len);
    
    if (type == 1) { // Custom message type
        // Process data
    }
}

void setup() {
    // ... configuration ...
    GMesh.onReceive(onMessageReceived);
    GMesh.begin();
}
```

---

## Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| Node not connecting | Verify radio parameters (frequency, bandwidth) match other nodes |
| Poor range | Increase spreading factor or transmission power |
| Packet loss | Enable ACK with `requireAck = true` |
| Web interface not accessible | Check WiFi credentials and enable web interface |
| Deep sleep not working | Verify wake pins are correctly configured |

### Debugging

Enable serial debug output:
```cpp
Serial.begin(115200);
GMesh.setDebugLevel(DEBUG_VERBOSE);
```

---

## Contributing

We welcome contributions! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Development Guidelines

- Follow C++ coding standards
- Add comments for complex logic
- Update documentation for new features
- Test on ESP32 hardware before submitting

---

## License

This project is licensed under the General Public License V3 License - see the [LICENSE](/LICENSE) file for details.

---

**Copyright (C) Semont Labs - semontlabs.com, Emanuele Ferraro, G-Mesh - g-mesh.org**
