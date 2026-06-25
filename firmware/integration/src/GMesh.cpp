#include "GMesh.h"

// Costruttore: inizializza i valori di default
GMesh::GMesh()
    : _protocol(PROTOCOL_LORA),
      _frequency(868.0),
      _bandwidth(125.0),
      _spreadingFactor(9),
      _codingRate(7),
      _txPower(10),
      _csPin(8), _dio1Pin(14), _rstPin(12), _busyPin(13),
      _sckPin(9), _misoPin(11), _mosiPin(10),
      _webEnabled(true),
      _loraModel(0), _internalAntenna(false),
      _ttl(DEFAULT_TTL),
      _heartbeatInterval(DEFAULT_HBI),
      _ackTimeoutMs(DEFAULT_ACK_TIMEOUT),
      _rxAssemblyTimeout(DEFAULT_RX_TIMEOUT),
      _cacheSize(DEFAULT_CACHE_SIZE),
      _floodingEnabled(true),
      _requireAck(false),
      _encryptionEnabled(true),
      _kyberEnabled(true),
      _kyberVariant(512),
      _handshakeTimeoutMs(30000),
      _deepSleepEnabled(false),
      _deepSleepTimeout(600000),
      _wakeButtonPin(0),
      _wakeRadioPin(14),
      _wakeOnRadio(true),
      _wakeOnTimer(true),
      _displayEnabled(false),
      _displayI2CAddr(DEFAULT_OLED_ADDR),
      _displaySDA(17), _displaySCL(18), _displayRST(21), _displayPower(36),
      _displayWidth(DEFAULT_SCREEN_W), _displayHeight(DEFAULT_SCREEN_H),
      _nodeID(0), _meshVersion(1),
      _radio(nullptr), _oled(nullptr), _server(nullptr),
      _initialized(false), _secureReady(false),
      _currentSessionID(0), _currentNonce(0),
      _cacheIndex(0), _lastActivity(0), _lastHeartbeat(0),
      _receiveCallback(nullptr), _eventCallback(nullptr)
{
    strcpy(_nodeName, "G-Mesh");
    strcpy(_apSSID, "G-MESH");
    strcpy(_apPassword, "12345678");
    _msgCache = new uint8_t[_cacheSize];
    memset(_msgCache, 0, _cacheSize);
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
}

GMesh::~GMesh() {
    delete[] _msgCache;
    if (_radio) delete _radio;
    if (_oled) delete _oled;
    if (_server) delete _server;
}

// --- Funzioni di configurazione ---
void GMesh::config(int protocol, float freq, float bw, int sf, int cr, int txPower) {
    _protocol = protocol;
    _frequency = freq;
    _bandwidth = bw;
    _spreadingFactor = sf;
    _codingRate = cr;
    _txPower = txPower;
}

void GMesh::gpio(int cs, int dio1, int rst, int busy, int sck, int miso, int mosi) {
    _csPin = cs; _dio1Pin = dio1; _rstPin = rst; _busyPin = busy;
    _sckPin = sck; _misoPin = miso; _mosiPin = mosi;
}

void GMesh::interfaceconfig(bool enable, const char* ssid, const char* password) {
    _webEnabled = enable;
    if (ssid) strncpy(_apSSID, ssid, sizeof(_apSSID)-1);
    if (password) strncpy(_apPassword, password, sizeof(_apPassword)-1);
}

void GMesh::loraconfig(int model, bool internalAntenna) {
    _loraModel = model;
    _internalAntenna = internalAntenna;
}

void GMesh::netconfig(uint8_t ttl, uint16_t hbInterval, uint32_t ackTimeout, uint32_t rxAssemblyTimeout,
                      uint8_t cacheSize, bool enableFlooding, bool requireAck) {
    _ttl = ttl;
    _heartbeatInterval = hbInterval;
    _ackTimeoutMs = ackTimeout;
    _rxAssemblyTimeout = rxAssemblyTimeout;
    _cacheSize = cacheSize;
    _floodingEnabled = enableFlooding;
    _requireAck = requireAck;
    // Ricrea la cache se la dimensione cambia
    delete[] _msgCache;
    _msgCache = new uint8_t[_cacheSize];
    memset(_msgCache, 0, _cacheSize);
    _cacheIndex = 0;
}

