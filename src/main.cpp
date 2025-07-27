#include <Arduino.h>
#include <wifi.h>
#include <ArduinoJson.h>
#include <MQTTClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// my includes
#include <secrets.h>

// my defines
#define LED 2
bool LEDstatus = true;
int batteryPin = A2; //IO34/A2 on FireBeetle 2
int batteryMillivolts = 0; // variable to store the battery voltage in millivolts
float batteryVoltage = 0.0; // variable to store the battery voltage in Volts
float temperature = 0.0; // variable to store the temperature in Fahrenheit
const int oneWireBus = D10; // D10 (GPIO17) on FireBeetle 2
OneWire oneWire(oneWireBus); // create OneWire instance on the specified pin
DallasTemperature sensors(&oneWire); // create DallasTemperature instance with the OneWire instance

// constants
const char* ssid = SECRET_SSID;
const char* ssid_pass = SECRET_SSID_PASSWORD;
const char* mqtt_address = MQTT_HOST;
const int mqtt_port = 1883;
const char* mqtt_user = MQTT_USERNAME;
const char* mqtt_password = MQTT_KEY;
const char* mqtt_temp_discovery_topic = "homeassistant/sensor/basementfridge/temp/config";
const char* mqtt_battery_discovery_topic = "homeassistant/sensor/basementfridge/battery/config";
const char* mqtt_publish_topic_temp = "homeassistant/sensor/basementfridge/temp";
const char* mqtt_publish_topic_battery = "homeassistant/sensor/basementfridge/battery";
const char* mqtt_hass_status_topic = "homeassistant/sensor/basementfridge/status";
const char* mqtt_client_id = "BasementFridge";
const int publish_interval = 5000;  // 5 seconds

// variables
unsigned long lastPublishTime = 0;
WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

// function declarations
void connectToMQTT();

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW); // turn on LED to indicate startup
  pinMode(batteryPin, INPUT);
  analogSetAttenuation(ADC_11db); // set attenuation to 11dB 1575 = 1V

  // start the DS18B20 temperature sensor
  sensors.begin();

  // read battery voltage
  batteryMillivolts = analogReadMilliVolts(batteryPin); // read the battery voltage in millivolts
  Serial.printf("Battery voltage: %d mV\n", batteryMillivolts);
  batteryVoltage = (float)batteryMillivolts / 1000.0 * 2; // convert to Volts
  Serial.printf("Battery voltage: %.2f V\n", batteryVoltage);

  // limit battery decimal places to 2
  batteryVoltage = roundf(batteryVoltage * 100) / 100.0; // round to 2 decimal places

  // read temperature
  sensors.requestTemperatures(); // request temperature from the sensor
  temperature = sensors.getTempFByIndex(0); // get the temperature in Fahrenheit
  Serial.printf("Temperature: %.2f F\n", temperature);

  // limit temperature decimal places to 2
  temperature = roundf(temperature * 100) / 100.0; // round

  // connect to WiFi
  Serial.print("Wifi connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, ssid_pass);
  uint32_t notConnectedCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    notConnectedCounter++;
    if(notConnectedCounter > 300) {  // reset if wifi not connected after 30s
      Serial.println("Resetting due to Wifi not connecting...");
      ESP.restart();
    }
  }

  Serial.print("\n\n");
  Serial.print("Wifi connected, IP address: ");
  Serial.println(WiFi.localIP());

  connectToMQTT();

  // // publish discovery message to Home Assistant for temperature sensor
  // StaticJsonDocument<256> doc;
  // doc["name"] = "Basement_Fridge_Temperature";
  // doc["device_class"] = "temperature";
  // doc["state_topic"] = mqtt_publish_topic_temp;
  // doc["unique_id"] = "basementfridge_temp";
  // doc["value_template"] = "{{ value_json.temperature }}";
  // doc["unit_of_measurement"] = "F";
  // char jsonBuffer[256];
  // serializeJson(doc, jsonBuffer);
  // mqtt.publish(mqtt_temp_discovery_topic, jsonBuffer);

  // //publish discovery message to Home Assistant for battery sensor
  // StaticJsonDocument<256> batteryDoc;
  // batteryDoc["name"] = "Basement_Fridge_Battery";
  // batteryDoc["device_class"] = "voltage";
  // batteryDoc["state_topic"] = mqtt_publish_topic_battery;
  // batteryDoc["unique_id"] = "basementfridge_battery";
  // batteryDoc["value_template"] = "{{ value_json.voltage }}";
  // batteryDoc["unit_of_measurement"] = "V";
  // char batteryJsonBuffer[256];
  // serializeJson(batteryDoc, batteryJsonBuffer);
  // mqtt.publish(mqtt_battery_discovery_topic, batteryJsonBuffer);

  // publish temperature status
  StaticJsonDocument<128> tempDoc;
  tempDoc["temperature"] = temperature;
  tempDoc["unit_of_measurement"] = "F";
  tempDoc["state_topic"] = mqtt_publish_topic_temp;
  char tempJsonBuffer[128];
  serializeJson(tempDoc, tempJsonBuffer);
  mqtt.publish(mqtt_publish_topic_temp, tempJsonBuffer);

  // publish battery status
  StaticJsonDocument<128> pubBatteryDoc;
  pubBatteryDoc["voltage"] = batteryVoltage;
  pubBatteryDoc["unit_of_measurement"] = "V";
  pubBatteryDoc["state_topic"] = mqtt_publish_topic_battery;
  char pubBatteryJsonBuffer[128];
  serializeJson(pubBatteryDoc, pubBatteryJsonBuffer);
  mqtt.publish(mqtt_publish_topic_battery, pubBatteryJsonBuffer);

  // turn off LED to indicate setup complete
  digitalWrite(LED, HIGH);

  // deep sleep for 5 mins
  Serial.println("Setup complete, going to sleep for 5 mins...");
  delay(1000);
  esp_sleep_enable_timer_wakeup(60 * 5 * 1000000); // 5 mins
  esp_deep_sleep_start();
  // Note: The ESP32 will reset after waking up from deep sleep
  // and the setup() function will be called again.
}

void loop() {
  if (LEDstatus)
  {digitalWrite(LED, LOW);}
  else
  {digitalWrite(LED, HIGH);}
  delay(1000);
  LEDstatus = !LEDstatus;

  
}

void connectToMQTT() {
  // connect to the mqtt broker
  mqtt.begin(mqtt_address, mqtt_port, network);
  Serial.print("Connecting to MQTT broker..");

  // wait for connection to mqtt broker
  while (!mqtt.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
    Serial.print(".");
    delay(100);
  }
  Serial.print("\n\n");

  // make sure connection was successful
  if (!mqtt.connected()) {
    Serial.println("unable to connect to MQTT broker");
    return;
  }
  else
  {
    Serial.println("Successfully connected to MQTT broker!");
  }

}