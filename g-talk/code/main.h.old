/* * G-Mesh Project - (C) 2026 Emanuele Ferraro & GeoReport International Technologies
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
/* I will translate this file soon, it will receive periodical updates by the GeoReport IT Team.
 * Reach out our blog channel (only italian): https://youtube.com/@georeport-international-tech/
 */
#include <Arduino.h>
#include <esp_mac.h>      // Per il MAC address
#include "mbedtls/aes.h"  // Per la crittografia hardware
#include <RadioLib.h>     // Per il modulo SX1262
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

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
    server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request){
        String type = request->getParam("type")->value();
        String targetStr = request->getParam("target")->value();
        String msg = request->getParam("msg")->value();

        uint32_t targetID = strtoul(targetStr.c_str(), NULL, 16);
        
        if(type == "SOS") {
            GPacket p = preparePacket(MSG_SOS_MEDIC, targetID, nullptr, 0);
            sendToRadio(p);
        } else if(type == "CHAT") {
            GPacket p = preparePacket(MSG_CHAT, targetID, (uint8_t*)msg.c_str(), msg.length());
            sendToRadio(p);
        }
        request->send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("Interfaccia UI avviata");
}

// 1. DEFINIZIONE DELLE STRUTTURE

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
};

struct __attribute__((packed)) GPacket {
    uint8_t  stx = 0x02;
    uint32_t senderID;
    uint32_t targetID;
    uint16_t sessionID;
    uint8_t  msgType;
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
    uint16_t sessionID;
    uint8_t  receivedFrames;
    uint8_t  totalFrames;
    uint8_t  fullData[1000]; // Buffer per messaggi lunghi (es. Kyber o Chat)
    uint32_t lastUpdate;     
};

// 2. VARIABILI GLOBALI E HARDWARE

SX1262 radio = new Module(5, 2, 14, 32);

uint32_t myID;
uint16_t currentSessionID = 0;
uint32_t currentNonce = 0;
uint8_t  shared_secret[32];
bool     is_secure_session = false;

IncomingMessage rxBuffer[5]; // La tua "scarpiera"
GPacket tempPacket;          // Contenitore per il pacchetto in arrivo

// 3. FUNZIONI DI SUPPORTO

uint32_t generateDeviceID() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
}

GPacket preparePacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len) {
    GPacket p;
    p.senderID = myID;
    p.targetID = target;
    p.sessionID = ++currentSessionID;
    p.msgType = (uint8_t)type;
    p.frameIdx = 0;
    p.totalFrames = 1;
    p.timestamp = millis() / 1000;
    p.nonce = currentNonce++;
    p.payloadLen = (len > 200) ? 200 : len;
    if (data != nullptr) memcpy(p.payload, data, p.payloadLen);
    return p;
}

void sendToRadio(GPacket &p) {
    Serial.printf("Invio MSG Tipo: 0x%02X a ID: 0x%08X\n", p.msgType, p.targetID);
    int state = radio.transmit((uint8_t*)&p, sizeof(GPacket));
    if (state != RADIOLIB_ERR_NONE) Serial.println("Errore TX!");
}

// 4. LOGICA DI RICEZIONE E RICONVERSIONE

void processFinalMessage(uint8_t type, uint8_t* data, uint16_t len, uint32_t nonce) {
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

    // Qui il pacchetto è pronto per essere usato dall'interfaccia
    switch (type) {
        case MSG_HEARTBEAT: Serial.println("Ricevuto: Heartbeat (Online)"); break;
        case MSG_SOS_MEDIC: Serial.println("!!! SOS MEDICO RICEVUTO !!!"); break;
        case MSG_CHAT:      Serial.printf("Chat: %s\n", (char*)decrypted); break;
        default:            Serial.printf("Ricevuto tipo sconosciuto: 0x%02X\n", type); break;
    }
}

void handlePacketReassembly(GPacket &p) {
    int slot = -1;
    // Cerca slot esistente
    for (int i = 0; i < 5; i++) {
        if (rxBuffer[i].senderID == p.senderID && rxBuffer[i].sessionID == p.sessionID) {
            slot = i; break;
        }
    }
    // Se nuovo, cerca slot vuoto
    if (slot == -1) {
        for (int i = 0; i < 5; i++) {
            if (rxBuffer[i].receivedFrames == 0) {
                slot = i;
                rxBuffer[i].senderID = p.senderID;
                rxBuffer[i].sessionID = p.sessionID;
                rxBuffer[i].totalFrames = p.totalFrames;
                break;
            }
        }
    }

    if (slot != -1) {
        uint16_t offset = p.frameIdx * 200;
        memcpy(&rxBuffer[slot].fullData[offset], p.payload, p.payloadLen);
        rxBuffer[slot].receivedFrames++;
        rxBuffer[slot].lastUpdate = millis();

        if (rxBuffer[slot].receivedFrames == rxBuffer[slot].totalFrames) {
            processFinalMessage(p.msgType, rxBuffer[slot].fullData, rxBuffer[slot].totalFrames * 200, p.nonce);
            memset(&rxBuffer[slot], 0, sizeof(IncomingMessage)); // Svuota slot
        }
    }
}

void checkIncomingLora() {
    int state = radio.receive((uint8_t*)&tempPacket, sizeof(GPacket));
    if (state == RADIOLIB_ERR_NONE) {
        // Filtro destinazione (Mio ID o Broadcast)
        if (tempPacket.targetID != myID && tempPacket.targetID != 0xFFFFFFFF) return;
        // Filtro integrità base
        if (tempPacket.stx != 0x02 || tempPacket.etx != 0x03) return;

        handlePacketReassembly(tempPacket);
    }
}

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

    Serial.print("Inizializzazione LoRa...");
    int state = radio.begin(868.0); 
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(" OK!");
    } else {
        Serial.print(" Errore: "); Serial.println(state);
        while (true); 
    }
}

void loop() {
    // 1. Ascolto continuo
    checkIncomingLora();

    // 2. Invio periodico (Heartbeat)
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 60000) {
        GPacket p = preparePacket(MSG_HEARTBEAT, 0xFFFFFFFF, nullptr, 0);
        sendToRadio(p);
        lastMsg = millis();
    }
}
