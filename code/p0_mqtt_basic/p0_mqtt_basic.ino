// MQTT BASIC: one sensor, one message, one dashboard
// Reads the LDR, switches a lamp when dark, and sends the reading to the
// Smart Town wall over MQTT every 2 seconds.
//
// Watch it: https://isuru-rana.github.io/sltiot-wall/   (your card appears)
// Raw messages: HiveMQ web client, subscribe to  sltiot/#

#include <WiFi.h>
#include <PubSubClient.h>        // Library Manager: add "PubSubClient"

// ---- YOUR TURN: your own name, no spaces ----
const char* HOME_ID = "DEMO-01";

const char* WIFI_SSID = "Wokwi-GUEST";      // Wokwi's free virtual WiFi
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com"; // free public broker
const int   MQTT_PORT = 1883;

const int LDR_PIN = 34;      // LDR module AO
const int LAMP_PIN = 25;     // LED through 220 ohm
const int DARK_BELOW = 30;   // % light

WiFiClient net;
PubSubClient mqtt(net);
String topic;                // sltiot/<HOME_ID>/telemetry

void setup() {
  Serial.begin(115200);
  pinMode(LAMP_PIN, OUTPUT);

  // 1. Join WiFi
  Serial.print("WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);       // channel 6 = faster on Wokwi
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(250); }
  Serial.println(" connected");

  // 2. Point at the broker
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  topic = String("sltiot/") + HOME_ID + "/telemetry";
}

void loop() {
  // 3. Stay connected to the broker
  if (!mqtt.connected()) {
    Serial.print("MQTT...");
    String id = String("sltiot-") + HOME_ID + "-" + String(random(0xffff), HEX);
    if (mqtt.connect(id.c_str())) Serial.println(" connected");
    else { Serial.printf(" failed (%d), retry\n", mqtt.state()); delay(2000); return; }
  }
  mqtt.loop();

  // 4. Read, decide, act
  int light = 100 - analogRead(LDR_PIN) * 100 / 4095;   // 0 = dark, 100 = bright
  bool dark = light < DARK_BELOW;
  digitalWrite(LAMP_PIN, dark ? HIGH : LOW);

  // 5. Publish a small JSON message
  char msg[120];
  snprintf(msg, sizeof msg, "{\"home\":\"%s\",\"light\":%d,\"lamp\":\"%s\",\"alarms\":\"\"}",
           HOME_ID, light, dark ? "ON" : "OFF");
  mqtt.publish(topic.c_str(), msg);
  Serial.printf("SENT %s  %s\n", topic.c_str(), msg);

  delay(2000);
}
