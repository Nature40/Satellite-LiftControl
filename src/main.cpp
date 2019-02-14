#include <WiFi.h>

// connections to L298N
#define EN_A 21
#define IN1 13
#define IN2 12

// motor timeout (security fallback)
#define TIMEOUT_DELAY_MS 500

// PWM motor configuration
const int freq = 255;
const int chan = 0;
const int resolution = 8;

// Network configuration
char ssid[30];
const char *pass = "supersicher";
const int port = 35037;

WiFiServer server;
WiFiUDP udp;

#define MAX_UDP_SIZE 512

void setup() {
    // setup serial console output
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // read chip id
    uint64_t chipid = ESP.getEfuseMac();
    Serial.printf("ESP32 Chip ID: %04x\n", (uint16_t)(chipid >> 32));

    // setup WiFi access point
    snprintf(ssid, 30, "LiftSystem %04x", (uint16_t)(chipid >> 32));
    WiFi.softAP(ssid, pass);
    server.begin();

    // start udp server
    if (!udp.begin(WiFi.softAPIP(), port)) {
        Serial.println("Error: Failed to start UDP server");
        while (true) {
            delay(1000);
        }
    }

    // print configuration
    Serial.printf("SSID: %s\n", ssid);
    Serial.printf("Password: %s\n", pass);
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("Port: %i\n", port);

    // setup motor control pins
    pinMode(EN_A, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);

    // setup PWM channel
    ledcSetup(chan, freq, resolution);
    ledcAttachPin(EN_A, chan);
}

// timeout for motor command
int timeout = 0;

// parameters of last client
IPAddress remoteIP(0, 0, 0, 0);
uint16_t remotePort = 0;

int setSpeed(int speed) {
    if (speed > 255)
        speed = 255;
    if (speed < -255)
        speed = -255;

    Serial.printf("Setting speed to %i\n", speed);
    timeout = millis() + TIMEOUT_DELAY_MS;

    ledcWrite(chan, abs(speed));

    digitalWrite(IN1, (speed < 0));
    digitalWrite(IN2, (speed > 0));

    // send packet to last controller
    char buffer[MAX_UDP_SIZE];
    int payload_len = snprintf(buffer, MAX_UDP_SIZE, "set %i\n", speed);

    udp.beginPacket(remoteIP, remotePort);
    udp.write((const uint8_t *)buffer, payload_len);
    udp.endPacket();

    return speed;
}

bool handlePacket() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        remoteIP = udp.remoteIP();
        remotePort = udp.remotePort();

        char buffer[MAX_UDP_SIZE];
        size_t payload_len = udp.read(buffer, MAX_UDP_SIZE);
        buffer[payload_len] = 0;

        Serial.printf("Received %i bytes from %s:%i: '%s'\n", payload_len,
                      remoteIP.toString().c_str(), remotePort, buffer);

        int speedCmd = atoi(buffer);
        setSpeed(speedCmd);
    }

    return packetSize > 0;
}

void loop() {
    bool packet = handlePacket();

    if (timeout < millis() && ledcRead(chan) != 0) {
        setSpeed(0);
    }

    if (packet) {
        return;
    } else {
        delay(10);
    }
}
