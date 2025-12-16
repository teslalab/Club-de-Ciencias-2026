#include <PubSubClient.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "DHT.h"

// ====== Configuración WiFi ======
const char* WIFI_SSID = "galileo";
const char* WIFI_PASS = "";

// ====== Configuración MQTT ======
const char* mqttServer = "test.mosquitto.org";
const uint16_t mqttPort = 1883;

const char* mqttClientId = "esp32-neopixel-rod";
const char* mqttUser = "";
const char* mqttPass = "";

const char* topicOut = "teslalab";
const char* topicIn  = "teslalab/commands";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ====== NeoPixel conf ======
#define PIN 25
#define NUMPIXELS 16
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// ====== Temperature conf ======
#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== PIR conf ======
const uint8_t motionSensor = 14;
unsigned long now;

// ✅ PIR enable/disable por MQTT
bool pirEnabled = false;

// Timer: Auxiliary variables
volatile unsigned long lastTrigger = 0;
volatile bool startTimer = false;
bool printMotion = false;

const unsigned long timeSeconds = 5 * 1000UL;  // 5 seconds

void ARDUINO_ISR_ATTR motionISR() {
  lastTrigger = millis();
  startTimer = true;
}

// ====== Modos ======
enum Mode { MODE_OFF, MODE_WARM, MODE_PARTY, MODE_WHITE, MODE_WARNING };
Mode currentMode = MODE_OFF;

// ====== Party animation state ======
unsigned long lastPartyUpdate = 0;
uint8_t partyIndex = 0;
const unsigned long partyDelayMs = 40;

// Colores
uint32_t WARM_YELLOW() { return pixels.Color(255, 160, 40); }
uint32_t FULL_WHITE()  { return pixels.Color(255, 255, 255); }
uint32_t WARNING()     { return pixels.Color(255, 0, 0); }

void setOff() {
  currentMode = MODE_OFF;
  pixels.clear();
  pixels.show();
}

void setWarm() {
  currentMode = MODE_WARM;
  pixels.fill(WARM_YELLOW(), 0, NUMPIXELS);
  pixels.show();
}

void setWhite() {
  currentMode = MODE_WHITE;
  pixels.fill(FULL_WHITE(), 0, NUMPIXELS);
  pixels.show();
}

void setWarning() {
  currentMode = MODE_WARNING;
  pixels.fill(WARNING(), 0, NUMPIXELS);
  pixels.show();
}

// ✅ Attach/Detach interrupt (van aquí para evitar el error)
void enablePIR() {
  if (pirEnabled) return;
  pirEnabled = true;

  startTimer = false;
  printMotion = false;
  lastTrigger = millis();

  attachInterrupt(digitalPinToInterrupt(motionSensor), motionISR, RISING);
  Serial.println("PIR ENABLED (interrupt attached)");
}

void disablePIR() {
  if (!pirEnabled) return;
  pirEnabled = false;

  detachInterrupt(digitalPinToInterrupt(motionSensor));

  startTimer = false;
  printMotion = false;

  Serial.println("PIR DISABLED (interrupt detached)");

  // si estaba en warning por PIR, apaga (si temp está alta, luego vuelve a warning)
  if (currentMode == MODE_WARNING) setOff();
}

// Party color wheel
uint32_t wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) return pixels.Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return pixels.Color(0, pos * 3, 255 - pos * 3); }
  pos -= 170;
  return pixels.Color(pos * 3, 255 - pos * 3, 0);
}

void setParty() {
  currentMode = MODE_PARTY;
  partyIndex = 0;
  lastPartyUpdate = 0;
}

void updateParty() {
  unsigned long now = millis();
  if (now - lastPartyUpdate < partyDelayMs) return;
  lastPartyUpdate = now;

  // todos cambian juntos
  pixels.fill(wheel(partyIndex), 0, NUMPIXELS);
  pixels.show();
  partyIndex += 10;
}

void handleCommands(int command) {
  // ✅ Case 4 SIEMPRE permitido (toggle PIR)
  if (command == 4) {
    if (pirEnabled) disablePIR();
    else enablePIR();
    return;
  }

  if (currentMode != MODE_WARNING) {
    switch (command) {
      case 1:
        if (currentMode == MODE_WARM) setOff();
        else setWarm();
        break;

      case 2:
        if (currentMode == MODE_PARTY) setOff();
        else setParty();
        break;

      case 3:
        if (currentMode == MODE_WHITE) setOff();
        else setWhite();
        break;

      default:
        setOff();
        break;
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");

  char command[128];
  unsigned int n = (length < sizeof(command) - 1) ? length : (sizeof(command) - 1);
  for (unsigned int i = 0; i < n; i++) command[i] = (char)payload[i];
  command[n] = '\0';

  Serial.println(command);

  int cmd = atoi(command);
  handleCommands(cmd);
}

void connectWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi conectado. IP: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect(mqttClientId, mqttUser, mqttPass)) {
      Serial.println("OK");
      client.publish(topicOut, "hello world");
      client.subscribe(topicIn);
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" -> reintento en 2s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pixels.begin();
  dht.begin();

  pinMode(motionSensor, INPUT_PULLUP);

  // ✅ PIR apagado por defecto: NO attachInterrupt aquí
  pirEnabled = false;

  setOff();
  connectWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  reconnectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!client.connected()) reconnectMQTT();
  client.loop();

  now = millis();

  // ====== PIR -> WARNING_MODE SOLO si está habilitado ======
  if (pirEnabled) {
    if (startTimer && !printMotion) {
      Serial.println("MOTION DETECTED!!!");
      printMotion = true;
      setWarning();
    }

    if (startTimer && (now - lastTrigger > timeSeconds)) {
      Serial.println("Motion stopped...");
      startTimer = false;
      printMotion = false;

      if (currentMode == MODE_WARNING) setOff();
    }
  }

  if (currentMode == MODE_PARTY) {
    updateParty();
  }

  float t = dht.readTemperature();
  if (!isnan(t) && t > 30) {
    Serial.println("WARNING: La temperatura sobrepaso los 30°C");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print("\n");
    setWarning();
  } else {
    if (currentMode == MODE_WARNING && !startTimer) {
      setOff();
    }
  }
}
