#ifndef GMESH_H
#define GMESH_H

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <mbedtls/gcm.h>
#include <pqc_kyber.h>

// Costanti per le feature (enable/disable)
#define FEATURE_WEB          0x01
#define FEATURE_ENCRYPTION   0x02
#define FEATURE_DISPLAY      0x04

// Protocolli supportati (per ora solo LORA)
#define PROTOCOL_LORA        1
#define PROTOCOL_WIFI        2
#define PROTOCOL_BLUETOOTH   3

// Dimensioni Kyber‑512
#define KYBER_PK_SIZE  800
#define KYBER_SK_SIZE  1632
#define KYBER_CT_SIZE  768

// Valori di default
#define DEFAULT_TTL          3
#define DEFAULT_HBI          60000
#define DEFAULT_ACK_TIMEOUT  5000
#define DEFAULT_RX_TIMEOUT   120000
#define DEFAULT_CACHE_SIZE   10
#define DEFAULT_OLED_ADDR    0x3C
#define DEFAULT_SCREEN_W     128
#define DEFAULT_SCREEN_H     64

// Tipi di messaggio (enum)
enum GMessageType : uint8_t {
    MSG_NET_TEST          = 0x00,
    MSG_HEARTBEAT         = 0x01,
    MSG_ACK               = 0x08,
    MSG_SAT_QUERY         = 0x09,
    MSG_NODE_QUERY        = 0x0A,
    MSG_BATT_LOW          = 0x0B,
    MSG_NET_TEST_ALT      = 0x17,
    MSG_NO_SIGNAL         = 0x16,
    MSG_IS_NODE           = 0x19,
    MSG_NACK              = 0x1B,
    MSG_SOS_GENERIC       = 0x02,
    MSG_SOS_MEDIC         = 0x03,
    MSG_CRIT_INJURY       = 0x04,
    MSG_LOST              = 0x15,
    MSG_GENERIC_PROBLEM   = 0x12,
    MSG_PATH_BLOCKED      = 0x13,
    MSG_SUPPLY_LOW        = 0x05,
    MSG_STATUS_OK         = 0x06,
    MSG_FOUND             = 0x07,
    MSG_WEATHER_BAD       = 0x0C,
    MSG_RETURNING         = 0x0D,
    MSG_STATUS_FINE       = 0x18,
    MSG_AUTH_NOTIFIED     = 0x1A,
    MSG_HOW_ARE_YOU       = 0x0E,
    MSG_OK                = 0x0E,
    MSG_YES               = 0x0F,
    MSG_NO                = 0x10,
    MSG_DONT_KNOW         = 0x11,
    MSG_RECEIVED          = 0x14,
    MSG_CHAT              = 0x1C,
    MSG_KYBER_PUBKEY      = 0x20,
    MSG_KYBER_CIPHERTEXT  = 0x21
};

// Struttura pacchetto (come nell'originale)
struct __attribute__((packed)) GPacket {
    uint8_t  stx = 0x02;
    uint32_t senderID;
    uint32_t targetID;
    uint16_t sessionID;
    uint8_t  msgType;
    uint8_t  ttl;
    uint8_t  frameIdx;
    uint8_t  totalFrames;
    uint16_t payloadLen;
    uint32_t timestamp;
    uint32_t nonce;
    uint8_t  payload[128];
    uint8_t  signature[32];
    uint8_t  etx = 0x03;
};

// Struttura per riassemblaggio pacchetti
struct IncomingMessage {
    uint32_t senderID;
    uint32_t targetID;
    uint16_t sessionID;
    uint8_t  receivedFrames;
    uint8_t  totalFrames;
    uint16_t actualPayloadLen;
    uint8_t  fullData[1000];
    uint32_t lastUpdate;
};

// Classe principale GMesh
class GMesh {
public:
    // Costruttore / distruttore
    GMesh();
    ~GMesh();

    // --- Funzioni di configurazione ---
    void config(int protocol, float freq, float bw, int sf, int cr, int txPower);
    void gpio(int cs, int dio1, int rst, int busy, int sck, int miso, int mosi);
    void interfaceconfig(bool enable, const char* ssid, const char* password);
    void loraconfig(int model, bool internalAntenna);
    void netconfig(uint8_t ttl, uint16_t hbInterval, uint32_t ackTimeout, uint32_t rxAssemblyTimeout,
                   uint8_t cacheSize, bool enableFlooding, bool requireAck);
    void securityconfig(bool enableEncryption, bool enableKyber, int kyberVariant, uint32_t handshakeTimeout);
    void powerconfig(bool enableDeepSleep, uint32_t deepSleepTimeout, int wakeButtonPin, int wakeRadioPin,
                     bool wakeOnRadio, bool wakeOnTimer);
    void displayconfig(bool enabled, uint8_t i2cAddress, int sdaPin, int sclPin, int rstPin, int powerPin,
                       int width, int height);
    void identity(uint32_t nodeID, const char* nodeName, uint8_t meshVersion);

