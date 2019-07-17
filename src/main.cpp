#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <SSD1306.h>
#include <WiFi.h>

// connections to L298N
#define EN_A 21
#define IN1 13
#define IN2 12

#define BUTTON_DOWN 32
#define BUTTON_UP 33
enum direction_t {
    down = -1,
    stop = 0,
    up = 1,
};

// motor runtime properties
uint16_t timeout_ms = 500;
uint8_t speed = 0;
direction_t direction = stop;

// PWM motor configuration
const int freq = 255;
const int chan = 0;
const int resolution = 8;
// display configuration
#define OLED_ADDRESS 0x3c
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
SSD1306 display(OLED_ADDRESS, OLED_SDA, OLED_SCL);

int display_speed = 0;
int display_redraw = 0;
unsigned long timeout_ts = 0;

// instance values
char name[30];

#define BLE_SERVICE_LIFT_UUID BLEUUID((uint16_t)35037)
#define BLE_CHAR_TIMEOUT_UUID BLEUUID((uint16_t)0xFF00)
#define BLE_CHAR_DIRECTION_UUID BLEUUID((uint16_t)0xFF01)
#define BLE_CHAR_SPEED_UUID BLEUUID((uint16_t)0xFF02)

BLECharacteristic timeoutChar(BLE_CHAR_TIMEOUT_UUID,
                              BLECharacteristic::PROPERTY_READ |
                                  BLECharacteristic::PROPERTY_WRITE |
                                  BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic directionChar(BLE_CHAR_DIRECTION_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                    BLECharacteristic::PROPERTY_WRITE |
                                    BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic speedChar(BLE_CHAR_SPEED_UUID,
                            BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY);

void redraw() {
    char stations_str[16];
    char speed_str[16];
    char timeout_str[16];
    snprintf(stations_str, 16, "%i", 0);
    snprintf(speed_str, 16, "%i", display_speed);
    double timeout_s = ((double)millis() - (double)timeout_ts) / 1000;
    snprintf(timeout_str, 16, "%15.1f", timeout_s);

    display.clear();
    display.flipScreenVertically();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 0, name + 20);

    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 24, "WiFi Stations");
    display.drawString(0, 36, "Speed");
    display.drawString(0, 48, "Last Movement");

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 24, stations_str);
    display.drawString(128, 36, speed_str);
    display.drawString(128, 48, timeout_str);
    display.display();

    display_redraw = millis() + 100;
}

void setSpeed(direction_t _direction, uint8_t _speed) {
    Serial.printf("INFO - Setting speed to %u, direction %i\n", _speed,
                  _direction);
    timeout_ts = millis() + timeout_ms;

    ledcWrite(chan, abs(speed));

    digitalWrite(IN1, (_direction == down));
    digitalWrite(IN2, (_direction == up));

    speed = _speed;
    direction = _direction;

    speedChar.setValue(&speed, 1);
    speedChar.notify();
    directionChar.setValue((uint8_t *)&direction, 1);
    directionChar.notify();

    redraw();
}

class TimeoutCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *timeoutChar) {
        int len = timeoutChar->getValue().length();

        if (len != 2) {
            Serial.printf("ERROR - received %i bytes for timeout, ignoring.\n ",
                          len);
            timeoutChar->setValue((uint8_t *)&timeout_ms, 2);
            return;
        }

        uint16_t _timeout_ms = *((uint16_t *)timeoutChar->getData());
        Serial.printf("INFO - BLE received timeout (%u byte): %u\n", len,
                      _timeout_ms);

        timeout_ms = _timeout_ms;
    }
};

class SpeedCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *speedChar) {
        int len = speedChar->getValue().length();

        if (len != 1) {
            Serial.printf("ERROR - received %u bytes for speed, ignoring.\n",
                          len);
            speedChar->setValue(&speed, 1);
            return;
        }

        uint8_t _speed = *speedChar->getData();
        Serial.printf("INFO - BLE received speed (%u byte): %u\n", len, _speed);

        setSpeed(direction, _speed);
    }
};

class DirectionCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *directionChar) {
        int len = directionChar->getValue().length();

        if (len != 1) {
            Serial.printf(
                "ERROR - received %i bytes for direction, ignoring.\n ", len);
            directionChar->setValue((uint8_t *)&direction, 1);
            return;
        }

        int8_t _direction_recv = (int8_t)*directionChar->getData();
        direction_t _direction = (direction_t)_direction_recv;

        // Test if received value is acceptable
        switch (_direction) {
        case down:
        case stop:
        case up:
            break;
        default:
            Serial.printf("ERROR - received direction %i, ignoring.\n ",
                          _direction);
            directionChar->setValue((uint8_t *)&direction, 1);
            return;
        }

        Serial.printf("INFO - BLE received direction (%u byte): %i\n", len,
                      _direction);
        setSpeed(_direction, speed);
    }
};

bool handleButtons() {
    if (digitalRead(BUTTON_UP) && digitalRead(BUTTON_DOWN)) {
        Serial.println("INFO - Both buttons pressed, stopping lift.");
        setSpeed(stop, 0);
        return true;
    }

    if (digitalRead(BUTTON_UP)) {
        Serial.println("INFO - Button up pressed.");
        setSpeed(up, 255);
        return true;
    }

    if (digitalRead(BUTTON_DOWN)) {
        Serial.println("INFO - Button down pressed.");
        setSpeed(down, 255);
        return true;
    }

    return false;
}

void setup() {
    // setup serial console output
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // read chip id
    uint64_t chipid = ESP.getEfuseMac();
    Serial.printf("INFO: ESP32 Chip ID: %04x\n", (uint16_t)(chipid >> 32));
    snprintf(name, 30, "nature40-liftsystem-%04x", (uint16_t)(chipid >> 32));

    // Reset OLED
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);

    // Init OLED
    display.init();
    redraw();
    Serial.println("INFO - OLED display initialised.");

    // print configuration
    Serial.printf("INFO - Name: %s\n", name);

    // setup manual control buttons
    pinMode(BUTTON_DOWN, INPUT);
    pinMode(BUTTON_UP, INPUT);

    // setup motor control pins
    pinMode(EN_A, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

    // setup PWM channel
    ledcSetup(chan, freq, resolution);
    ledcAttachPin(EN_A, chan);

    // set callback methods
    timeoutChar.setCallbacks(new TimeoutCharCallbacks());
    directionChar.setCallbacks(new DirectionCharCallbacks());
    speedChar.setCallbacks(new SpeedCharCallbacks());

    // setup BLE server, service and characteristics
    BLEDevice::init(name);
    BLEServer *bleServer = BLEDevice::createServer();
    BLEService *liftService = bleServer->createService(BLE_SERVICE_LIFT_UUID);
    liftService->addCharacteristic(&timeoutChar);
    liftService->addCharacteristic(&speedChar);
    liftService->addCharacteristic(&directionChar);
    bleServer->getAdvertising()->addServiceUUID(BLE_SERVICE_LIFT_UUID);

    // start lift service
    liftService->start();
    bleServer->getAdvertising()->start();

    // announce timeout and set initial break
    timeoutChar.setValue((uint8_t *)&timeout_ms, 2);
    timeoutChar.notify();
    setSpeed(down, 0);
}

void loop() {
    bool button_pressed = handleButtons();

    if (display_redraw < millis()) {
        redraw();
    }

    if (timeout_ts < millis()) {
        if (direction != stop) {
            Serial.printf("INFO - Hit timeout, stopping lift");
            setSpeed(stop, speed);
            timeout_ts = millis();
        }
    }

    delay(10);
}
