#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "secrets.h"

// FIREBASE
#define ENABLE_USER_AUTH
#define ENABLE_FIRESTORE
#include <FirebaseClient.h>
#include "ExampleFunctions.h" // Provides the functions used in the examples.
#include <time.h> // nezabudni na začiatku
#include "FirebaseHelper.h"
SSL_CLIENT ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000 /* expire period in seconds (<3600) */);
FirebaseApp app;
Firestore::Documents Docs;
AsyncResult firestoreResult;
unsigned long dataMillis = 0;

// OLED displej
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// LoRa konfigurácia (frekvencia musí byť rovnaká ako na odosielacom zariadení)
#define RF_FREQUENCY 868000000  // 868 MHz pre Európu
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
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

void show_on_display(const String &text1, const String &text2) {
  factory_display.clear();
  factory_display.drawString(0, 0, "Hlbka: " + text1 + " cm");
  factory_display.drawString(0, 20, "Bateria: " + text2 + " V");
  factory_display.display();
}

void show_waiting_message() {
  factory_display.clear();
  factory_display.drawString(0, 0, "Cakam na LoRa...");
  factory_display.display();
}

void log_to_firebase(int depth, float battery) {
  if (app.ready()) {
    Document<Values::Value> doc = createWaterLevelDocument(depth, battery);
    String documentPath = "water_level_readings/";
    create_document_async(Docs, aClient, doc, documentPath);
    Serial.println("Dokument odoslaný: " + documentPath);
  }
}

// Funkcia na parsovanie prichádzajúceho packetu a volanie show_on_display
void parse_and_show_data(const String &packet) {
  const String prefix = "TDNODE|";

  if (!packet.startsWith(prefix)) {
    // Neplatný paket – ignoruj alebo zobraz fallback
    factory_display.clear();
    factory_display.drawString(0, 0, "Neznamy paket:");
    factory_display.drawString(0, 20, packet);
    factory_display.display();
    return;
  }

  // Odstráň prefix
  String payload = packet.substring(prefix.length());

  // Hľadáme ďalší oddelovač medzi hodnotami
  int sepIndex = payload.indexOf('|');
  if (sepIndex > 0) {
    String depth = payload.substring(0, sepIndex);
    depth.trim();
    String battery = payload.substring(sepIndex + 1);
    battery.trim();
    show_on_display(depth, battery);
    log_to_firebase(depth.toInt(), battery.toFloat());
  } else {
    // Chýba oddelovač medzi hodnotami – zobraz ako fallback
    factory_display.clear();
    factory_display.drawString(0, 0, "Chyba v pakete:");
    factory_display.drawString(0, 20, payload);
    factory_display.display();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  // Inicializácia OLED
  factory_display.init();
  factory_display.setFont(ArialMT_Plain_16);
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  factory_display.drawString(0, 0, "Cakam na LoRa...");
  factory_display.display();

  // Inicializácia LoRa
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

  // Prepneme rádio do režimu príjmu
  Radio.Rx(0);

  // FIREBASE
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
      Serial.print(".");
      delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
  set_ssl_client_insecure_and_buffer(ssl_client);
  Serial.println("Initializing app...");
  initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");
  app.getApp<Firestore::Documents>(Docs);
  // Nastavenie NTP času
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Čakám na čas (NTP)");
  while (time(nullptr) < 100000) { // čakaj kým sa čas zosynchronizuje (cca 1970 + 1 deň = bezpečná hranica)
      Serial.print(".");
      delay(200);
  }
  Serial.println();
  Serial.println("Čas nastavený.");
}

void loop() {
  Radio.IrqProcess();  // spracuj IRQ, ak nejaké prišlo

  if (new_packet) {
    Serial.printf("Prijaty packet: \"%s\"\n", rxpacket);
    parse_and_show_data(String(rxpacket));
    new_packet = false;

    delay(3000);  // necháme 3 sekundy na zobrazenie
    show_waiting_message();  // zobrazíme stav čakania
    Radio.Rx(0);  // znovu zapneme RX
  }

  // FIREBASE
  // To maintain the authentication and async tasks
  app.loop();
  // if (app.ready() && (millis() - dataMillis > 60000 || dataMillis == 0)) // každú minútu
  // {
  //     dataMillis = millis();

  //     float hladina = 82.5;     // v cm, meranie zo senzora
  //     float baterka = 3.92;     // v V, ADC čítanie batérie

  //     Document<Values::Value> doc = createWaterLevelDocument(hladina, baterka);
  //     String documentPath = "water_level_readings/";
  //     create_document_async(Docs, aClient, doc, documentPath);

  //     Serial.println("Dokument odoslaný: " + documentPath);
  // }
  // // For async call with AsyncResult.
  processData(firestoreResult);
}

// Callback: úspešný príjem
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memset(rxpacket, 0, sizeof(rxpacket));
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';  // ukonči reťazec
  new_packet = true;
}

// Callback: časový limit
void OnRxTimeout(void) {
  Serial.println("LoRa RX timeout");
  Radio.Rx(0);  // znovu čakaj
}

// Callback: chyba pri príjme
void OnRxError(void) {
  Serial.println("LoRa RX error");
  Radio.Rx(0);
}
