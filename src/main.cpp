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
const IPAddress ip = IPAddress(192, 168, 4, 254);
const IPAddress gateway = IPAddress(192, 168, 4, 254);
const IPAddress subnet = IPAddress(255, 255, 255, 0);

WiFiServer server;
WiFiClient client;

#define MAX_CMD_SIZE 512

void setup() {
    // setup serial console output
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // read chip id
    uint64_t chipid = ESP.getEfuseMac();
    Serial.printf("ESP32 Chip ID: %04x\n", (uint16_t)(chipid >> 32));

    // setup WiFi access point
    snprintf(ssid, 30, "nature40.liftsystem.%04x", (uint16_t)(chipid >> 32));
    WiFi.softAP(ssid, pass);
    WiFi.softAPConfig(ip, gateway, subnet);
    server.begin(port);

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
    char buffer[MAX_CMD_SIZE];
    int cmd_len = snprintf(buffer, MAX_CMD_SIZE, "set %i\n", speed);

    client.write_P(buffer, cmd_len);

    return speed;
}

bool handleCommand() {
    if (!client.available())
        return false;

    char buffer[MAX_CMD_SIZE];
    size_t cmd_len = client.readBytesUntil('\n', buffer, MAX_CMD_SIZE);
    buffer[cmd_len] = '\0';

    Serial.printf("Received %i bytes from %s: '%s'\n", cmd_len,
                  client.remoteIP().toString().c_str(), buffer);

    int speedCmd = atoi(buffer);
    setSpeed(speedCmd);

    return true;
}

uint8_t checkClient() {
    if (!client)
        client = server.available();

    if (!client.connected())
        client = server.available();

    return client.connected();
}

void loop() {
    bool packet = false;

    // if a client is connected
    if (checkClient()) {
        packet = handleCommand();
    }

    if (timeout < millis() && ledcRead(chan) != 0) {
        setSpeed(0);
    }

    if (packet) {
        return;
    } else {
        delay(10);
    }
}
