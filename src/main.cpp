#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <DHT20.h>
#include <Wifi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Config
const char *ssid = "anhne";
const char *password = "abc102201";
const char *coreiot_server = "app.coreiot.io";
const char *access_token = "IOT_DEVICE_1S";

// WiFi & MQTT
WiFiClient espClient;
PubSubClient client(espClient);
#define DHTTYPE DHT22
#define DHTPIN 4
DHT dht(DHTPIN, DHTTYPE);
// LED
#define LED_PIN 2

// DHT20 dht20;
float temperature = 0.0;
float humidity = 0.0;

void connectWiFi()
{
  Serial.println("Starting WiFi connection...");

  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20)
  {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nWiFi Connection Failed!");
    ESP.restart();
  }
}

void reconnectMQTT()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Client", access_token, NULL))
    {
      Serial.println("Connected to MQTT Broker");
      client.subscribe("v1/devices/me/rpc/request/+");
    }
    else
    {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5s...");
      delay(5000);
    }
  }
}

void RPCHandler(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String message = String((char *)payload);
  Serial.print("Received RPC Request: ");
  Serial.println(message);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message.c_str());

  if (!error)
  {
    String method = doc["method"].as<String>();
    if (method == "setValueButtonLED")
    {
      bool ledState = doc["params"];
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      Serial.print("LED state: ");
      Serial.println(ledState ? "ON" : "OFF");
    }
  }
  else
  {
    Serial.print("JSON Parse Error: ");
    Serial.println(error.c_str());
  }
}

void TaskTemperature_Humidity(void *pvParameters)
{
  while (1)
  {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity))
    {
      Serial.print("Temp: ");
      Serial.print(temperature);
      Serial.print(" *C");
      Serial.print(" Humidity: ");
      Serial.print(humidity);
      Serial.print(" %");

      String payload = "{\"temperature\":" + String(temperature) + ", \"humidity\":" + String(humidity) + "}";
      Serial.println("Publishing: " + payload);
      client.publish("v1/devices/me/telemetry", payload.c_str());
    }
    else
    {
      Serial.println("DHT Sensor Error!");
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void TaskMQTT(void *pvParameters)
{
  while (1)
  {
    if (!client.connected())
    {
      reconnectMQTT();
    }
    client.loop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(DHTPIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  connectWiFi();
  client.setServer(coreiot_server, 1883);
  client.setCallback(RPCHandler);

  dht.begin();

  xTaskCreate(TaskMQTT, "MQTTHandler", 2048, NULL, 1, NULL);
  xTaskCreate(TaskTemperature_Humidity, "Temperature & Humidity", 2048, NULL, 2, NULL);
}

void loop()
{
}
