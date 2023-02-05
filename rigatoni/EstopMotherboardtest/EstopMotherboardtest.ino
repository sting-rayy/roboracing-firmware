#include "EstopMotherboardtest.h"

void setup() {
    pinMode(INT, INPUT);
    pinMode(SAFE_RB, INPUT);
    pinMode(SENSOR_1, INPUT);
    pinMode(SENSOR_2, INPUT);
    pinMode(STEERING_IN, INPUT);
    pinMode(DRIVE_IN, INPUT);

    Serial.begin(115200);
    Serial.println("Initialized");
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print(digitalRead(SENSOR_1));
  Serial.print(digitalRead(STEERING_IN));
  Serial.print(digitalRead(DRIVE_IN));
  Serial.println(digitalRead(POWER_IN));
  delay(1000);
}
