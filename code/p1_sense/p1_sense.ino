// P1 SENSE: your smart home reads its world
// Tank level (ultrasonic), daylight (LDR), motion (PIR), temperature (DHT22)

#include <DHT.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LDR_PIN = 34;
const int PIR_PIN = 19;
const int DHT_PIN = 15;

DHT dht(DHT_PIN, DHT22);

// The sensor sits on the tank lid and measures down to the water.
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long us = pulseIn(ECHO_PIN, HIGH, 30000);
  return us / 58.0;   // 0 means no echo came back
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  dht.begin();
  Serial.println("Smart home is awake");
}

void loop() {
  float cm = readDistanceCm();
  if (cm <= 0) return;                                          // no echo: skip this reading
  int tank = constrain(map((int)cm, 200, 20, 0, 100), 0, 100);  // 200 cm = empty, 20 cm = full
  int light = 100 - analogRead(LDR_PIN) * 100 / 4095;           // 0 = dark, 100 = bright
  bool motion = digitalRead(PIR_PIN) == HIGH;
  float temp = dht.readTemperature();

  Serial.printf("Tank %d %% | Light %d %% | Motion %s | Temp %.1f C\n",
                tank, light, motion ? "YES" : "no", temp);
  delay(2000);
}
