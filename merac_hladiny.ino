#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

#define trigPin 46
#define echoPin 45

void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void display_distance(int distance) {
  factory_display.clear();
  factory_display.drawString(0, 0, "Vzdialenost:");
  factory_display.drawString(0, 20, String(distance) + " cm");
  factory_display.display();
}

void trigger_distance_sensor() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);
}

int get_distance_sensor_data(){
  int duration = pulseIn(echoPin, HIGH);
  int distance = (duration/2) / 29.1;
  return distance;
}

void log_distance(int distance){
  Serial.print("Distance = ");
  Serial.print(distance);
  Serial.println(" cm");
}

void setup() {
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

  delay(2000);
}

void loop() {
  // trigger sensor
  trigger_distance_sensor();
  
  // listen from sensor
  int distance = get_distance_sensor_data();
  
  // log distance
  log_distance(distance);

  // show on display
  display_distance(distance);

  delay(1000); 
}
