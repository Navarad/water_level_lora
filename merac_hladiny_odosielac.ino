#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "esp_sleep.h"

// features
#define SENSOR_ENABLED true
#define BATTERY_READING_ENABLED true
#define LORA_ENABLED true
#define DISPLAY_ENABLED true
#define DEEP_SLEEP_ENABLED true
#define READING_FREQUENCY_MS 10000

// OLED display
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Battery
#define VBAT_PIN 1  // GPIO1 (VBat pin)
#define ADC_CTRL_PIN 37    // Needs to be HIGH before reading battery

// Sensor
#define trigPin 46
#define echoPin 45

// LoRa
#define RF_FREQUENCY 868000000  // 868 MHz for Europe
#define TX_OUTPUT_POWER 5
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define LORA_TIMEOUT_MS 3000

char txpacket[30];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

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
  digitalWrite(ADC_CTRL_PIN, HIGH);

  int raw = analogRead(VBAT_PIN);
  float v_adc = (raw / 4095.0) * 3.3;
  float vbat = v_adc * 4.9;

  digitalWrite(ADC_CTRL_PIN, LOW);

  return vbat;
}

void log_battery_voltage(float battery_voltage) {
  Serial.print("Battery voltage: ");
  Serial.print(battery_voltage, 2);
  Serial.println(" V");
}

void send_lora_distance(int distance) {
  if (lora_idle) {
    sprintf(txpacket, "Hlbka: %d cm", distance);
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    Serial.println("LoRa TX started.");
    lora_idle = false;

    // process lora status
    unsigned long start = millis();
    while (!lora_idle && millis() - start < LORA_TIMEOUT_MS) {
      // check if lora finished sending packet -> this should trigger TX done callback
      Radio.IrqProcess();
      delay(10);
    }
    // NOTE: if time passed without processing then the TX done callback wont be triggered 
    // after this time the timeout callback should be called
  }
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
    default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void deep_sleep() {
  // disable all peripherals
  if (LORA_ENABLED) {
    // Radio.Sleep();
  }
  if (SENSOR_ENABLED || DISPLAY_ENABLED) {
    // digitalWrite(Vext, HIGH);
  }
  if (DISPLAY_ENABLED) {
    // factory_display.displayOff();
  }

  // start timer and initiate deep sleep
  esp_sleep_enable_timer_wakeup(READING_FREQUENCY_MS * 1000); // microseconds
  Serial.flush();
  Serial.println("Deep sleep starting");
  esp_deep_sleep_start();
  // NOTE: code here should not be executed
  Serial.println("Deep sleep ending, you shouldn't see this");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("-----Setup starting-----");

  print_wakeup_reason();

  // battery
  if (BATTERY_READING_ENABLED) {
    analogReadResolution(12);  // 12-bit resolution (0–4095)
    pinMode(ADC_CTRL_PIN, OUTPUT);
  }

  // distance sensor JSN-SR04T-V3.3
  if (SENSOR_ENABLED){
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
  }

  // for display or distance sensor
  if (SENSOR_ENABLED || DISPLAY_ENABLED) {
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
  }

  // display setup
  if (DISPLAY_ENABLED) {
    factory_display.init();
    factory_display.clear();
    factory_display.setFont(ArialMT_Plain_16);
    factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  }

  // LoRa
  if (LORA_ENABLED) {
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;

    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, LORA_TIMEOUT_MS);
  }

  Serial.println("-----Setup ending-----");

  delay(2000);
}

void loop() {
  Serial.println("-----Loop start------");

  factory_display.clear();

  // battery
  if (BATTERY_READING_ENABLED) {
    float battery_voltage = get_battery_voltage();

    // log battery voltage
    log_battery_voltage(battery_voltage);

    //show on display
    if (DISPLAY_ENABLED) {
      display_battery(battery_voltage);
    }
  }

  if (SENSOR_ENABLED){
    // trigger distance sensor
    trigger_distance_sensor();
    
    // listen from the sensor
    int distance = get_distance_sensor_data();
    
    // log distance
    log_distance(distance);

    // show on display
    if (DISPLAY_ENABLED) {
      display_distance(distance);
    }

    // lora
    if (LORA_ENABLED) {
      send_lora_distance(distance);
    }
  }

  if (DEEP_SLEEP_ENABLED) {
    deep_sleep();
  } else {
    Serial.println("Starting delay");
    delay(READING_FREQUENCY_MS);
    Serial.println("Ending delay");
  }

  Serial.println("-----Loop end------");
}

// Callback functions
void OnTxDone(void) {
  Serial.println("LoRa TX finished.");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Serial.println("LoRa TX timeout.");
  lora_idle = true;
}
