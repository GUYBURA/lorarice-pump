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
#include <EasyButton.h>

#define BUTTON_MODE 4
#define BUTTON_PUMP 15
#define LED_MODE_PUMP 33
#define LED_STATUS 32
#define LED_PUMP 17
#define RelayOn 25
#define RelayOff 26

void checkPumpControl();
void turnPumpOn();
void turnPumpOff();
void onModePressed();
void onPumpPressed();

BlynkTimer timer;
Adafruit_BME280 bme;

EasyButton buttonMode(BUTTON_MODE);
EasyButton buttonPump(BUTTON_PUMP);

bool pump_switch = false;
bool auto_switch = false;
int start_level = 0;
int stop_level = 0;
int water_level_now = 0;
bool pumpStatus = false;

float pumpTemp = 0.0;
float pumpHum = 0.0;

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
  water_level_now = param.asFloat();
  Serial.print("Water Level Now: ");
  Serial.println(water_level_now);
}
BLYNK_DISCONNECTED()
{
  Serial.println("Disconnected from Blynk!");

  Blynk.virtualWrite(statusNode, 0);

  digitalWrite(LED_STATUS, HIGH);
}
// =========================== SETUP ===========================
void setup()
{
  Serial.begin(115200);

  pinMode(LED_MODE_PUMP, OUTPUT);
  pinMode(LED_PUMP, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(RelayOn, OUTPUT);
  pinMode(RelayOff, OUTPUT);

  digitalWrite(RelayOn, LOW);
  digitalWrite(RelayOff, LOW);
  digitalWrite(LED_PUMP, HIGH);
  digitalWrite(LED_MODE_PUMP, HIGH);

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
    Blynk.syncVirtual(VWATER_LEVEL_NOW);
  }

  digitalWrite(LED_STATUS, LOW);
  Blynk.virtualWrite(statusNode, 1);
  Blynk.virtualWrite(VPUMP_SWITCH, pump_switch);
  Blynk.virtualWrite(VAUTO_SWITCH, auto_switch);

  Wire.begin(21, 22);
  if (!bme.begin(0x76))
  {
    Serial.println("Could not find BME280 sensor!");
    while (1)
      ;
  }

  buttonMode.begin();
  buttonPump.begin();
  buttonMode.onPressed(onModePressed);
  buttonPump.onPressed(onPumpPressed);

  timer.setInterval(1000L, checkPumpControl);
}

// =========================== LOOP ===========================
void loop()
{
  Blynk.run();
  timer.run();

  buttonMode.read();
  buttonPump.read();
}

// =========================== BUTTON HANDLERS ===========================
void onModePressed()
{
  auto_switch = !auto_switch;
  Blynk.virtualWrite(VAUTO_SWITCH, auto_switch);
  Serial.print("Mode toggled (button): ");
  Serial.println(auto_switch ? "AUTO" : "MANUAL");
}

void onPumpPressed()
{
  if (!auto_switch)
  {
    pump_switch = !pump_switch;
    Blynk.virtualWrite(VPUMP_SWITCH, pump_switch);
    Serial.print("Pump toggled (button): ");
    Serial.println(pump_switch);
  }
  else
  {
    Serial.println("Manual pump button ignored (AUTO mode)");
  }
}

// =========================== PUMP CONTROL ===========================
void checkPumpControl()
{
  Serial.print("Water Level Now: ");
  Serial.println(water_level_now);

  digitalWrite(LED_MODE_PUMP, auto_switch ? LOW : HIGH);

  pumpTemp = bme.readTemperature();
  pumpHum = bme.readHumidity();
  Serial.print("Temp: ");
  Serial.print(pumpTemp);
  Serial.print(" °C | Hum: ");
  Serial.print(pumpHum);
  Serial.println(" %");

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
    digitalWrite(LED_PUMP, LOW);
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
    digitalWrite(LED_PUMP, HIGH);
    pumpStatus = false;
    Serial.println(">> Trigger OFF");
  }
}
