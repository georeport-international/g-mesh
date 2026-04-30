/* * G-Mesh Project - (C) 2026 Emanuele Ferraro & GeoReport International Technologies
 * Versione: ML-KEM (Kyber-512) + AES-256-GCM
 */

#include <Arduino.h>
#include <esp_mac.h>
#include "mbedtls/gcm.h"
#include <RadioLib.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pqc_kyber.h"

// --- DEFINIZIONE CACHE ---
#define CACHE_SIZE 10
uint32_t msgCache[CACHE_SIZE];
uint8_t cacheIndex = 0;

// --- CONFIGURAZIONE HARDWARE ---
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEFAULT_TTL 3

// --- PARAMETRI KYBER-512 ---
#define KYBER_PK_SIZE 800
#define KYBER_SK_SIZE 1632
#define KYBER_CT_SIZE 768

// --- STRUTTURE DATI ---
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
    uint8_t  payload[200];
    uint8_t  etx = 0x03;
};

struct IncomingBuffer {
    uint32_t senderID;
    uint16_t sessionID;
    uint8_t  receivedFrames;
    uint8_t  totalFrames;
    uint8_t  data[1000]; // Buffer per PK/CT o messaggi lunghi
    uint32_t lastUpdate;
};

// --- VARIABILI GLOBALI ---
SX1262 radio = new Module(8, 14, 12, 13);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
AsyncWebServer server(80);

uint32_t myID;
uint16_t currentSessionID = 0;
uint8_t  shared_secret[32];
uint8_t  my_private_key[KYBER_SK_SIZE]; // Temporanea per sessione
bool     is_secure_ready = false;
IncomingBuffer rxSlots[3]; 

// --- FUNZIONI CORE ---
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

uint32_t generateDeviceID() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
}

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

// --- CRITTOGRAFIA AES-GCM ---

void encryptGCM(uint8_t* plaintext, uint16_t len, uint8_t* outputPayload) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, shared_secret, 256);
    
    uint8_t iv[12];
    for(int i=0; i<12; i++) iv[i] = (uint8_t)esp_random();
    
    // Struttura nel payload: IV (12) + TAG (16) + CIPHERTEXT
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

void sendPacket(GMessageType type, uint32_t target, uint8_t* data, uint16_t len) {
    uint8_t totalFrames = (len == 0) ? 1 : (len / 200) + ((len % 200) != 0 ? 1 : 0);
    currentSessionID++;

    for (uint8_t i = 0; i < totalFrames; i++) {
        GPacket p;
        p.senderID = myID;
        p.targetID = target;
        p.sessionID = currentSessionID;
        p.msgType = (uint8_t)type;
        p.ttl = DEFAULT_TTL--;
        p.frameIdx = i;
        p.totalFrames = totalFrames;
        p.timestamp = millis() / 1000;
        
        uint16_t offset = i * 200;
        p.payloadLen = (len - offset > 200) ? 200 : (len - offset);
        if (data) memcpy(p.payload, data + offset, p.payloadLen);

        radio.transmit((uint8_t*)&p, sizeof(GPacket));
        if (totalFrames > 1) delay(200); // Prevenzione collisioni
    }
}
// --- logica riassemblaggio e messaggio ---
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

void processMessage(uint8_t type, uint8_t* data, uint16_t len, uint32_t sender) {
    if (type == MSG_KYBER_PUBKEY || type == MSG_KYBER_CIPHERTEXT) {
        handleKeyExchange(type, data, sender);
        return;
    }

    uint8_t plain[1000];
    if (is_secure_ready) {
        if (!decryptGCM(data, len - 28, plain)) {
            Serial.println("Errore Decrittazione/Auth!");
            return;
        }
    } else {
        memcpy(plain, data, len);
    }

    if (type == MSG_CHAT) Serial.printf("CHAT da %08X: %s\n", sender, (char*)plain);
    if (type == MSG_SOS_MEDIC) Serial.println("!!! SOS MEDICO RICEVUTO !!!");
}

