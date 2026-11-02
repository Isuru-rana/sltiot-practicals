// P3 CONNECT: your home joins the Smart Town
// P2 + WiFi + MQTT. Sends its data every 5 s and takes orders from the town.

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ---- YOUR TURN: your own home name, e.g. "KANDY-07" (town + number, no spaces) ----
const char* HOME_ID = "HOME-01";

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";   // public test broker: no private data!
const int MQTT_PORT = 1883;

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LDR_PIN = 34;
const int PIR_PIN = 19;
const int DHT_PIN = 15;
const int LAMP_PIN = 25;
const int PUMP_PIN = 26;
const int ALARM_PIN = 27;
const int BUZZER_PIN = 13;

const int PUMP_START = 25;
const int PUMP_STOP = 90;
const int RATION_STOP = 50;   // town water rationing: fill only to half
const int DARK_BELOW = 30;
const float HOT_C = 35.0;

DHT dht(DHT_PIN, DHT22);
WiFiClient net;
PubSubClient mqtt(net);

enum PumpMode { PUMP_AUTO, PUMP_FORCE_ON, PUMP_FORCE_OFF };
PumpMode pumpMode = PUMP_AUTO;
bool pumpOn = false;
bool ration = false;          // set by the town for every home at once

String topicTelemetry, topicStatus, topicCmd;
const char* TOPIC_TOWN = "sltiot/town/cmd";
unsigned long lastRead = 0, lastPublish = 0, lastMqttTry = 0;

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long us = pulseIn(ECHO_PIN, HIGH, 30000);
  return us / 58.0;   // 0 means no echo came back
}

const char* modeName() {
  return pumpMode == PUMP_AUTO ? "AUTO" : (pumpMode == PUMP_FORCE_ON ? "ON" : "OFF");
}

// Orders arrive here: for this home only, or for the whole town.
void onCommand(char* topic, byte* payload, unsigned int length) {
  String cmd;
  for (unsigned int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.trim();
  Serial.printf("ORDER <- %s: %s\n", topic, cmd.c_str());
  if (cmd == "PUMP_ON") pumpMode = PUMP_FORCE_ON;
  else if (cmd == "PUMP_OFF") pumpMode = PUMP_FORCE_OFF;
  else if (cmd == "AUTO") pumpMode = PUMP_AUTO;
  else if (cmd == "RATION_ON") ration = true;
  else if (cmd == "RATION_OFF") ration = false;
  lastPublish = 0;   // report the change straight away
}

void connectMqtt() {
  if (mqtt.connected() || millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();
  String clientId = String("sltiot-") + HOME_ID + "-" + String(random(0xffff), HEX);
  Serial.print("Joining the town (MQTT)... ");
  // Last Will: if this home drops off, the broker tells everyone "offline".
  if (mqtt.connect(clientId.c_str(), topicStatus.c_str(), 0, true, "offline")) {
    Serial.println("joined");
    mqtt.publish(topicStatus.c_str(), "online", true);
    mqtt.subscribe(topicCmd.c_str());
    mqtt.subscribe(TOPIC_TOWN);
  } else {
    Serial.printf("failed, rc=%d\n", mqtt.state());
  }
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

  String base = String("sltiot/") + HOME_ID;
  topicTelemetry = base + "/telemetry";
  topicStatus = base + "/status";
  topicCmd = base + "/cmd";

  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(250); }
  Serial.println(" connected");

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setCallback(onCommand);
}

void loop() {
  connectMqtt();
  mqtt.loop();

  if (millis() - lastRead < 1000) return;
  lastRead = millis();

  float cm = readDistanceCm();
  if (cm <= 0) return;   // no echo: skip this reading
  int tank = constrain(map((int)cm, 200, 20, 0, 100), 0, 100);
  int light = 100 - analogRead(LDR_PIN) * 100 / 4095;
  bool motion = digitalRead(PIR_PIN) == HIGH;
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  int stopAt = ration ? RATION_STOP : PUMP_STOP;
  if (pumpMode == PUMP_FORCE_ON) pumpOn = true;
  else if (pumpMode == PUMP_FORCE_OFF) pumpOn = false;
  else {
    if (tank < PUMP_START) pumpOn = true;
    if (tank >= stopAt) pumpOn = false;
  }
  bool dark = light < DARK_BELOW;

  String alarms = "";
  if (temp > HOT_C) alarms += "HEAT ";
  if (dark && motion) alarms += "NIGHT MOTION ";
  if (tank < 10) alarms += "TANK EMPTY ";
  alarms.trim();

  digitalWrite(PUMP_PIN, pumpOn);
  digitalWrite(LAMP_PIN, dark);
  digitalWrite(ALARM_PIN, alarms.length() > 0);
  if (alarms.length() > 0) tone(BUZZER_PIN, 2000, 150);

  if (mqtt.connected() && (lastPublish == 0 || millis() - lastPublish > 5000)) {
    lastPublish = millis();
    char json[320];
    snprintf(json, sizeof(json),
             "{\"home\":\"%s\",\"tank\":%d,\"pump\":\"%s\",\"mode\":\"%s\",\"ration\":%s,"
             "\"light\":%d,\"lamp\":\"%s\",\"motion\":%s,\"temp\":%.1f,\"hum\":%.0f,"
             "\"alarms\":\"%s\",\"rssi\":%d}",
             HOME_ID, tank, pumpOn ? "ON" : "OFF", modeName(), ration ? "true" : "false",
             light, dark ? "ON" : "OFF", motion ? "true" : "false", temp, hum,
             alarms.c_str(), WiFi.RSSI());
    mqtt.publish(topicTelemetry.c_str(), json);
    Serial.printf("DATA -> %s %s\n", topicTelemetry.c_str(), json);
  }
}