void GMesh::securityconfig(bool enableEncryption, bool enableKyber, int kyberVariant, uint32_t handshakeTimeout) {
    _encryptionEnabled = enableEncryption;
    _kyberEnabled = enableKyber;
    _kyberVariant = kyberVariant;
    _handshakeTimeoutMs = handshakeTimeout;
}

void GMesh::powerconfig(bool enableDeepSleep, uint32_t deepSleepTimeout, int wakeButtonPin, int wakeRadioPin,
                        bool wakeOnRadio, bool wakeOnTimer) {
    _deepSleepEnabled = enableDeepSleep;
    _deepSleepTimeout = deepSleepTimeout;
    _wakeButtonPin = wakeButtonPin;
    _wakeRadioPin = wakeRadioPin;
    _wakeOnRadio = wakeOnRadio;
    _wakeOnTimer = wakeOnTimer;
}

void GMesh::displayconfig(bool enabled, uint8_t i2cAddress, int sdaPin, int sclPin, int rstPin, int powerPin,
                          int width, int height) {
    _displayEnabled = enabled;
    _displayI2CAddr = i2cAddress;
    _displaySDA = sdaPin;
    _displaySCL = sclPin;
    _displayRST = rstPin;
    _displayPower = powerPin;
    _displayWidth = width;
    _displayHeight = height;
}

void GMesh::identity(uint32_t nodeID, const char* nodeName, uint8_t meshVersion) {
    if (nodeID == 0) {
        // Genera da MAC
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        _nodeID = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    } else {
        _nodeID = nodeID;
    }
    if (nodeName) strncpy(_nodeName, nodeName, sizeof(_nodeName)-1);
    _meshVersion = meshVersion;
}

// --- Inizializzazione ---
bool GMesh::begin() {
    if (_initialized) return true;

    Serial.begin(115200);
    delay(400);
    Serial.println("=== G-Mesh Node Starting ===");

    // Setup display
    if (_displayEnabled) {
        pinMode(_displayPower, OUTPUT);
        digitalWrite(_displayPower, LOW);
        delay(500);
        pinMode(_displayRST, OUTPUT);
        digitalWrite(_displayRST, LOW);
        delay(100);
        digitalWrite(_displayRST, HIGH);
        delay(200);
        Wire.begin(_displaySDA, _displaySCL);
        Wire.setClock(100000);
        _oled = new Adafruit_SSD1306(_displayWidth, _displayHeight, &Wire, _displayRST);
        if (!_oled->begin(SSD1306_SWITCHCAPVCC, _displayI2CAddr, false, false)) {
            Serial.println("OLED init failed!");
            delete _oled;
            _oled = nullptr;
        } else {
            _oled->clearDisplay();
            _oled->setTextSize(1);
            _oled->setTextColor(SSD1306_WHITE);
            _oled->setCursor(0,0);
            _oled->println("G-Mesh Starting...");
            _oled->display();
        }
    }

    // Setup radio
    setupRadio();
    if (!_radio) {
        Serial.println("Radio init failed!");
        return false;
    }

    // Identità
    if (_nodeID == 0) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        _nodeID = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    }

    // Web server
    if (_webEnabled) {
        setupUI();
    }

    updateUI("READY", "Online");
    _initialized = true;
    _lastActivity = millis();
    _lastHeartbeat = millis();
    return true;
}

