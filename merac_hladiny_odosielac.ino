#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

#define LORA_ENABLED false
#define DISPLAY_ENABLED true

// OLED displej
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

#define VBAT_PIN 1  // GPIO1 (VBat pin)
#define ADC_CTRL_PIN 37    // Potrebuješ tento pin zapnúť HIGH pred čítaním

// Senzor
#define trigPin 46
#define echoPin 45

// LoRa konfigurácia (zmeň podľa regiónu)
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

char txpacket[30];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void display_distance(int distance) {
  factory_display.drawString(0, 0, "Hlbka: " + String(distance) + " cm");
  factory_display.display();
}

void display_battery(float battery_voltage) {
  factory_display.drawString(0, 20, "Bateria: " + String(battery_voltage) + " V");
  factory_display.display();
}

void trigger_distance_sensor() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);
}

int get_distance_sensor_data() {
  int duration = pulseIn(echoPin, HIGH);
  int distance = (duration / 2) / 29.1;
  return distance;
}

void log_distance(int distance) {
  Serial.print("Distance = ");
  Serial.print(distance);
  Serial.println(" cm");
}

float get_battery_voltage() {
  int raw = analogRead(VBAT_PIN);
  float v_adc = (raw / 4095.0) * 3.3;
  float vbat = v_adc * 4.9;
  return vbat;
}

void log_battery_voltage(float battery_voltage) {
  Serial.print("Battery voltage: ");
  Serial.print(battery_voltage, 2);
  Serial.println(" V");
}

void setup() {
  // battery
  analogReadResolution(12);  // 12-bitové rozlíšenie (0–4095)
  pinMode(ADC_CTRL_PIN, OUTPUT);
  digitalWrite(ADC_CTRL_PIN, HIGH);

  // distance sensor JSN-SR04T-V3.3
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // for display or distance sensor
  VextON();

  // display setup
  factory_display.init();
  factory_display.clear();
  factory_display.setFont(ArialMT_Plain_16);
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);

  // LoRa
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  delay(2000);
}

void loop() {
  factory_display.clear();

  // battery
  float battery_voltage = get_battery_voltage();

  // log battery voltage
  log_battery_voltage(battery_voltage);

  //show on display
  display_battery(battery_voltage);

  // trigger distance sensor
  trigger_distance_sensor();
  
  // listen from the sensor
  int distance = get_distance_sensor_data();
  
  // log distance
  log_distance(distance);

  // show on display
  display_distance(distance);

  // lora
  if (LORA_ENABLED) {
    if (lora_idle) {
      sprintf(txpacket, "Vzdialenost: %d cm", distance);
      Serial.printf("Odosielam LoRa packet: \"%s\"\n", txpacket);
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      lora_idle = false;
    }
    Radio.IrqProcess();
  }

  delay(2000);  // Posielaj každé 2s
}

// Callback funkcie
void OnTxDone(void) {
  Serial.println("LoRa TX hotové.");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Serial.println("LoRa TX timeout.");
  lora_idle = true;
}
