#include <WiFi.h>

// connections to L298N
#define EN_A 21
#define IN1 13
#define IN2 12

// PWM motor configuration
const int freq = 255;
const int chan = 0;
const int resolution = 8;

// Network configuration
char ssid [30];
const char *pass = "supersicher";
const int port = 35037;

WiFiServer server;
WiFiUDP udp;

char buff[64];

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  uint64_t chipid = ESP.getEfuseMac();
  Serial.printf("ESP32 Chip ID: %04x\n", (uint16_t)(chipid>>32));

  snprintf(ssid, 30, "LiftSystem %04x", (uint16_t)(chipid>>32));
  WiFi.softAP(ssid, pass);
  server.begin();

  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(pass);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Port: ");
  Serial.println(port);

  if (!udp.begin(WiFi.softAPIP(), port)) {
    Serial.println("Error: Failed to start UDP server");
    while (1);
  }

  // setup in pins
  pinMode(EN_A, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // setup PWM channel
  ledcSetup(chan, freq, resolution);
  ledcAttachPin(EN_A, chan);

}

void loop() {
  int len = udp.parsePacket();
  if (len == 0) {
    return;
  }

  udp.read(buff, len);
  int cmd_data = atoi(buff);

  Serial.println(cmd_data);

  ledcWrite(chan, abs(cmd_data));

  digitalWrite(IN1, (cmd_data < 0));
  digitalWrite(IN2, (cmd_data > 0));
}
