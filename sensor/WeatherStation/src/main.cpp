#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// Wifi manager
#include <DNSServer.h>        //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h> //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

char hostParam[40];

#define GPIO0 0

Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); // pass in a number for the sensor identifier (for your use later)
Adafruit_BME280 bme;                           // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

// READ
// https://www.instructables.com/id/ESP8266-Pro-Tips/
// https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/

void setup(void)
{
  pinMode(GPIO0, INPUT);

  Serial.begin(115200);

  if (tsl.begin())
  {
    Serial.println(F("Found a TSL2591 sensor"));
  }
  else
  {
    Serial.println(F("No sensor found ... check your wiring?"));
    while (1)
      ;
  }

  if (!bme.begin(BME280_ADDRESS_ALTERNATE))
  {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    while (1)
      delay(10);
  }

  /* Configure the sensor */
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
  tsl.setGain(TSL2591_GAIN_MED); // 25x gain
  //tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain

  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)
}

void loop(void)
{
  String host;

  if (digitalRead(GPIO0) == LOW)
  {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    WiFiManagerParameter host_parameter("server", "host", hostParam, 40);
    wifiManager.addParameter(&host_parameter);

    wifiManager.startConfigPortal();
    Serial.println("connected...yeey :)");
    host = host_parameter.getValue();
    Serial.println(host);
  }

  Serial.println(host);

  const size_t capacity = JSON_OBJECT_SIZE(7);
  DynamicJsonDocument doc(capacity);
  // More advanced data read example. Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
  // That way you can do whatever math and comparisons you want!
  // Light
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;

  // Heat, humidity, pressure
  sensors_event_t temp_event, pressure_event, humidity_event;
  bme_temp->getEvent(&temp_event);
  bme_pressure->getEvent(&pressure_event);
  bme_humidity->getEvent(&humidity_event);

  doc["IR"] = ir;
  doc["Full"] = full;
  doc["Visible"] = full - ir;
  doc["Lux"] = tsl.calculateLux(full, ir);
  doc["Temperature"] = (temp_event.temperature - 1) * 9 / 5 + 32;
  doc["Humidity"] = humidity_event.relative_humidity;
  doc["Pressure"] = pressure_event.pressure;

  String sensorValues;
  serializeJson(doc, sensorValues);

  Serial.println(sensorValues);

  WiFiClient client;
  const int httpPort = 1880;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // We now create a URI for the request
  String url = "/weather";
  Serial.print("Requesting URL: ");
  Serial.println(url);

  HTTPClient httpClient;

  String postUrl = "http://" + String(host) + ":" +  String(httpPort) + url;

  httpClient.begin(postUrl);
  httpClient.addHeader("Content-Type", "application/json");
  auto httpCode = httpClient.POST(sensorValues);

  Serial.println(httpCode);
  String payload = httpClient.getString();
  Serial.println(payload); //Print request response payload
  httpClient.end();

  delay(2000);
}