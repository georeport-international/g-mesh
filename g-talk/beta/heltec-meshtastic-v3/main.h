/* * G-Mesh Project - (C) 2026 Emanuele Ferraro & GeoReport International Technologies
 * Versione: ML-KEM (Kyber-512) + AES-256-GCM + Full Utilities
 */
// Last update: battery optimization

#include <Arduino.h>
#include <esp_mac.h>      // Per il MAC address
#include "mbedtls/gcm.h"  // Per la crittografia AES-GCM
//#include <heltec.h>     // Per il modulo SX1262
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pqc_kyber.h"
// --- LAST ACTIVITY FOR SCEEN TIMEOUT ---
unsigned long lastActivity = 0;
// --- CONFIGURAZIONE HARDWARE ---
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEFAULT_TTL 3
#define OLED_PWR 36  // or try with 35 if 36 does not work
// pin radio
#define LORA_SCK   9
#define LORA_MOSI  10
#define LORA_MISO  11
SX1262 radio = new Module(8, 14, 12, 13);

// --- PARAMETRI SISTEMA ---
#define CACHE_SIZE 10

// --- PARAMETRI KYBER-512 ---
#define KYBER_PK_SIZE 800
#define KYBER_SK_SIZE 1632
#define KYBER_CT_SIZE 768

// --- WAKE UP BUTTONS & LORA ---
#define BUTTON_PIN GPIO_NUM_0      // BOOT button
#define RADIO_DIO1_PIN GPIO_NUM_14 // LoRa module

// --- STRUTTURE DATI ---
enum GMessageType : uint8_t {
    // --- enum ---
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

    // --- EMERGENZA E PERICOLO ---
    MSG_SOS_GENERIC       = 0x02, 
    MSG_SOS_MEDIC         = 0x03, 
    MSG_CRIT_INJURY       = 0x04, 
    MSG_LOST              = 0x15, 
    MSG_GENERIC_PROBLEM   = 0x12, 
    MSG_PATH_BLOCKED      = 0x13, 

    // --- STATO E LOGISTICA ---
    MSG_SUPPLY_LOW        = 0x05, 
    MSG_STATUS_OK         = 0x06, 
    MSG_FOUND             = 0x07, 
    MSG_WEATHER_BAD       = 0x0C, 
    MSG_RETURNING         = 0x0D, 
    MSG_STATUS_FINE       = 0x18, 
    MSG_AUTH_NOTIFIED     = 0x1A, 

    // --- INTERAZIONE E CHAT RAPIDA ---
    MSG_HOW_ARE_YOU       = 0x0E, 
    MSG_OK                = 0x0E, 
    MSG_YES               = 0x0F, 
    MSG_NO                = 0x10, 
    MSG_DONT_KNOW         = 0x11, 
    MSG_RECEIVED          = 0x14, 

    // --- TECNICI ---
    MSG_CHAT              = 0x1C, 
    MSG_KYBER_PUBKEY      = 0x20, 
    MSG_KYBER_CIPHERTEXT  = 0x21
};

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
    uint8_t  payload[200]; 
    uint8_t  signature[32];
    uint8_t  etx = 0x03;
};

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

// --- VARIABILI GLOBALI ---
uint32_t msgCache[CACHE_SIZE];
uint8_t cacheIndex = 0;

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
AsyncWebServer server(80);

uint32_t myID;
uint16_t currentSessionID = 0;
uint32_t currentNonce = 0;
uint8_t  shared_secret[32];
uint8_t  my_private_key[KYBER_SK_SIZE]; 
bool     is_secure_ready = false;

IncomingMessage rxBuffer[5]; 
GPacket tempPacket;          

// --- FUNZIONI DI SUPPORTO ---
float getBatteryVoltage() {
  uint32_t raw = analogRead(1);
  float voltage = (raw / 4095.0) * 2 * 3.3 * 1.1; 
  return voltage;
}

uint32_t generateDeviceID() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
}

void updateUI(String status, String info, float rssi = 0, float snr = 0) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    
    oled.setCursor(0, 0);
    oled.printf("ID: %08X", myID);
    oled.setCursor(0, 10);
    oled.print("Sec: ");
    oled.print(is_secure_ready ? "KYBER+GCM" : "OFF");

    oled.setCursor(0, 25);
    oled.print("Stato: "); oled.print(status);

    if(rssi != 0) {
        oled.setCursor(0, 35);
        oled.printf("Sig: %.1f dBm | SNR: %.1f", rssi, snr);
    }

    oled.setCursor(0, 45);
    oled.print("Ultimo Msg:");
    oled.setCursor(0, 55);
    oled.print(info);
    
    oled.display();
}

