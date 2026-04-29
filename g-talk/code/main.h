/* * G-Mesh Project - (C) 2026 Emanuele Ferraro & GeoReport International Technologies
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include <Arduino.h>
#include <esp_mac.h>      // Per il MAC address
#include "mbedtls/aes.h"  // Per la crittografia hardware
#include <RadioLib.h>     // Per il modulo SX1262
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEFAULT_TTL 3

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

AsyncWebServer server(80);

// 1. DEFINIZIONE DELLE STRUTTURE
#define CACHE_SIZE 10
uint32_t msgCache[CACHE_SIZE];
uint8_t cacheIndex = 0;

enum GMessageType : uint8_t {
    // --- SISTEMA E CONNESSIONE ---
    MSG_NET_TEST          = 0x00, // xxr0: Test di rete/connessione
    MSG_HEARTBEAT         = 0x01, // xxr:  Segnale di vita (Heartbeat)
    MSG_ACK               = 0x08, // xxr8: Conferma di ricezione
    MSG_SAT_QUERY         = 0x09, // xxr9: Qualcuno sente il satellite?
    MSG_NODE_QUERY        = 0x0A, // xxr10: Qualcuno sente il nodo?
    MSG_BATT_LOW          = 0x0B, // xxr11: Low Battery Warning
    MSG_NET_TEST_ALT      = 0x17, // xxr23: Test di rete (alternativo)
    MSG_NO_SIGNAL         = 0x16, // xxr22: Qui non c'è campo
    MSG_IS_NODE           = 0x19, // xxr25: Il mittente è un nodo
    MSG_NACK              = 0x1B, // xxr27: Errore/Timeout ricezione (NUOVO)

    // --- EMERGENZA E PERICOLO ---
    MSG_SOS_GENERIC       = 0x02, // xxr2: SOS generico
    MSG_SOS_MEDIC         = 0x03, // xxr3: Ho bisogno di un medico
    MSG_CRIT_INJURY       = 0x04, // xxr4: Infortunio grave
    MSG_LOST              = 0x15, // xxr21: Mi sono perso
    MSG_GENERIC_PROBLEM   = 0x12, // xxr18: C'è un problema
    MSG_PATH_BLOCKED      = 0x13, // xxr19: Sentiero bloccato / frana

    // --- STATO E LOGISTICA ---
    MSG_SUPPLY_LOW        = 0x05, // xxr5: Esaurimento scorte o guasto tecnico
    MSG_STATUS_OK         = 0x06, // xxr6: Tutto bene
    MSG_FOUND             = 0x07, // xxr7: Ho trovato la persona/oggetto
    MSG_WEATHER_BAD       = 0x0C, // xxr12: Il meteo sta peggiorando
    MSG_RETURNING         = 0x0D, // xxr13: Mi fermo/Torno indietro
    MSG_STATUS_FINE       = 0x18, // xxr24: Qui tutto bene
    MSG_AUTH_NOTIFIED     = 0x1A, // xxr26: Chi di competenza è stato avvisato

    // --- INTERAZIONE E CHAT RAPIDA ---
    MSG_HOW_ARE_YOU       = 0x0E, // xxr1: Come stai?
    MSG_OK                = 0x0E, // xxr14: OK
    MSG_YES               = 0x0F, // xxr15: SI
    MSG_NO                = 0x10, // xxr16: NO
    MSG_DONT_KNOW         = 0x11, // xxr17: Non lo so
    MSG_RECEIVED          = 0x14  // xxr20: Messaggio ricevuto

    // --- TECNICI ---
    MSG_RECEIVED          = 0x14,
    MSG_CHAT              = 0x1C, // Aggiunto: necessario per la compilazione
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
    uint32_t targetID;       // Necessario per sapere se era un broadcast
    uint16_t sessionID;
    uint8_t  receivedFrames;
    uint8_t  totalFrames;
    uint16_t actualPayloadLen;
    uint8_t  fullData[1000]; // Buffer per messaggi lunghi
    uint32_t lastUpdate;     
};

// 2. VARIABILI GLOBALI E HARDWARE

SX1262 radio = new Module(8, 14, 12, 13);
uint32_t myID;
uint16_t currentSessionID = 0;
uint32_t currentNonce = 0;
uint8_t  shared_secret[32];
bool     is_secure_session = false;

IncomingMessage rxBuffer[5]; // La scarpiera
GPacket tempPacket;          // Contenitore per il pacchetto in arrivo

// 3. FUNZIONI DI SUPPORTO
void updateUI(String status, String lastMsg, float rssi = 0, float snr = 0) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.printf("ID: %08X", myID);
    display.setCursor(0, 10);
    display.print("Sec: ");
    display.print(is_secure_session ? "KYBER+AES" : "OFF");

    display.setCursor(0, 25);
    display.print("Stato: "); display.print(status);

    if(rssi != 0) {
        display.setCursor(0, 35);
        display.printf("Sig: %.1f dBm | SNR: %.1f", rssi, snr);
    }

    display.setCursor(0, 45);
    display.print("Ultimo Msg:");
    display.setCursor(0, 55);
    display.print(lastMsg);
    
    display.display();
}

bool isMsgSeen(uint32_t sender, uint16_t session) {
    uint32_t hash = sender ^ session;
    for(int i=0; i<CACHE_SIZE; i++) if(msgCache[i] == hash) return true;
    msgCache[cacheIndex] = hash;
    cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
    return false;
}

void handleKyberExchange(uint8_t type, uint8_t* data) {
    // Qui andranno kyber512_encap e kyber512_decap per popolare shared_secret
    updateUI("Kyber KEM", "Scambio chiavi PQC");
}

uint32_t generateDeviceID() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
}

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
    memset(p.payload, 0, 200); // Pulisce il payload prima dell'uso
    if (data != nullptr) memcpy(p.payload, data, p.payloadLen);
    return p;
}

void sendToRadio(GPacket &p) {
    Serial.printf("Invio MSG Tipo: 0x%02X a ID: 0x%08X\n", p.msgType, p.targetID);
    int state = radio.transmit((uint8_t*)&p, sizeof(GPacket));
    if (state != RADIOLIB_ERR_NONE) Serial.println("Errore TX!");
}

// Funzione dedicata all'invio di ACK e NACK
void sendAckNack(uint32_t target, uint16_t originalSessionID, bool isSuccess) {
    if (target == 0xFFFFFFFF) return; // Mai rispondere ai broadcast per evitare loop/flood

    uint8_t payload[2];
    payload[0] = (originalSessionID >> 8) & 0xFF;
    payload[1] = originalSessionID & 0xFF;

    GMessageType type = isSuccess ? MSG_ACK : MSG_NACK;
    GPacket p = preparePacket(type, target, payload, 2);
    sendToRadio(p);
}

// 4. LOGICA DI RICEZIONE E RICONVERSIONE

void processFinalMessage(uint8_t type, uint8_t* data, uint16_t len, uint32_t nonce) {
    if (type == MSG_KYBER_PUBKEY || type == MSG_KYBER_CIPHERTEXT) {
        handleKyberExchange(type, data);
        return;
    }
    uint8_t decrypted[1000];
    
    if (is_secure_session) {
        unsigned char iv[16] = {0};
        memcpy(iv, &nonce, 4);
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, shared_secret, 256);
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, data, decrypted);
        mbedtls_aes_free(&aes);
    } else {
        memcpy(decrypted, data, len);
    }

    switch (type) {
        case MSG_HEARTBEAT: Serial.println("Ricevuto: Heartbeat (Online)"); break;
        case MSG_SOS_MEDIC: Serial.println("!!! SOS MEDICO RICEVUTO !!!"); break;
        case MSG_CHAT:      Serial.printf("Chat: %s\n", (char*)decrypted); break;
        case MSG_ACK: {
            uint16_t ackSession = (decrypted[0] << 8) | decrypted[1];
            Serial.printf("Ricevuto ACK. Consegna confermata per sessione: %d\n", ackSession);
            break;
        }
        case MSG_NACK: {
            uint16_t nackSession = (decrypted[0] << 8) | decrypted[1];
            Serial.printf("Ricevuto NACK! Consegna fallita/Timeout per sessione: %d\n", nackSession);
            break;
        }
        default:            Serial.printf("Ricevuto tipo sconosciuto: 0x%02X\n", type); break;
    }
}

void handlePacketReassembly(GPacket &p) {
    int slot = -1;

    // Cerca se esiste già una sessione attiva per questo mittente
    for (int i = 0; i < 5; i++) {
        if (rxBuffer[i].senderID == p.senderID && rxBuffer[i].sessionID == p.sessionID) {
            slot = i;
            break;
        }
    }

    // Se non esiste, cerca il primo slot libero (receivedFrames == 0)
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

    // Se abbiamo trovato un posto (nuovo o esistente)
    if (slot != -1) {
        uint16_t offset = p.frameIdx * 200;
        if (offset + p.payloadLen <= 1000) { // Protezione buffer overflow
            memcpy(&rxBuffer[slot].fullData[offset], p.payload, p.payloadLen);
            rxBuffer[slot].receivedFrames++;
            rxBuffer[slot].actualPayloadLen += p.payloadLen;
            rxBuffer[slot].lastUpdate = millis();
        }

        if (rxBuffer[slot].receivedFrames == rxBuffer[slot].totalFrames) {
            // Calcolo lunghezza per AES (deve essere multiplo di 16)
            uint16_t cryptLen = rxBuffer[slot].actualPayloadLen;
            if (is_secure_session && (cryptLen % 16 != 0)) {
                cryptLen = ((cryptLen / 16) + 1) * 16;
            }

            processFinalMessage(p.msgType, rxBuffer[slot].fullData, cryptLen, p.nonce);
            
            if (p.targetID == myID && p.msgType != MSG_ACK && p.msgType != MSG_NACK) {
                sendAckNack(p.senderID, p.sessionID, true);
            }
            memset(&rxBuffer[slot], 0, sizeof(IncomingMessage));
        }
    }
}

// Funzione di pulizia RAM in caso di pacchetti bloccati/persi
void checkRxBufferTimeout() {
    for (int i = 0; i < 5; i++) {
        // Se lo slot è in uso ma non completato
        if (rxBuffer[i].receivedFrames > 0 && rxBuffer[i].receivedFrames < rxBuffer[i].totalFrames) {
            if (millis() - rxBuffer[i].lastUpdate > 120000) { // Timeout 120 secondi
                Serial.printf("Timeout ricezione! Svuoto scarpiera. Mittente: 0x%08X\n", rxBuffer[i].senderID);
                
                // Se non era un broadcast, avvisa il mittente che il pacchetto è andato perso
                if (rxBuffer[i].targetID != 0xFFFFFFFF) {
                    sendAckNack(rxBuffer[i].senderID, rxBuffer[i].sessionID, false);
                }
                
                memset(&rxBuffer[i], 0, sizeof(IncomingMessage)); // Libera la RAM
            }
        }
    }
}

void checkIncomingLora() {
    int state = radio.receive((uint8_t*)&tempPacket, sizeof(GPacket));
    
    if (state == RADIOLIB_ERR_NONE) {
        if (tempPacket.stx != 0x02 || tempPacket.etx != 0x03) return;

        float rssi = radio.getRSSI();
        float snr = radio.getSNR();

        // --- LOGICA TTL SOLO PER HEARTBEAT ---
        if (tempPacket.msgType == MSG_HEARTBEAT && tempPacket.targetID == 0xFFFFFFFF) {
            if (!isMsgSeen(tempPacket.senderID, tempPacket.sessionID)) {
                if (tempPacket.ttl > 0) {
                    tempPacket.ttl--;
                    radio.transmit((uint8_t*)&tempPacket, sizeof(GPacket)); // Rilancio mesh
                    updateUI("MESH HB", "Relay Heartbeat", rssi, snr);
                }
            }
        }

        // --- LOGICA MESSAGGI DIRETTI (NO MESH) ---
        // Accettiamo il pacchetto solo se è per noi o è un broadcast
        if (tempPacket.targetID == myID || tempPacket.targetID == 0xFFFFFFFF) {
            if (!isMsgSeen(tempPacket.senderID, tempPacket.sessionID)) {
                updateUI("RX DATA", "Ricezione in corso", rssi, snr);
                handlePacketReassembly(tempPacket);
            }
        }
    }
}

void setupUI() {
    // Crea una rete Wi-Fi chiamata "G-TALK-[TuoID]"
    char apName[20];
    sprintf(apName, "G-TALK-%08X", myID);
    WiFi.softAP(apName, "12345678");

    // Pagina principale
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    // Gestione dell'invio dal browser
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, [](const String& var){
        if(var == "MYID") {
            char idStr[10];
            sprintf(idStr, "%08X", myID);
            return String(idStr);
        }
        return String();
    });
});

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>G-TALK Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background: #222; color: #eee; }
    .btn { padding: 15px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
    .sos { background: #d32f2f; color: white; }
    .msg { background: #388e3c; color: white; }
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

  <script>
    function sendData(type) {
      var tid = document.getElementById('targetID').value;
      var msg = document.getElementById('chatMsg').value;
      fetch(`/send?type=${type}&target=${tid}&msg=${msg}`);
    }
  </script>
</body></html>
)rawliteral";

// 5. SETUP E LOOP

void setup() {
    Serial.begin(115200);
    myID = generateDeviceID();
    memset(rxBuffer, 0, sizeof(rxBuffer)); // Pulisce la scarpiera
    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
        Serial.println("SSD1306 Fallito");
    }
    updateUI("Benvenuto in G-MESH Gen.1", "Inizializzazione...");

    Serial.print("Inizializzazione LoRa...");
    int state = radio.begin(868.0); 
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(" OK!");
    } else {
        Serial.print(" Errore: "); Serial.println(state);
        while (true); 
    }
    
    setupUI();
}

void loop() {
    // 1. Ascolto continuo
    checkIncomingLora();

    // 2. Pulizia memoria (Timeout di 120 secondi)
    checkRxBufferTimeout();

    // 3. Invio periodico (Heartbeat)
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 60000) {
        GPacket p = preparePacket(MSG_HEARTBEAT, 0xFFFFFFFF, nullptr, 0);
        sendToRadio(p);
        lastMsg = millis();
    }
}