// --- Loop ---
void GMesh::update() {
    if (!_initialized) return;

    // Sleep timeout
    if (_deepSleepEnabled && (millis() - _lastActivity > _deepSleepTimeout)) {
        goToSleep();
        return;
    }

    // Ricezione LoRa
    checkIncomingLora();

    // Timeout buffer
    checkRxBufferTimeout();

    // Heartbeat periodico
    if (millis() - _lastHeartbeat > _heartbeatInterval) {
        GPacket p = preparePacket(MSG_HEARTBEAT, 0xFFFFFFFF, nullptr, 0);
        sendToRadio(p);
        _lastHeartbeat = millis();
    }

    // Comandi seriali (debug)
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'h') {
            String testMsg = "Hello non-Secure World!";
            sendPacket(MSG_CHAT, 0xFFFFFFFF, (uint8_t*)testMsg.c_str(), testMsg.length());
        }
        if (c == 'k') startKyberHandshake(0xFFFFFFFF);
        if (c == 's') {
            if (_secureReady) {
                uint8_t securePayload[228];
                String testMsg = "Hello Secure World!";
                encryptGCM((uint8_t*)testMsg.c_str(), testMsg.length(), securePayload);
                sendPacket(MSG_CHAT, 0xFFFFFFFF, securePayload, testMsg.length() + 28);
            } else {
                Serial.println("Sessione non sicura, impossibile inviare test cifrato.");
            }
        }
    }

    delay(10);
}

// --- Messaggistica ---
bool GMesh::sendMessage(uint32_t target, uint8_t type, const uint8_t* data, size_t len) {
    if (!_initialized) return false;
    sendPacket((GMessageType)type, target, (uint8_t*)data, len);
    return true;
}

bool GMesh::broadcastMessage(uint8_t type, const uint8_t* data, size_t len) {
    return sendMessage(0xFFFFFFFF, type, data, len);
}

// --- Callback ---
void GMesh::onReceive(void (*callback)(uint32_t, uint8_t, const uint8_t*, size_t)) {
    _receiveCallback = callback;
}

void GMesh::onEvent(void (*callback)(int, void*)) {
    _eventCallback = callback;
}

// --- Controllo runtime ---
void GMesh::enable(int feature) {
    if (feature & FEATURE_WEB) {
        if (!_server) setupUI();
        else {
            // eventualmente riavvia server
        }
    }
    if (feature & FEATURE_ENCRYPTION) {
        _encryptionEnabled = true;
    }
    if (feature & FEATURE_DISPLAY) {
        _displayEnabled = true;
        // riavvia display se necessario
    }
}

void GMesh::disable(int feature) {
    if (feature & FEATURE_WEB) {
        if (_server) {
            _server->end();
            delete _server;
            _server = nullptr;
        }
    }
    if (feature & FEATURE_ENCRYPTION) {
        _encryptionEnabled = false;
    }
    if (feature & FEATURE_DISPLAY) {
        _displayEnabled = false;
        if (_oled) {
            _oled->ssd1306_command(SSD1306_DISPLAYOFF);
        }
    }
}

// --- Getter ---
uint32_t GMesh::getNodeID() const { return _nodeID; }
const char* GMesh::getNodeName() const { return _nodeName; }
bool GMesh::isSecure() const { return _secureReady; }

// ------------------------------------------------------------------
//   IMPLEMENTAZIONE DEI METODI PRIVATI (copiati/adattati dall'originale)
// ------------------------------------------------------------------

void GMesh::updateUI(String status, String info, float rssi, float snr) {
    if (!_oled || !_displayEnabled) return;
    _oled->clearDisplay();
    _oled->setTextSize(1);
    _oled->setTextColor(SSD1306_WHITE);
    _oled->setCursor(0,0);
    _oled->printf("ID: %08X", _nodeID);
    _oled->setCursor(0,10);
    _oled->print("Sec: ");
    _oled->print(_secureReady ? "KYBER+GCM" : "OFF");
    _oled->setCursor(0,25);
    _oled->print("Stato: "); _oled->print(status);
    if (rssi != 0) {
        _oled->setCursor(0,35);
        _oled->printf("Sig: %.1f dBm | SNR: %.1f", rssi, snr);
    }
    _oled->setCursor(0,45);
    _oled->print("Ultimo Msg:");
    _oled->setCursor(0,55);
    _oled->print(info);
    _oled->display();
}

bool GMesh::isMsgSeen(uint32_t sender, uint16_t session) {
    uint32_t hash = sender ^ session;
    for (uint8_t i=0; i<_cacheSize; i++) {
        if (_msgCache[i] == hash) return true;
    }
    _msgCache[_cacheIndex] = hash;
    _cacheIndex = (_cacheIndex + 1) % _cacheSize;
    return false;
}

