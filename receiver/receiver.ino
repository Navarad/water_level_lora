#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

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

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

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

// Funkcia na parsovanie prichádzajúceho packetu a volanie show_on_display
void parse_and_show_data(const String &packet) {
  int commaIndex = packet.indexOf('|');
  if (commaIndex > 0) {
    String depth = packet.substring(0, commaIndex);
    depth.trim();  // odstráni prípadné medzery
    String battery = packet.substring(commaIndex + 1);
    battery.trim();  // odstráni prípadné medzery
    show_on_display(depth, battery);
  } else {
    // Ak neobsahuje pajpu, zobraz celý packet ako fallback
    factory_display.clear();
    factory_display.drawString(0, 0, "Prijate:");
    factory_display.drawString(0, 20, packet);
    factory_display.display();
  }
}

void setup() {
  Serial.begin(115200);

  VextON();

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
