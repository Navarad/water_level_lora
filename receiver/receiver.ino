#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "secrets.h"
#include <WiFi.h>

// ---------------- GLOBALNÉ NASTAVENIA ----------------
bool FIREBASE_ENABLED = true; // 🔥 Zapnúť/vypnúť Firebase logovanie
bool WIFI_ENABLED = true;

// ---------------- FIREBASE ----------------
#define ENABLE_USER_AUTH
#define ENABLE_FIRESTORE
#include <FirebaseClient.h>
#include "ExampleFunctions.h" // Provides the functions used in the examples.
#include <time.h> // nezabudni na začiatku 
#include "FirebaseHelper.h"

SSL_CLIENT ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
FirebaseApp app;
Firestore::Documents Docs;
AsyncResult firestoreResult;
unsigned long dataMillis = 0;

// ---------------- OLED ----------------
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// ---------------- LoRa CONFIG ----------------
#define RF_FREQUENCY 868000000
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 8
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 1000

char rxpacket[32];
bool new_packet = false;
static RadioEvents_t RadioEvents;

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnRxTimeout(void);
void OnRxError(void);

// ---------------- FUNKCIE ----------------
void show_on_display(const String &text1, const String &text2, const String &text3) {
  factory_display.clear();
  factory_display.drawString(0, 0, "Hlbka: " + text1 + " cm");
  factory_display.drawString(0, 20, "Bateria: " + text2 + " V");
  factory_display.drawString(0, 40, "Cas: " + text3);
  factory_display.display();
}

void show_waiting_message() {
  factory_display.clear();
  factory_display.drawString(0, 0, "Cakam na LoRa...");
  factory_display.display();
}

void log_to_firebase(int depth, float battery) {
  if (!FIREBASE_ENABLED) return;
  Serial.println("Logging to firebase");
  if (app.ready()) {
    Document<Values::Value> doc = createWaterLevelDocument(depth, battery);
    String documentPath = "water_level_readings/";
    create_document_async(Docs, aClient, doc, documentPath);
    Serial.println("Dokument odoslaný: " + documentPath);
  }
}

String get_time_string() {
  time_t now = time(nullptr);
  if (now < 1700000000) return "NTP?";
  struct tm tmnow;
  localtime_r(&now, &tmnow);
  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M:%S", &tmnow);
  return String(buf);
}

void parse_and_show_data(const String &packet) {
  const String prefix = "TDNODE|";

  if (!packet.startsWith(prefix)) {
    factory_display.clear();
    factory_display.drawString(0, 0, "Neznamy paket:");
    factory_display.drawString(0, 20, packet);
    factory_display.display();
    return;
  }

  String payload = packet.substring(prefix.length());
  int sepIndex = payload.indexOf('|');

  if (sepIndex > 0) {
    String depth = payload.substring(0, sepIndex);
    depth.trim();
    String battery = payload.substring(sepIndex + 1);
    battery.trim();
    String time = get_time_string();
    show_on_display(depth, battery, time);
    log_to_firebase(depth.toInt(), battery.toFloat());
  } else {
    factory_display.clear();
    factory_display.drawString(0, 0, "Chyba v pakete:");
    factory_display.drawString(0, 20, payload);
    factory_display.display();
  }
}

void initWIFI() {
  if (!WIFI_ENABLED) return;
  
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Pripajam na Wi-Fi");
  
  int wifiRetry = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetry < 40) {
    Serial.print(".");
    delay(500);
    yield(); // kŕmenie watchdogu
    wifiRetry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi OK: " + WiFi.localIP().toString());
    initTime();
  } else {
    Serial.println("\nWi-Fi zlyhalo!");
  }
}

void initTime() {
  Serial.println("Syncujem NTP");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);

  struct tm timeinfo;
  int retry = 0;
  const int maxRetries = 20; // 10 sekúnd max
  while (!getLocalTime(&timeinfo) && retry < maxRetries) {
    delay(500);
    yield(); // ✅ kŕmi watchdog
    retry++;
  }

  if (retry < maxRetries) {
    Serial.println("\nNTP OK");
  } else {
    Serial.println("\nNTP zlyhal!");
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  // OLED init
  factory_display.init();
  factory_display.setFont(ArialMT_Plain_16);
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  factory_display.drawString(0, 0, "Cakam na LoRa...");
  factory_display.display();

  // LoRa init
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, false,
                    0, false, 0, 0, LORA_IQ_INVERSION_ON, true);
  Radio.Rx(0);
  initWIFI();
  if (FIREBASE_ENABLED) {
    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    set_ssl_client_insecure_and_buffer(ssl_client);
    initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");
    app.getApp<Firestore::Documents>(Docs);
  }
}

// ---------------- LOOP ----------------
void loop() {
  Radio.IrqProcess();

  if (new_packet) {
    Serial.printf("Prijaty packet: \"%s\"\n", rxpacket);
    parse_and_show_data(String(rxpacket));
    new_packet = false;

    // delay(3000);
    // show_waiting_message();
    Radio.Rx(0);
  }

  if (FIREBASE_ENABLED) {
    app.loop();
    processData(firestoreResult);
  }
}

// ---------------- CALLBACKS ----------------
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memset(rxpacket, 0, sizeof(rxpacket));
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  new_packet = true;
}

void OnRxTimeout(void) {
  Serial.println("LoRa RX timeout");
  Radio.Rx(0);
}

void OnRxError(void) {
  Serial.println("LoRa RX error");
  Radio.Rx(0);
}