bool isMsgSeen(uint32_t sender, uint16_t session) {
    uint32_t hash = sender ^ session;
    for(int i=0; i<CACHE_SIZE; i++) if(msgCache[i] == hash) return true;
    msgCache[cacheIndex] = hash;
    cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
    return false;
}

// --- CRITTOGRAFIA AES-GCM ---
void encryptGCM(uint8_t* plaintext, uint16_t len, uint8_t* outputPayload) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, shared_secret, 256);
    
    uint8_t iv[12];
    for(int i=0; i<12; i++) iv[i] = (uint8_t)esp_random();
    
    memcpy(outputPayload, iv, 12);
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, iv, 12, NULL, 0, plaintext, outputPayload + 28, 16, outputPayload + 12);
    mbedtls_gcm_free(&gcm);
}

bool decryptGCM(uint8_t* inputPayload, uint16_t cipherLen, uint8_t* outputPlaintext) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, shared_secret, 256);
    
    uint8_t iv[12];
    uint8_t tag[16];
    memcpy(iv, inputPayload, 12);
    memcpy(tag, inputPayload + 12, 16);
    
    int ret = mbedtls_gcm_auth_decrypt(&gcm, cipherLen, iv, 12, NULL, 0, tag, 16, inputPayload + 28, outputPlaintext);
    mbedtls_gcm_free(&gcm);
    return (ret == 0);
}

// --- COMUNICAZIONE RADIO ---
GPacket preparePacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len, uint8_t ttlValue = DEFAULT_TTL) {
    GPacket p;
    p.senderID = myID;
    p.targetID = target;
    p.sessionID = ++currentSessionID;
    p.msgType = (uint8_t)type;
    p.ttl = ttlValue; 
    p.frameIdx = 0;
    p.totalFrames = 1;
    p.timestamp = millis() / 1000;
    p.nonce = currentNonce++;
    p.payloadLen = (len > 200) ? 200 : len;
    memset(p.payload, 0, 200); 
    if (data != nullptr) memcpy(p.payload, data, p.payloadLen);
    return p;
}

void sendToRadio(GPacket &p) {
	Serial.printf("Invio MSG Tipo: 0x%02X\n", p.msgType);
	
	// FORZA LA RADIO IN STANDBY (interrompe la ricezione)
	radio.standby();
	delay(10);
	
	int state = radio.transmit((uint8_t*)&p, sizeof(GPacket));
	if (state == RADIOLIB_ERR_NONE) {
		Serial.println("TX OK!");
	} else {
		Serial.printf("Errore TX! Codice: %d\n", state);
	}
	
	// RIMETTE IN RICEZIONE dopo la trasmissione
	radio.startReceive();
}

void sendPacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len) {
    uint8_t totalFrames = (len == 0) ? 1 : (len / 200) + ((len % 200) != 0 ? 1 : 0);
    currentSessionID++;

    for (uint8_t i = 0; i < totalFrames; i++) {
        GPacket p;
        p.senderID = myID;
        p.targetID = target;
        p.sessionID = currentSessionID;
        p.msgType = (uint8_t)type;
        p.ttl = DEFAULT_TTL;
        p.frameIdx = i;
        p.totalFrames = totalFrames;
        p.timestamp = millis() / 1000;
        p.nonce = currentNonce++;
        
        uint16_t offset = i * 200;
        p.payloadLen = (len - offset > 200) ? 200 : (len - offset);
        memset(p.payload, 0, 200);
        if (data) memcpy(p.payload, data + offset, p.payloadLen);

        Serial.printf("Invio Frame %d/%d Tipo: 0x%02X a ID: 0x%08X\n", i+1, totalFrames, p.msgType, p.targetID);
        sendToRadio(p);
        if (totalFrames > 1) delay(200); 
    }
}

void sendAckNack(uint32_t target, uint16_t originalSessionID, bool isSuccess) {
    if (target == 0xFFFFFFFF) return; 

    uint8_t payload[2];
    payload[0] = (originalSessionID >> 8) & 0xFF;
    payload[1] = originalSessionID & 0xFF;

    GPacket p = preparePacket(isSuccess ? MSG_ACK : MSG_NACK, target, payload, 2);
    sendToRadio(p);
}

// --- LOGICA KYBER (ML-KEM) ---
void startKyberHandshake(uint32_t target) {
    updateUI("KYBER", "Generazione Chiavi...");
    uint8_t pk[KYBER_PK_SIZE];
    pqc_kyber512_keypair(pk, my_private_key);
    sendPacket(MSG_KYBER_PUBKEY, target, pk, KYBER_PK_SIZE);
    updateUI("KYBER", "PK Inviata, attesa CT");
}

