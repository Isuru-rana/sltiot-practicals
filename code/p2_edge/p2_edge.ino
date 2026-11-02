// P2 DECIDE AT THE EDGE: the home acts on its own, no internet needed
// Pump fills the tank, the street lamp follows daylight, alarms for heat and night motion

#include <DHT.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LDR_PIN = 34;
const int PIR_PIN = 19;
const int DHT_PIN = 15;
const int LAMP_PIN = 25;
const int PUMP_PIN = 26;
const int ALARM_PIN = 27;
const int BUZZER_PIN = 13;

// ---- YOUR TURN: the rules of your home ----
const int PUMP_START = 25;   // tank % : start pumping below this
const int PUMP_STOP = 90;    // tank % : stop pumping above this
const int DARK_BELOW = 30;   // light % : lamp on below this
const float HOT_C = 35.0;    // heat alarm above this

DHT dht(DHT_PIN, DHT22);
bool pumpOn = false;

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
  pinMode(LAMP_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  dht.begin();
}

void loop() {
  float cm = readDistanceCm();
  if (cm <= 0) return;   // no echo: skip this reading
  int tank = constrain(map((int)cm, 200, 20, 0, 100), 0, 100);
  int light = 100 - analogRead(LDR_PIN) * 100 / 4095;
  bool motion = digitalRead(PIR_PIN) == HIGH;
  float temp = dht.readTemperature();

  // Rule 1: pump with two limits, so it does not switch on and off every second
  if (tank < PUMP_START) pumpOn = true;
  if (tank > PUMP_STOP) pumpOn = false;

  // Rule 2: lamp follows daylight
  bool dark = light < DARK_BELOW;

  // Rule 3: alarms
  String alarms = "";
  if (temp > HOT_C) alarms += "HEAT ";
  if (dark && motion) alarms += "NIGHT MOTION ";
  if (tank < 10) alarms += "TANK EMPTY ";
  alarms.trim();

  digitalWrite(PUMP_PIN, pumpOn);
  digitalWrite(LAMP_PIN, dark);
  digitalWrite(ALARM_PIN, alarms.length() > 0);
  if (alarms.length() > 0) tone(BUZZER_PIN, 2000, 150);

  Serial.printf("Tank %d %% pump %s | Light %d %% lamp %s | %.1f C | %s\n",
                tank, pumpOn ? "ON" : "off", light, dark ? "ON" : "off", temp,
                alarms.length() ? alarms.c_str() : "OK");
  delay(1000);
}