void checkIncomingRadio() {
    GPacket p;
    int state = radio.receive((uint8_t*)&p, sizeof(GPacket));
  
    
    
    if (state == RADIOLIB_ERR_NONE) {
        if (p.stx != 0x02 || p.targetID != myID && p.targetID != 0xFFFFFFFF) return;

        float rssi = radio.getRSSI();
        float snr = radio.getSNR();

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

        int slot = -1;
        for (int i=0; i<3; i++) {
            if (rxSlots[i].senderID == p.senderID && rxSlots[i].sessionID == p.sessionID) { slot = i; break; }
            if (rxSlots[i].receivedFrames == 0) slot = i;
        }

        if (slot != -1) {
            rxSlots[slot].senderID = p.senderID;
            rxSlots[slot].sessionID = p.sessionID;
            memcpy(rxSlots[slot].data + (p.frameIdx * 200), p.payload, p.payloadLen);
            rxSlots[slot].receivedFrames++;
            rxSlots[slot].totalFrames = p.totalFrames;
            rxSlots[slot].lastUpdate = millis();

            if (rxSlots[slot].receivedFrames == rxSlots[slot].totalFrames) {
                processMessage(p.msgType, rxSlots[slot].data, (p.totalFrames-1)*200 + p.payloadLen, p.senderID);
                memset(&rxSlots[slot], 0, sizeof(IncomingBuffer));
            }
        }
    }
}

// --- ACK NACK ---
void sendAckNack(uint32_t target, uint16_t originalSessionID, bool isSuccess) {
    if (target == 0xFFFFFFFF) return; // Mai rispondere ai broadcast per evitare loop/flood

    uint8_t payload[2];
    payload[0] = (originalSessionID >> 8) & 0xFF;
    payload[1] = originalSessionID & 0xFF;

    GMessageType type = isSuccess ? MSG_ACK : MSG_NACK;
    GPacket p = preparePacket(type, target, payload, 2);
    sendToRadio(p);
}
// --- SETUP UI ---
void setupUI() {
    char apName[20];
    sprintf(apName, "G-TALK-%08X", myID);
    WiFi.softAP(apName, "12345678");

    // 1. Pagina principale con processore di template per l'ID
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

    // 2. Gestione dell'invio (RIPRISTINATA)
    server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("type") && request->hasParam("target")) {
            String type = request->getParam("type")->value();
            uint32_t targetID = strtoul(request->getParam("target")->value().c_str(), NULL, 16);
            String msg = request->hasParam("msg") ? request->getParam("msg")->value() : "";

            if(type == "SOS") {
                GPacket p = preparePacket(MSG_SOS_MEDIC, targetID, nullptr, 0);
                sendToRadio(p);
            } else if(type == "CHAT") {
                GPacket p = preparePacket(MSG_CHAT, targetID, (uint8_t*)msg.c_str(), msg.length());
                sendToRadio(p);
            }
            request->send(200, "text/plain", "Inviato");
        } else {
            request->send(400, "text/plain", "Parametri mancanti");
        }
    });

    server.begin();
    Serial.println("Interfaccia UI avviata correttamente");
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

// --- SETUP E LOOP ---

void setup() {
    Serial.begin(115200);
    myID = generateDeviceID();
    Wire.begin(OLED_SDA, OLED_SCL);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    updateUI("BOOT", "Inizializzazione...");

    if (radio.begin(868.0) != RADIOLIB_ERR_NONE) {
        Serial.println("Errore LoRa!");
        while(1);
    }

    // WiFi AP per interfaccia web (omesso per brevità, usa il tuo setupUI precedente)
    Serial.printf("Dispositivo Pronto. ID: %08X\n", myID);
    updateUI("READY", "In attesa di segnale...");

    setupUI();
}

void loop() {
    checkIncomingRadio();

    // Pulizia buffer timeout
    for(int i=0; i<3; i++) {
        if(rxSlots[i].receivedFrames > 0 && millis() - rxSlots[i].lastUpdate > 10000) {
            memset(&rxSlots[i], 0, sizeof(IncomingBuffer));
        }
    }

    // Esempio test: Se scrivi 'k' nel serial monitor, avvia handshake con un ID target
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'k') startKyberHandshake(0x12345678); // Sostituisci con ID reale
        if (c == 's') {
            uint8_t securePayload[228]; // 200 + 28 (IV+TAG)
            encryptGCM((uint8_t*)"Hello Secure World!", 20, securePayload);
            sendPacket(MSG_CHAT, 0x12345678, securePayload, 20 + 28);
        }
    }
   static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 60000) {
        GPacket p = preparePacket(MSG_HEARTBEAT, 0xFFFFFFFF, nullptr, 0);
        sendToRadio(p);
        lastMsg = millis();
     }
    if (getBatteryVoltage() < 3.4) {
     GPacket p = preparePacket(MSG_BATT_LOW, 0xFFFFFFFF, nullptr, 0);
     sendToRadio(p);
     }
}