void handleKeyExchange(uint8_t type, uint8_t* fullData, uint32_t remoteID) {
    if (type == MSG_KYBER_PUBKEY) {
        uint8_t ct[KYBER_CT_SIZE];
        pqc_kyber512_encapsulate(ct, shared_secret, fullData);
        sendPacket(MSG_KYBER_CIPHERTEXT, remoteID, ct, KYBER_CT_SIZE);
        is_secure_ready = true;
        updateUI("SECURE", "Sessione Criptata OK");
    } 
    else if (type == MSG_KYBER_CIPHERTEXT) {
        pqc_kyber512_decapsulate(shared_secret, fullData, my_private_key);
        is_secure_ready = true;
        updateUI("SECURE", "Handshake Completato");
    }
}

// --- RICEZIONE E RIASSEMBLAGGIO ---
void processFinalMessage(uint8_t type, uint8_t* data, uint16_t len, uint32_t sender) {
    if (type == MSG_KYBER_PUBKEY || type == MSG_KYBER_CIPHERTEXT) {
        handleKeyExchange(type, data, sender);
        return;
    }

    uint8_t plain[1000];
    uint16_t plainLen = len;

    if (is_secure_ready && type != MSG_ACK && type != MSG_NACK && type != MSG_HEARTBEAT && len >= 28) {
        if (!decryptGCM(data, len - 28, plain)) {
            Serial.println("Errore Decrittazione/Auth GCM!");
            return;
        }
        plainLen = len - 28;
    } else {
        memcpy(plain, data, len);
    }

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

void handlePacketReassembly(GPacket &p) {
    int slot = -1;

    for (int i = 0; i < 5; i++) {
        if (rxBuffer[i].senderID == p.senderID && rxBuffer[i].sessionID == p.sessionID) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        for (int i = 0; i < 5; i++) {
            if (rxBuffer[i].receivedFrames == 0) {
                slot = i;
                rxBuffer[i].senderID = p.senderID;
                rxBuffer[i].targetID = p.targetID;
                rxBuffer[i].sessionID = p.sessionID;
                rxBuffer[i].totalFrames = p.totalFrames;
                rxBuffer[i].actualPayloadLen = 0; 
                break;
            }
        }
    }

    if (slot != -1) {
        uint16_t offset = p.frameIdx * 200;
        if (offset + p.payloadLen <= 1000) { 
            memcpy(&rxBuffer[slot].fullData[offset], p.payload, p.payloadLen);
            rxBuffer[slot].receivedFrames++;
            rxBuffer[slot].actualPayloadLen += p.payloadLen;
            rxBuffer[slot].lastUpdate = millis();
        }

        if (rxBuffer[slot].receivedFrames == rxBuffer[slot].totalFrames) {
            processFinalMessage(p.msgType, rxBuffer[slot].fullData, rxBuffer[slot].actualPayloadLen, p.senderID);
            
            if (p.targetID == myID && p.msgType != MSG_ACK && p.msgType != MSG_NACK) {
                sendAckNack(p.senderID, p.sessionID, true);
            }
            memset(&rxBuffer[slot], 0, sizeof(IncomingMessage));
        }
    }
}

void checkRxBufferTimeout() {
    for (int i = 0; i < 5; i++) {
        if (rxBuffer[i].receivedFrames > 0 && rxBuffer[i].receivedFrames < rxBuffer[i].totalFrames) {
            if (millis() - rxBuffer[i].lastUpdate > 120000) { 
                Serial.printf("Timeout ricezione! Svuoto scarpiera. Mittente: 0x%08X\n", rxBuffer[i].senderID);
                if (rxBuffer[i].targetID != 0xFFFFFFFF) {
                    sendAckNack(rxBuffer[i].senderID, rxBuffer[i].sessionID, false);
                }
                memset(&rxBuffer[i], 0, sizeof(IncomingMessage)); 
            }
        }
    }
}
void checkIncomingLora() {
    int state = radio.receive((uint8_t*)&tempPacket, sizeof(GPacket), 50);
    
    if (state == RADIOLIB_ERR_NONE) {
        if (tempPacket.stx != 0x02 || tempPacket.etx != 0x03) return;
        
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();

        if (tempPacket.msgType == MSG_HEARTBEAT && tempPacket.targetID == 0xFFFFFFFF) {
            if (!isMsgSeen(tempPacket.senderID, tempPacket.sessionID)) {
                if (tempPacket.ttl > 0) {
                    tempPacket.ttl--;
                    radio.standby();
                    delay(10);
                    radio.transmit((uint8_t*)&tempPacket, sizeof(GPacket));
                    updateUI("MESH HB", "Relay Heartbeat", rssi, snr);
                }
            }
        }

        if (tempPacket.targetID == myID || tempPacket.targetID == 0xFFFFFFFF) {
            if (!isMsgSeen(tempPacket.senderID, tempPacket.sessionID)) {
                updateUI("RX DATA", "Ricezione in corso", rssi, snr);
                handlePacketReassembly(tempPacket);
            }
        }
    }
}

// --- UI WEB SERVER ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>G-TALK Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background: #222; color: #eee; }
    .btn { padding: 15px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
    .sos { background: #d32f2f; color: white; }
    .msg { background: #388e3c; color: white; }
    .key { background: #fbc02d; color: black; }
    input { padding: 10px; width: 80%; margin: 10px; border-radius: 5px; border: none; }
  </style>
</head><body>
  <h2>G-TALK Terminal</h2>
  <div id="status">ID: %MYID%</div>
  
  <input type="text" id="targetID" placeholder="Target ID (es. 0xFFFFFFFF)">
  <input type="text" id="chatMsg" placeholder="Scrivi messaggio...">
  
  <br>
  <button class="btn msg" onclick="sendData('CHAT')">INVIA CHAT</button>
  <button class="btn sos" onclick="sendData('SOS')">RICHIESTA MEDICO</button>
  <button class="btn key" onclick="sendData('KEY')">HANDSHAKE KYBER</button>

  <script>
    function sendData(type) {
      var tid = document.getElementById('targetID').value;
      var msg = document.getElementById('chatMsg').value;
      fetch(`/send?type=${type}&target=${tid}&msg=${msg}`);
    }
  </script>
</body></html>
)rawliteral";

void setupUI() {
    char apName[20];
    sprintf(apName, "G-TALK-%08X", myID);
    WiFi.softAP(apName, "12345678");
    
    Serial.print("WiFi AP avviato. SSID: ");
    Serial.println(apName);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        lastActivity = millis();
        
        char idStr[10];
        sprintf(idStr, "%08X", myID);
        
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
        html += "<button class='btn sos' onclick='sendData(\"SOS\")'>SOS</button>";
        html += "<button class='btn key' onclick='sendData(\"KEY\")'>HANDSHAKE</button>";
        html += "<script>";
        html += "function sendData(type) {";
        html += "var tid = document.getElementById('targetID').value;";
        html += "var msg = document.getElementById('chatMsg').value;";
        html += "fetch('/send?type='+type+'&target='+tid+'&msg='+msg);";
        html += "}</script></body></html>";
        
        request->send(200, "text/html", html);
    });

    server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request){
        lastActivity = millis();
        if (request->hasParam("type") && request->hasParam("target")) {
            String type = request->getParam("type")->value();
            uint32_t targetID = strtoul(request->getParam("target")->value().c_str(), NULL, 16);
            String msg = request->hasParam("msg") ? request->getParam("msg")->value() : "";

            if(type == "SOS") {
                GPacket p = preparePacket(MSG_SOS_MEDIC, targetID, nullptr, 0);
                sendToRadio(p);
            } else if(type == "CHAT") {
                if (is_secure_ready) {
                    uint8_t securePayload[1000];
                    encryptGCM((uint8_t*)msg.c_str(), msg.length(), securePayload);
                    sendPacket(MSG_CHAT, targetID, securePayload, msg.length() + 28);
                } else {
                    sendPacket(MSG_CHAT, targetID, (uint8_t*)msg.c_str(), msg.length());
                }
            } else if(type == "KEY") {
                startKyberHandshake(targetID);
            }
            request->send(200, "text/plain", "Inviato");
        } else {
            request->send(400, "text/plain", "Parametri mancanti");
        }
    });

    server.begin();
    Serial.println("Server web avviato su http://192.168.4.1");
}

