#include <Arduino.h>
#include <SPI.h>

// Huzzah
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid     = "*******";
const char* password = "*******";
 
const char* host = "*******";

// OneWire
#include <OneWire.h> 
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2 

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

String convertAddress(DeviceAddress deviceAddress)
{
  String returnValue = "";
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) returnValue += "0";
    returnValue += String(deviceAddress[i], HEX);
  }

  return returnValue;
}
 
void setup() {
  Serial.begin(115200);
  delay(100);
 
 // WiFi
 // *********************
  // We start by connecting to a WiFi network
 
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  // OneWire
   sensors.begin(); 
}
 
int value = 0;
 
void loop() {
  delay(10000);
  ++value;
  uint8_t sensorCount = sensors.getDeviceCount();
  sensors.requestTemperatures();
  float temp;
  String sensorValues = "[";
  for (int i = 0; i < sensorCount ; i++){
      temp = sensors.getTempFByIndex(i);
      Serial.print(i); Serial.print(" temperature is: "); 
      Serial.print(temp);
      DeviceAddress address;
      sensors.getAddress(address, i);

      sensorValues += "{\"sensor\": \"" + convertAddress(address) + "\", \"index\": " + String(i) + ", \"value\": " + String(temp) + "}";
      if (i < sensorCount - 1) sensorValues += ",";
  }

  sensorValues += "]";
  Serial.println("Payload is:");
  Serial.println(sensorValues);

  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 1880;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/watertemp";
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
}