void GMesh::encryptGCM(uint8_t* plaintext, uint16_t len, uint8_t* outputPayload) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _sharedSecret, 256);
    uint8_t iv[12];
    for (int i=0; i<12; i++) iv[i] = (uint8_t)esp_random();
    memcpy(outputPayload, iv, 12);
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, iv, 12, NULL, 0, plaintext, outputPayload + 28, 16, outputPayload + 12);
    mbedtls_gcm_free(&gcm);
}

bool GMesh::decryptGCM(uint8_t* inputPayload, uint16_t cipherLen, uint8_t* outputPlaintext) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _sharedSecret, 256);
    uint8_t iv[12], tag[16];
    memcpy(iv, inputPayload, 12);
    memcpy(tag, inputPayload + 12, 16);
    int ret = mbedtls_gcm_auth_decrypt(&gcm, cipherLen, iv, 12, NULL, 0, tag, 16, inputPayload + 28, outputPlaintext);
    mbedtls_gcm_free(&gcm);
    return (ret == 0);
}

GPacket GMesh::preparePacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len, uint8_t ttlValue) {
    GPacket p;
    p.senderID = _nodeID;
    p.targetID = target;
    p.sessionID = ++_currentSessionID;
    p.msgType = (uint8_t)type;
    p.ttl = ttlValue;
    p.frameIdx = 0;
    p.totalFrames = 1;
    p.timestamp = millis() / 1000;
    p.nonce = _currentNonce++;
    p.payloadLen = (len > 128) ? 128 : len;
    memset(p.payload, 0, 128);
    if (data) memcpy(p.payload, data, p.payloadLen);
    return p;
}

void GMesh::sendToRadio(GPacket &p) {
    if (!_radio) return;
    Serial.printf("Invio MSG Tipo: 0x%02X\n", p.msgType);
    while(digitalRead(_busyPin) == HIGH) delay(1);
    _radio->standby();
    delay(50);
    int state = _radio->transmit((uint8_t*)&p, sizeof(GPacket));
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("TX OK!");
    } else {
        Serial.printf("Errore TX! Codice: %d\n", state);
    }
    delay(50);
    _radio->startReceive();
}

void GMesh::sendPacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len) {
    uint8_t totalFrames = (len == 0) ? 1 : (len / 128) + ((len % 128) != 0 ? 1 : 0);
    _currentSessionID++;
    for (uint8_t i=0; i<totalFrames; i++) {
        GPacket p;
        p.senderID = _nodeID;
        p.targetID = target;
        p.sessionID = _currentSessionID;
        p.msgType = (uint8_t)type;
        p.ttl = _ttl;
        p.frameIdx = i;
        p.totalFrames = totalFrames;
        p.timestamp = millis() / 1000;
        p.nonce = _currentNonce++;
        uint16_t offset = i * 128;
        p.payloadLen = (len - offset > 128) ? 128 : (len - offset);
        memset(p.payload, 0, 128);
        if (data) memcpy(p.payload, data + offset, p.payloadLen);
        sendToRadio(p);
        if (totalFrames > 1) delay(200);
    }
}

void GMesh::sendAckNack(uint32_t target, uint16_t originalSessionID, bool isSuccess) {
    if (target == 0xFFFFFFFF) return;
    uint8_t payload[2];
    payload[0] = (originalSessionID >> 8) & 0xFF;
    payload[1] = originalSessionID & 0xFF;
    GPacket p = preparePacket(isSuccess ? MSG_ACK : MSG_NACK, target, payload, 2);
    sendToRadio(p);
}

void GMesh::startKyberHandshake(uint32_t target) {
    updateUI("KYBER", "Generazione Chiavi...");
    uint8_t pk[KYBER_PK_SIZE];
    pqc_kyber512_keypair(pk, _myPrivateKey);
    sendPacket(MSG_KYBER_PUBKEY, target, pk, KYBER_PK_SIZE);
    updateUI("KYBER", "PK Inviata, attesa CT");
}

