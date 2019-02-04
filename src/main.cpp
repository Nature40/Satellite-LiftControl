#include <WiFi.h>

const char *ssid = "foobar";
const char *pass = "supersicher";
const int   port = 35037;

WiFiServer server;
WiFiUDP udp;

char buff[64];

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  WiFi.softAP(ssid, pass);
  server.begin();

  Serial.print("GuMo ");
  Serial.print(WiFi.softAPIP());
  Serial.print("\n");

  if (!udp.begin(WiFi.softAPIP(), port)) {
    Serial.println("Failed to start UDP server");
    while (1);
  }
}

void loop() {
  int len = udp.parsePacket();
  if (len == 0) {
    return;
  }

  udp.read(buff, len);
  int cmdData = atoi(buff);

  Serial.println(cmdData);
}