// --- SLEEP MODE ---
void SleepMode() {
    updateUI("SLEEPING", "SleepMode avviata");
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
    server.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);
    esp_sleep_enable_ext1_wakeup((1ULL << RADIO_DIO1_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_timer_wakeup(60 * 1000000);
    esp_deep_sleep_start();
}

void WakeUpReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Waked Up from button"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Waked Up from LoRa"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Waked Up from timer"); break;
    default: Serial.printf("wake up: %d\n", wakeup_reason); break;
  }
}

// --- SETUP E LOOP ---
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== AVVIO ===");
    SPI.begin(9, 11, 10, 8);  // SCK, MISO, MOSI, CS
    Serial.println("SPI avviato correttamente...");
    delay(200);
    
    Serial.println("Accensione display...");
    pinMode(OLED_PWR, OUTPUT);
    digitalWrite(OLED_PWR, LOW);
    delay(500);
    
    Serial.println("Reset display...");
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(100);
    digitalWrite(OLED_RST, HIGH);
    delay(200);
    
    Serial.println("Avvio I2C...");
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(100000);
    delay(200);
    
    Serial.println("Scansione I2C in corso...");
    int deviceCount = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("Trovato I2C: 0x%02X\n", addr);
            deviceCount++;
            if (addr == 0x3C) Serial.println("  >>> INDIRIZZO OLED 0x3C TROVATO! <<<");
            if (addr == 0x3D) Serial.println("  >>> INDIRIZZO OLED 0x3D TROVATO! <<<");
        }
        delay(1);
    }
    Serial.printf("Totale dispositivi I2C trovati: %d\n", deviceCount);
    
    if (deviceCount == 0) {
        Serial.println("NESSUN DISPOSITIVO I2C TROVATO");
    }
    
    Serial.println("Inizializzazione display...");
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
    delay(50);
    Serial.println("Display OK con 0x3C!");
    
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.println("G-Mesh Starting...");
    oled.display();
    delay(100);
    oled.display();
    delay(100);
    oled.display();
    
    // ===== RADIO LORA =====
    Serial.print("Inizializzazione LoRa...");
    Serial.println("=== DIAGNOSTICA RADIO ===");

    pinMode(23, OUTPUT);
    digitalWrite(23, HIGH);
    Serial.println("Pin 23: HIGH (alimentazione radio)");

    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);
    delay(100);
    digitalWrite(12, HIGH);
    delay(100);
    Serial.println("Pin 12: reset completato");

    SPI.begin(9, 11, 10, 8);  // SCK, MISO, MOSI, CS
    Serial.printf("SPI iniziato con SCK=%d, MISO=%d, MOSI=%d\n", LORA_SCK, LORA_MISO, LORA_MOSI);

    delay(100);
    byte version = 0;
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(8, LOW);
    version = SPI.transfer(0x42);
    digitalWrite(8, HIGH);
    SPI.endTransaction();
    Serial.printf("Lettura registro versione: 0x%02X (dovrebbe essere 0x12 o 0x22)\n", version);

    Serial.print("Inizializzazione LoRa...");
    int state = radio.begin(868.0);
    Serial.printf(" Risultato: %d\n", state);
    pinMode(23, OUTPUT);
    digitalWrite(23, HIGH);
    delay(100);

    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);
    delay(100);
    digitalWrite(12, HIGH);
    delay(100);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(" OK");
    } else {
        Serial.printf(" Fallito! Codice: %d\n", state);
        while(true);
    }

    radio.setOutputPower(10);

    Serial.print("Test TX breve...");
    uint8_t testMsg[] = "G-MESH TEST";
    int txState = radio.transmit(testMsg, sizeof(testMsg));
    if (txState == RADIOLIB_ERR_NONE) {
        Serial.println(" OK!");
    } else {
        Serial.printf(" ERRORE: %d\n", txState);
    }
    /*Serial.print("Inizializzazione LoRa...");
    Heltec.begin(false, true, true, false);
    Heltec.LoRa.setSpreadingFactor(12);
    Heltec.LoRa.setSignalBandwidth(125);
    Heltec.LoRa.setCodingRate4(8);
    Heltec.LoRa.setTxPower(10, 1);
    Serial.println(" OK");

    Serial.print("Test TX breve...");
    uint8_t testMsg[] = "G-MESH TEST";
    Heltec.LoRa.beginPacket();
    Heltec.LoRa.write(testMsg, sizeof(testMsg));
    Heltec.LoRa.endPacket();
    Serial.println(" inviato!");*/
    
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    myID = generateDeviceID();
    memset(rxBuffer, 0, sizeof(rxBuffer));
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println("Risveglio... Processo pacchetto...");
    }
    
    setupUI();
    server.begin();
    Serial.println("Server web avviato su:");
    Serial.println(WiFi.softAPIP());
    
    updateUI("READY", wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 ? "Msg ricevuto" : "Online");
}

void loop() {
    if (millis() - lastActivity > 600000) {
        SleepMode();
    }
    checkIncomingLora();
    checkRxBufferTimeout();

    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 60000) {
        GPacket p = preparePacket(MSG_HEARTBEAT, 0xFFFFFFFF, nullptr, 0);
        sendToRadio(p);
        lastMsg = millis();
    }
    
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'k') startKyberHandshake(0xFFFFFFFF);
        if (c == 's') {
            if (is_secure_ready) {
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
