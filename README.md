# Water level sensor 

Distance sensor: JSN-SR04T-V3.3
Board Heltec Lora 32 v3.2

Useful docs:

https://docs.heltec.org/en/node/esp32/wifi_lora_32/index.html#important-resources

https://digitalconcepts.net.au/arduino/index.php?op=Battery

https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/TimerWakeUp/TimerWakeUp.ino


you need firebase firestore database setup,
you need to add this secrets.h file to folder
you need to set the api key from project settings in firebase
you need to set the email and password from firebase - authentication - users


for compilation you need to have installed Arduino IDE

library: FirebaseClient by Mobizt, maybe: Firebase ESP32 Client by Mobizt, maybe: Firebase ESP8266 Client by Mobizt

library: Heltec ESP32 Dev-Boards by Heltec, Heltec ESP8266 DevBoards by Heltec

secrets.example.h:

#pragma once

#define WIFI_SSID "ssid"

#define WIFI_PASSWORD "pass"


#define API_KEY "apikey"

#define USER_EMAIL "email"

#define USER_PASSWORD "pass"

#define FIREBASE_PROJECT_ID "id"

