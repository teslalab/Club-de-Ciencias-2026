
#include <PubSubClient.h>
#include <WiFi.h>


// ====== Configuración WiFi ======
const char* WIFI_SSID = "Gaby";
const char* WIFI_PASS = "1234hola";


// ====== Configuración MQTT ======
const char* mqttServer = "test.mosquitto.org";  // <-- aquí tu dirección
const uint16_t mqttPort = 1883;

const char* mqttClientId = "";
const char* mqttUser     = "";
const char* mqttPass     = "";

const char* topicOut = "teslalab";
const char* topicIn  = "teslalab/commands";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");

  char command[128];

  unsigned int n = (length < sizeof(command) - 1) ? length : sizeof(command) - 1;

  for (unsigned int i = 0; i < n; i++) {
    command[i] = (char)payload[i];
  }
  command[n] = '\0';

  Serial.println(command);
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
    // Nota: si clientId/usuario/pass te pasan del buffer default, usa:
    // client.setBufferSize(255);

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

  connectWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  reconnectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();
}
