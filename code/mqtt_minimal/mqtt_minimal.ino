#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient net;
PubSubClient mqtt(net);

void setup() {
  Serial.begin(115200);
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  mqtt.setServer("broker.hivemq.com", 1883);
}

void loop() {
  if (!mqtt.connected()) mqtt.connect(("demo-" + String(random(0xffff))).c_str());
  mqtt.loop();

  int value = analogRead(34);
  String msg = "{\"light\":" + String(value * 100 / 4095) + "}";
  mqtt.publish("sltiot/DEMO-01/telemetry", msg.c_str());
  Serial.println(msg);

  delay(2000);
}