void GMesh::handleKeyExchange(uint8_t type, uint8_t* fullData, uint32_t remoteID) {
    if (type == MSG_KYBER_PUBKEY) {
        uint8_t ct[KYBER_CT_SIZE];
        pqc_kyber512_encapsulate(ct, _sharedSecret, fullData);
        sendPacket(MSG_KYBER_CIPHERTEXT, remoteID, ct, KYBER_CT_SIZE);
        _secureReady = true;
        updateUI("SECURE", "Sessione Criptata OK");
    } else if (type == MSG_KYBER_CIPHERTEXT) {
        pqc_kyber512_decapsulate(_sharedSecret, fullData, _myPrivateKey);
        _secureReady = true;
        updateUI("SECURE", "Handshake Completato");
    }
}

void GMesh::processFinalMessage(uint8_t type, uint8_t* data, uint16_t len, uint32_t sender) {
    // Gestione scambio chiavi
    if (type == MSG_KYBER_PUBKEY || type == MSG_KYBER_CIPHERTEXT) {
        handleKeyExchange(type, data, sender);
        return;
    }

    uint8_t plain[1000];
    uint16_t plainLen = len;

    // Decrittazione se abilitata
    if (_encryptionEnabled && _secureReady && type != MSG_ACK && type != MSG_NACK && type != MSG_HEARTBEAT && len >= 28) {
        if (!decryptGCM(data, len - 28, plain)) {
            Serial.println("Errore Decrittazione/Auth GCM!");
            return;
        }
        plainLen = len - 28;
    } else {
        memcpy(plain, data, len);
    }

    // Chiamata callback utente
    if (_receiveCallback) {
        _receiveCallback(sender, type, plain, plainLen);
    }

    // Gestione interna (log)
    switch (type) {
        case MSG_HEARTBEAT: Serial.println("Ricevuto: Heartbeat (Online)"); break;
        case MSG_SOS_MEDIC: Serial.println("!!! SOS MEDICO RICEVUTO !!!"); break;
        case MSG_CHAT: {
            plain[plainLen] = '\0';
            Serial.printf("Chat da %08X: %s\n", sender, (char*)plain);
            break;
        }
        case MSG_ACK: {
            uint16_t ackSession = (plain[0] << 8) | plain[1];
            Serial.printf("Ricevuto ACK. Consegna confermata per sessione: %d\n", ackSession);
            break;
        }
        case MSG_NACK: {
            uint16_t nackSession = (plain[0] << 8) | plain[1];
            Serial.printf("Ricevuto NACK! Consegna fallita/Timeout per sessione: %d\n", nackSession);
            break;
        }
        default: Serial.printf("Ricevuto tipo sconosciuto: 0x%02X\n", type); break;
    }
}

void GMesh::handlePacketReassembly(GPacket &p) {
    int slot = -1;
    for (int i=0; i<5; i++) {
        if (_rxBuffer[i].senderID == p.senderID && _rxBuffer[i].sessionID == p.sessionID) {
            slot = i; break;
        }
    }
    if (slot == -1) {
        for (int i=0; i<5; i++) {
            if (_rxBuffer[i].receivedFrames == 0) {
                slot = i;
                _rxBuffer[i].senderID = p.senderID;
                _rxBuffer[i].targetID = p.targetID;
                _rxBuffer[i].sessionID = p.sessionID;
                _rxBuffer[i].totalFrames = p.totalFrames;
                _rxBuffer[i].actualPayloadLen = 0;
                break;
            }
        }
    }
    if (slot != -1) {
        uint16_t offset = p.frameIdx * 128;
        if (offset + p.payloadLen <= 1000) {
            memcpy(&_rxBuffer[slot].fullData[offset], p.payload, p.payloadLen);
            _rxBuffer[slot].receivedFrames++;
            _rxBuffer[slot].actualPayloadLen += p.payloadLen;
            _rxBuffer[slot].lastUpdate = millis();
        }
        if (_rxBuffer[slot].receivedFrames == _rxBuffer[slot].totalFrames) {
            processFinalMessage(p.msgType, _rxBuffer[slot].fullData, _rxBuffer[slot].actualPayloadLen, p.senderID);
            if (p.targetID == _nodeID && p.msgType != MSG_ACK && p.msgType != MSG_NACK) {
                sendAckNack(p.senderID, p.sessionID, true);
            }
            memset(&_rxBuffer[slot], 0, sizeof(IncomingMessage));
        }
    }
}