    // --- Ciclo di vita ---
    bool begin();
    void update();

    // --- Messaggistica ---
    bool sendMessage(uint32_t target, uint8_t type, const uint8_t* data, size_t len);
    bool broadcastMessage(uint8_t type, const uint8_t* data, size_t len);

    // --- Callback ---
    void onReceive(void (*callback)(uint32_t sender, uint8_t type, const uint8_t* data, size_t len));
    void onEvent(void (*callback)(int event, void* info));

    // --- Controllo runtime ---
    void enable(int feature);
    void disable(int feature);

    // --- Getter ---
    uint32_t getNodeID() const;
    const char* getNodeName() const;
    bool isSecure() const;

private:
    // ---- Variabili di configurazione ----
    int _protocol;
    float _frequency, _bandwidth;
    int _spreadingFactor, _codingRate, _txPower;
    int _csPin, _dio1Pin, _rstPin, _busyPin, _sckPin, _misoPin, _mosiPin;
    bool _webEnabled;
    char _apSSID[32], _apPassword[32];
    int _loraModel;
    bool _internalAntenna;
    uint8_t _ttl;
    uint16_t _heartbeatInterval;
    uint32_t _ackTimeoutMs, _rxAssemblyTimeout;
    uint8_t _cacheSize;
    bool _floodingEnabled, _requireAck;
    bool _encryptionEnabled, _kyberEnabled;
    int _kyberVariant;
    uint32_t _handshakeTimeoutMs;
    bool _deepSleepEnabled;
    uint32_t _deepSleepTimeout;
    int _wakeButtonPin, _wakeRadioPin;
    bool _wakeOnRadio, _wakeOnTimer;
    bool _displayEnabled;
    uint8_t _displayI2CAddr;
    int _displaySDA, _displaySCL, _displayRST, _displayPower;
    int _displayWidth, _displayHeight;
    uint32_t _nodeID;
    char _nodeName[16];
    uint8_t _meshVersion;

    // ---- Stato runtime ----
    SX1262* _radio;
    Adafruit_SSD1306* _oled;
    AsyncWebServer* _server;
    bool _initialized;
    bool _secureReady;
    uint16_t _currentSessionID;
    uint32_t _currentNonce;
    uint8_t _sharedSecret[32];
    uint8_t _myPrivateKey[KYBER_SK_SIZE];
    uint8_t* _msgCache;
    uint8_t _cacheIndex;
    IncomingMessage _rxBuffer[5];
    GPacket _tempPacket;
    unsigned long _lastActivity;
    unsigned long _lastHeartbeat;
    float _lastRSSI, _lastSNR;

    // ---- Callback ----
    void (*_receiveCallback)(uint32_t, uint8_t, const uint8_t*, size_t);
    void (*_eventCallback)(int, void*);

    // ---- Metodi privati ----
    void updateUI(String status, String info, float rssi = 0, float snr = 0);
    bool isMsgSeen(uint32_t sender, uint16_t session);
    void encryptGCM(uint8_t* plaintext, uint16_t len, uint8_t* outputPayload);
    bool decryptGCM(uint8_t* inputPayload, uint16_t cipherLen, uint8_t* outputPlaintext);
    GPacket preparePacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len, uint8_t ttlValue = DEFAULT_TTL);
    void sendToRadio(GPacket &p);
    void sendPacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len);
    void sendAckNack(uint32_t target, uint16_t originalSessionID, bool isSuccess);
    void startKyberHandshake(uint32_t target);
    void handleKeyExchange(uint8_t type, uint8_t* fullData, uint32_t remoteID);
    void processFinalMessage(uint8_t type, uint8_t* data, uint16_t len, uint32_t sender);
    void handlePacketReassembly(GPacket &p);
    void checkRxBufferTimeout();
    void checkIncomingLora();
    void goToSleep();
    void wakeUpReason();
    void setupRadio();
    void setupUI();
    void processIncoming(const GPacket& p);
};

#endif // GMESH_H
