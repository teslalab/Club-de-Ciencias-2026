#include <PubSubClient.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

// ====== Configuración WiFi ======
const char* WIFI_SSID = "Gaby";
const char* WIFI_PASS = "1234hola";

// ====== Configuración MQTT ======
const char* mqttServer = "test.mosquitto.org";
const uint16_t mqttPort = 1883;

const char* mqttClientId = "esp32-neopixel-rod"; // pon algo único
const char* mqttUser     = "";
const char* mqttPass     = "";

const char* topicOut = "teslalab";
const char* topicIn  = "teslalab/commands";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ====== NeoPixel conf ======
#define PIN 25
#define NUMPIXELS 16
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// ====== Modos ======
enum Mode { MODE_OFF, MODE_WARM, MODE_PARTY, MODE_WHITE };
Mode currentMode = MODE_OFF;

// ====== Party animation state ======
unsigned long lastPartyUpdate = 0;
uint8_t partyIndex = 0;
const unsigned long partyDelayMs = 40; // velocidad de la secuencia

// Colores
uint32_t WARM_YELLOW() { return pixels.Color(255, 160, 40); }   // amarillo cálido
uint32_t FULL_WHITE()  { return pixels.Color(255, 255, 255); }  // blanco total

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

// Rueda de colores para modo fiesta (tipo rainbow)
uint32_t wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return pixels.Color(255 - pos * 3, 0, pos * 3);
  }
  if (pos < 170) {
    pos -= 85;
    return pixels.Color(0, pos * 3, 255 - pos * 3);
  }
  pos -= 170;
  return pixels.Color(pos * 3, 255 - pos * 3, 0);
}

void setParty() {
  currentMode = MODE_PARTY;
  partyIndex = 0;
  lastPartyUpdate = 0; // para que arranque de inmediato en loop
}

void updateParty() {
  unsigned long now = millis();
  if (now - lastPartyUpdate < partyDelayMs) return;
  lastPartyUpdate = now;

  for (int i = 0; i < NUMPIXELS; i++) {
    uint8_t c = (uint8_t)((i * 256 / NUMPIXELS + partyIndex) & 255);
    pixels.setPixelColor(i, wheel(c));
  }
  pixels.show();
  partyIndex++;
}

void handleCommands(int command) {
  switch (command) {
    case 1:
      // Toggle amarillo cálido
      if (currentMode == MODE_WARM) setOff();
      else setWarm();
      break;

    case 2:
      // Secuencia de fiesta
      if (currentMode == MODE_PARTY) setOff();
      else setParty();
      break;

    case 3:
      if (currentMode == MODE_WHITE) setOff();
      else setWhite();
      break;

    default:
      // Si llega algo raro, apaga
      setOff();
      break;
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

  int cmd = atoi(command);   // asume número
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

  // Animación no bloqueante si estamos en modo fiesta
  if (currentMode == MODE_PARTY) {
    updateParty();
  }
}