void GMesh::checkRxBufferTimeout() {
    for (int i=0; i<5; i++) {
        if (_rxBuffer[i].receivedFrames > 0 && _rxBuffer[i].receivedFrames < _rxBuffer[i].totalFrames) {
            if (millis() - _rxBuffer[i].lastUpdate > _rxAssemblyTimeout) {
                Serial.printf("Timeout ricezione! Svuoto scarpiera. Mittente: 0x%08X\n", _rxBuffer[i].senderID);
                if (_rxBuffer[i].targetID != 0xFFFFFFFF) {
                    sendAckNack(_rxBuffer[i].senderID, _rxBuffer[i].sessionID, false);
                }
                memset(&_rxBuffer[i], 0, sizeof(IncomingMessage));
            }
        }
    }
}

void GMesh::checkIncomingLora() {
    if (!_radio) return;
    int state = _radio->receive((uint8_t*)&_tempPacket, sizeof(GPacket), 150);
    if (state == RADIOLIB_ERR_NONE) {
        if (_tempPacket.stx != 0x02 || _tempPacket.etx != 0x03) return;
        float rssi = _radio->getRSSI();
        float snr = _radio->getSNR();

        // Forwarding (flooding)
        if (_floodingEnabled && _tempPacket.msgType == MSG_HEARTBEAT && _tempPacket.targetID == 0xFFFFFFFF) {
            if (!isMsgSeen(_tempPacket.senderID, _tempPacket.sessionID)) {
                if (_tempPacket.ttl > 0) {
                    _tempPacket.ttl--;
                    _radio->standby();
                    delay(10);
                    _radio->transmit((uint8_t*)&_tempPacket, sizeof(GPacket));
                    updateUI("MESH HB", "Relay Heartbeat", rssi, snr);
                }
            }
        }

        if (_tempPacket.targetID == _nodeID || _tempPacket.targetID == 0xFFFFFFFF) {
            if (!isMsgSeen(_tempPacket.senderID, _tempPacket.sessionID)) {
                updateUI("RX DATA", "Ricezione in corso", rssi, snr);
                handlePacketReassembly(_tempPacket);
            }
        }
    }
}

void GMesh::goToSleep() {
    updateUI("SLEEPING", "SleepMode avviata");
    if (_oled) _oled->ssd1306_command(SSD1306_DISPLAYOFF);
    if (_server) {
        _server->end();
        delete _server;
        _server = nullptr;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)_wakeButtonPin, 0);
    esp_sleep_enable_ext1_wakeup((1ULL << _wakeRadioPin), ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_timer_wakeup(_deepSleepTimeout * 1000);
    esp_deep_sleep_start();
}

void GMesh::wakeUpReason() {
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    switch(reason) {
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Waked Up from button"); break;
        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Waked Up from LoRa"); break;
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Waked Up from timer"); break;
        default: Serial.printf("wake up: %d\n", reason); break;
    }
}

void GMesh::setupRadio() {
    Serial.println("=== DIAGNOSTICA RADIO ===");
    SPIClass spi(FSPI);
    spi.begin(_sckPin, _misoPin, _mosiPin, _csPin);
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, LOW);
    delay(100);
    digitalWrite(_rstPin, HIGH);
    delay(200);
    pinMode(_busyPin, INPUT);
    delay(50);
    SPI.begin(_sckPin, _misoPin, _mosiPin);
    SPI.setFrequency(100000);
    _radio = new SX1262(new Module(_csPin, _dio1Pin, _rstPin, _busyPin, spi));
    Serial.print("Inizializzazione LoRa...");
    int state = _radio->begin(_frequency, _bandwidth, _spreadingFactor, _codingRate, 0x12, _txPower, 8);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf(" Fallito Codice: %d\n", state);
        delete _radio;
        _radio = nullptr;
        return;
    }
    Serial.println(" OK!");
    _radio->setCRC(true);
    _radio->setOutputPower(_txPower);
    delay(100);
    Serial.print("Test TX breve...");
    uint8_t testMsg[] = "G-MESH TEST";
    while(digitalRead(_busyPin) == HIGH) delay(1);
    state = _radio->transmit(testMsg, sizeof(testMsg));
    if (state == RADIOLIB_ERR_NONE) Serial.println(" OK!");
    else Serial.printf(" ERRORE: %d\n", state);
    delay(100);
    _radio->startReceive();
    Serial.println("Radio pronta in ricezione");
}

