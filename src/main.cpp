#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>

const char *ssid = "foobar";
const char *pass = "supersicher";

WiFiServer server;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  WiFi.softAP(ssid, pass);
  server.begin();

  Serial.print("GuMo ");
  Serial.print(WiFi.softAPIP());
  Serial.print("\n");

  if (udp.begin(WiFi.softAPIP(), 2323) == 0) {
    Serial.println("udp begin failed");
    while (1);
  }

  while (1) {
    char buff[128];
    int len = udp.parsePacket();

    if (len == 0) {
      continue;
    }

    udp.read(buff, len);
    Serial.println(buff);
  }
}

void loop() {
}
