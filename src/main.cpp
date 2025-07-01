#define BLYNK_TEMPLATE_ID "TMPL6TdJHiUps"
#define BLYNK_TEMPLATE_NAME "LoRaRiceCopyCopy"
#define BLYNK_AUTH_TOKEN "LXtWlaaw4DLHM4mUKKIH-TbPgGJ3S6uw"

#define statusNode V27
#define VPUMP_SWITCH V25
#define VAUTO_SWITCH V23
#define VSTART_LEVEL V38
#define VSTOP_LEVEL V39
#define VWATER_LEVEL_NOW V1
#define VPUMP_TEMP V21
#define VPUMP_HUM V22

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_BME280.h>

#define BUTTON_MODE 4
#define BUTTON_PUMP 15
#define LED_MODE_PUMP 33
#define LED_STATUS 32
#define LED_PUMP 17
#define RelayOn 0
#define RelayOff 2

void checkPumpControl();
void readLocalButtons();
void turnPumpOn();
void turnPumpOff();

BlynkTimer timer;
Adafruit_BME280 bme;

bool pump_switch = false;
bool auto_switch = false;
int start_level = 0;
int stop_level = 0;
int water_level_now = 0;
bool pumpStatus = false;

float pumpTemp = 0.0;
float pumpHum = 0.0;

bool last_mode_button = HIGH;
bool last_pump_button = HIGH;
unsigned long lastDebounceTimeMode = 0;
unsigned long lastDebounceTimePump = 0;
const unsigned long debounceDelay = 50;

// =========================== Blynk ===========================
BLYNK_WRITE(VPUMP_SWITCH)
{
  if (!auto_switch)
  {
    pump_switch = param.asInt();
    Serial.print("Pump Switch (Manual from Blynk): ");
    Serial.println(pump_switch);
  }
  else
  {
    Serial.println("Ignored: Manual control disabled in Auto mode");
    Blynk.virtualWrite(VPUMP_SWITCH, pump_switch);
  }
}

BLYNK_WRITE(VAUTO_SWITCH)
{
  auto_switch = param.asInt();
  Serial.print("Auto Switch: ");
  Serial.println(auto_switch ? "AUTO" : "MANUAL");
}

BLYNK_WRITE(VSTART_LEVEL)
{
  start_level = param.asInt();
  Serial.print("Start Level: ");
  Serial.println(start_level);
}

BLYNK_WRITE(VSTOP_LEVEL)
{
  stop_level = param.asInt();
  Serial.print("Stop Level: ");
  Serial.println(stop_level);
}

BLYNK_WRITE(VWATER_LEVEL_NOW)
{
  water_level_now = param.asInt();
  Serial.print("Water Level Now: ");
  Serial.println(water_level_now);
}

// =========================== SETUP ===========================
void setup()
{
  Serial.begin(115200);

  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_PUMP, INPUT_PULLUP);
  pinMode(LED_MODE_PUMP, OUTPUT);
  pinMode(LED_PUMP, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(RelayOn, OUTPUT);
  pinMode(RelayOff, OUTPUT);

  digitalWrite(RelayOn, LOW);
  digitalWrite(RelayOff, LOW);
  digitalWrite(LED_PUMP, LOW);
  digitalWrite(LED_MODE_PUMP, LOW);
  digitalWrite(LED_STATUS, LOW);

  WiFiManager wm;
  if (!wm.startConfigPortal("ESP32-Setup-LoRaRice-Pump"))
  {
    Serial.println("Failed to connect to WiFi, restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("WiFi Connected!");

  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
  if (Blynk.connect())
  {
    Serial.println("Blynk Connected");
  }

  digitalWrite(LED_STATUS, HIGH);
  Blynk.virtualWrite(statusNode, 1);

  // เริ่ม BME280
  if (!bme.begin(0x76)) // ถ้าใช้ address อื่น เปลี่ยนได้ เช่น 0x77
  {
    Serial.println("Could not find BME280 sensor!");
    while (1)
      ;
  }

  timer.setInterval(200L, readLocalButtons);
  timer.setInterval(1000L, checkPumpControl);
}

// =========================== LOOP ===========================
void loop()
{
  Blynk.run();
  timer.run();
}

// =========================== BUTTON ===========================
void readLocalButtons()
{
  int readingMode = digitalRead(BUTTON_MODE);
  if (readingMode != last_mode_button)
    lastDebounceTimeMode = millis();
  if ((millis() - lastDebounceTimeMode) > debounceDelay)
  {
    if (readingMode == LOW && last_mode_button == HIGH)
    {
      auto_switch = !auto_switch;
      Blynk.virtualWrite(VAUTO_SWITCH, auto_switch);
      Serial.print("Mode toggled (button): ");
      Serial.println(auto_switch ? "AUTO" : "MANUAL");
    }
  }
  last_mode_button = readingMode;

  if (!auto_switch)
  {
    int readingPump = digitalRead(BUTTON_PUMP);
    if (readingPump != last_pump_button)
      lastDebounceTimePump = millis();
    if ((millis() - lastDebounceTimePump) > debounceDelay)
    {
      if (readingPump == LOW && last_pump_button == HIGH)
      {
        pump_switch = !pump_switch;
        Blynk.virtualWrite(VPUMP_SWITCH, pump_switch);
        Serial.print("Pump toggled (button): ");
        Serial.println(pump_switch);
      }
    }
    last_pump_button = readingPump;
  }
}

// =========================== PUMP CONTROL ===========================
void checkPumpControl()
{
  Serial.print("Water Level Now: ");
  Serial.println(water_level_now);

  digitalWrite(LED_MODE_PUMP, auto_switch ? HIGH : LOW);

  // อ่านค่าจาก BME280
  pumpTemp = bme.readTemperature();
  pumpHum = bme.readHumidity();
  Serial.print("Temp: ");
  Serial.print(pumpTemp);
  Serial.print(" °C | Hum: ");
  Serial.print(pumpHum);
  Serial.println(" %");

  // ส่งขึ้น Blynk
  Blynk.virtualWrite(VPUMP_TEMP, pumpTemp);
  Blynk.virtualWrite(VPUMP_HUM, pumpHum);

  if (auto_switch)
  {
    if (water_level_now <= start_level)
    {
      turnPumpOn();
      Serial.println("AUTO: ปั๊มเปิด");
    }
    else if (water_level_now >= stop_level)
    {
      turnPumpOff();
      Serial.println("AUTO: ปั๊มหยุด");
    }
  }
  else
  {
    if (pump_switch)
    {
      turnPumpOn();
      Serial.println("MANUAL: ปั๊มเปิด");
    }
    else
    {
      turnPumpOff();
      Serial.println("MANUAL: ปั๊มหยุด");
    }
  }
}

// =========================== RELAY ===========================
void turnPumpOn()
{
  if (!pumpStatus)
  {
    digitalWrite(RelayOn, HIGH);
    delay(100);
    digitalWrite(RelayOn, LOW);
    digitalWrite(LED_PUMP, HIGH);
    pumpStatus = true;
    Serial.println(">> Trigger ON");
  }
}

void turnPumpOff()
{
  if (pumpStatus)
  {
    digitalWrite(RelayOff, HIGH);
    delay(100);
    digitalWrite(RelayOff, LOW);
    digitalWrite(LED_PUMP, LOW);
    pumpStatus = false;
    Serial.println(">> Trigger OFF");
  }
}