void GMesh::setupUI() {
    if (_server) return;
    char apName[20];
    sprintf(apName, "G-TALK-%08X", _nodeID);
    WiFi.softAP(apName, _apPassword);
    Serial.print("WiFi AP avviato. SSID: ");
    Serial.println(apName);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    _server = new AsyncWebServer(80);

    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        _lastActivity = millis();
        char idStr[10];
        sprintf(idStr, "%08X", _nodeID);
        String html = "<!DOCTYPE HTML><html><head>";
        html += "<title>G-TALK Dashboard</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "body { font-family: sans-serif; text-align: center; background: #222; color: #eee; }";
        html += ".btn { padding: 15px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
        html += ".sos { background: #d32f2f; color: white; }";
        html += ".msg { background: #388e3c; color: white; }";
        html += ".key { background: #fbc02d; color: black; }";
        html += "input { padding: 10px; width: 80%; margin: 10px; border-radius: 5px; border: none; }";
        html += "</style></head><body>";
        html += "<h2>G-TALK Terminal</h2>";
        html += "<div id='status'>ID: " + String(idStr) + "</div>";
        html += "<input type='text' id='targetID' placeholder='Target ID (es. 0xFFFFFFFF)'>";
        html += "<input type='text' id='chatMsg' placeholder='Scrivi messaggio...'><br>";
        html += "<button class='btn msg' onclick='sendData(\"CHAT\")'>INVIA CHAT</button>";
        html += "<button class='btn sos' onclick='sendData(\"SOS\")'>RICHIESTA MEDICO</button>";
        html += "<button class='btn key' onclick='sendData(\"KEY\")'>HANDSHAKE KYBER</button>";
        html += "<script>";
        html += "function sendData(type) {";
        html += "var tid = document.getElementById('targetID').value;";
        html += "var msg = document.getElementById('chatMsg').value;";
        html += "fetch('/send?type='+type+'&target='+tid+'&msg='+msg);";
        html += "}</script></body></html>";
        request->send(200, "text/html", html);
    });

    _server->on("/send", HTTP_GET, [this](AsyncWebServerRequest *request){
        _lastActivity = millis();
        if (request->hasParam("type") && request->hasParam("target")) {
            String type = request->getParam("type")->value();
            uint32_t targetID = strtoul(request->getParam("target")->value().c_str(), NULL, 16);
            String msg = request->hasParam("msg") ? request->getParam("msg")->value() : "";
            if (type == "SOS") {
                GPacket p = preparePacket(MSG_SOS_MEDIC, targetID, nullptr, 0);
                sendToRadio(p);
            } else if (type == "CHAT") {
                if (_encryptionEnabled && _secureReady) {
                    uint8_t securePayload[1000];
                    encryptGCM((uint8_t*)msg.c_str(), msg.length(), securePayload);
                    sendPacket(MSG_CHAT, targetID, securePayload, msg.length() + 28);
                } else {
                    sendPacket(MSG_CHAT, targetID, (uint8_t*)msg.c_str(), msg.length());
                }
            } else if (type == "KEY") {
                startKyberHandshake(targetID);
            }
            request->send(200, "text/plain", "Inviato");
        } else {
            request->send(400, "text/plain", "Parametri mancanti");
        }
    });

    _server->begin();
    Serial.println("Server web avviato su http://192.168.4.1");
}
