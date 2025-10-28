#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//DATOS DE CONEXIÓN
const char* ssid = "Fam_HERNANDEZ CONTRERAS";
const char* password = "NORTHLANE_2025";
const char* mqtt_server = "192.168.1.26";
const int mqtt_port = 1883;
const char* mqtt_user = "Alfredo";
const char* mqtt_pass = "220705";

//TOPICS MQTT
const char* topic_sensor = "zona1/riego/sensor";
const char* topic_led_rojo = "zona1/riego/estado/ledRojo";
const char* topic_led_verde = "zona1/riego/estado/ledVerde";
const char* topic_boton = "zona1/riego/estado/boton";
const char* topic_buzzer = "zona1/riego/estado/buzzer";

//DEFINICIÓN DE PINES
const int SENSOR_AOUT_PIN = 34;
const int LED_REGAR_PIN = 22;
const int LED_BOMBA_PIN = 23;
const int BOTON_BOMBA_PIN = 18;
const int BUZZER_PIN = 19;

//VARIABLES GLOBALES Y DE CALIBRACIÓN
const int VALOR_SECO = 4095;
const int VALOR_HUMEDO = 1700;
int valorHumedad = 0;
int porcentajeHumedad = 0;

//VARIABLES PARA DETECTAR CAMBIOS DE ESTADO
int last_led_rojo_state = -1;
int last_led_verde_state = -1;
int last_boton_state = -1;
int last_buzzer_state = -1;

// Intervalo de publicación del sensor
const long publishInterval = 2000;
unsigned long lastPublishTime = 0;

WiFiClient espClient;
PubSubClient client(espClient);

//FUNCIÓN DE RECONEXIÓN
void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conectar al broker MQTT...");
    String clientId = "ESP32_Sensor_Riego-" + WiFi.macAddress();
    clientId.replace(":", "");
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Conectado al broker MQTT");
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" -> Reintentando en 5 segundos");
      delay(5000);
    }
  }
}

//FUNCIÓN PARA PUBLICAR CAMBIOS DE ESTADO
void publishState(const char* topic, int& lastState, int currentState) {
  if (currentState != lastState) {
    client.publish(topic, String(currentState).c_str(), true);  // `true` para mensaje retenido
    lastState = currentState;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_REGAR_PIN, OUTPUT);
  pinMode(LED_BOMBA_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BOTON_BOMBA_PIN, INPUT);
  pinMode(SENSOR_AOUT_PIN, INPUT);
  digitalWrite(LED_REGAR_PIN, LOW);
  digitalWrite(LED_BOMBA_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //LECTURA Y LÓGICA DE CONTROL
  valorHumedad = analogRead(SENSOR_AOUT_PIN);
  porcentajeHumedad = map(valorHumedad, VALOR_SECO, VALOR_HUMEDO, 0, 100);
  if (porcentajeHumedad > 100) porcentajeHumedad = 100;
  if (porcentajeHumedad < 0) porcentajeHumedad = 0;

  //Lógica de control local
  int led_rojo_state = (porcentajeHumedad < 50) ? HIGH : LOW;
  digitalWrite(LED_REGAR_PIN, led_rojo_state);

  int boton_state = digitalRead(BOTON_BOMBA_PIN);
  digitalWrite(LED_BOMBA_PIN, boton_state);

  int buzzer_state = (porcentajeHumedad >= 80) ? HIGH : LOW;
  digitalWrite(BUZZER_PIN, buzzer_state);

  //PUBLICACIÓN DE ESTADOS
  publishState(topic_led_rojo, last_led_rojo_state, led_rojo_state);
  publishState(topic_boton, last_boton_state, boton_state);
  publishState(topic_led_verde, last_led_verde_state, boton_state);  // El LED verde refleja el estado del botón
  publishState(topic_buzzer, last_buzzer_state, buzzer_state);

  //PUBLICACIÓN PERIÓDICA DEL SENSOR
  if (millis() - lastPublishTime > publishInterval) {
    lastPublishTime = millis();
    StaticJsonDocument<128> doc;
    doc["humedad"] = porcentajeHumedad;
    doc["raw"] = valorHumedad;

    char buffer[128];
    serializeJson(doc, buffer);
    client.publish(topic_sensor, buffer);

    Serial.print("Publicado en MQTT: ");
    Serial.println(buffer);
  }
}