#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Wifi manager
#include <DNSServer.h>        
#include <ESP8266WebServer.h> 
#include <WiFiManager.h>     

////// PIN settings
#define WIFICONFIG 0 // use for WiFiManager captive portal setup, 
#define PIN2 2 // Can use as an output, used to detect boot mode
#define PIN4 4 // SDA - used by Adafruit sensors
#define PIN5 5  // SCL - used by adafruit sensors
#define TIMERDONE 12 // Timer circuit done
#define WSPEED 13 // WIND speed
#define RAIN 14  // Rain
#define PIN15 15 // Can use as an output, used to detect boot mode
#define PIN16 16 // Not using, it is used for deep-sleep

const byte WDIR = A0;


////// Device identification
const String FW_VERSION = "1004";
const String DeviceType = "weather";


/////// Sensors
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); 
Adafruit_BME280 bme;                          
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

///////// Wind and rain
// Using from https://github.com/sparkfun/Wimp_Weather_Station/blob/master/Wimp_Weather_Station.ino

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
long lastSecond; //The millis counter to see when a second rolls by
unsigned int minutesSinceLastReset; //Used to reset variables after 24 hours. Imp should tell us when it's midnight, this is backup.
byte seconds; //When it hits 60, increase the current minute
byte seconds_2m; //Keeps track of the "wind speed/dir avg" over last 2 minutes array of data
byte minutes; //Keeps track of where we are in various arrays of data
byte minutes_10m; //Keeps track of where we are in wind gust/dir over last 10 minutes array of data

long lastWindCheck = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;

//We need to keep track of the following variables:
//Wind speed/dir each update (no storage)
//Wind gust/dir over the day (no storage)
//Wind speed/dir, avg over 2 minutes (store 1 per second)
//Wind gust/dir over last 10 minutes (store 1 per minute)
//Rain over the past hour (store 1 per minute)
//Total rain over date (store one per day)

byte windspdavg[120]; //120 bytes to keep track of 2 minute average
#define WIND_DIR_AVG_SIZE 120
int winddiravg[WIND_DIR_AVG_SIZE]; //120 ints to keep track of 2 minute average
float windgust_10m[10]; //10 floats to keep track of largest gust in the last 10 minutes
int windgustdirection_10m[10]; //10 ints to keep track of 10 minute max
volatile float rainHour[60]; //60 floating numbers to keep track of 60 minutes of rain

//These are all the weather values that wunderground expects:
int winddir; // [0-360 instantaneous wind direction]
float windspeedmph; // [mph instantaneous wind speed]
float windgustmph; // [mph current wind gust, using software specific time period]
int windgustdir; // [0-360 using software specific time period]
float windspdmph_avg2m; // [mph 2 minute average wind speed mph]
int winddir_avg2m; // [0-360 2 minute average wind direction]
float windgustmph_10m; // [mph past 10 minutes wind gust mph ]
int windgustdir_10m; // [0-360 past 10 minutes wind gust direction]

float rainin; // [rain inches over the past hour)] -- the accumulated rainfall in the past 60 min
volatile float dailyrainin; // [rain inches so far today in local time]
//float baromin = 30.03;// [barom in] - It's hard to calculate baromin locally, do this in the agent

//float dewptf; // [dewpoint F] - It's hard to calculate dewpoint locally, do this in the agent

// volatiles are subject to modification by IRQs
volatile unsigned long raintime, rainlast, raininterval, rain;

///////// Wi-Fi stuff
WiFiManager wifiManager;
WiFiClient client;

String host;
String portString;
int port;
String dataPath;
String firmwareVersionCheckPath;
String firmwareDownloadPath;

char hostParam[40];
char portParam[40];
char dataPathParam[40];
char firmwareVersionPathParam[40];
char firmwareDownloadPathParam[40];

WiFiManagerParameter host_parameter("server", "host", hostParam, 40);
WiFiManagerParameter port_parameter("port", "1880", portParam, 40);
WiFiManagerParameter dataPath_parameter("dataPath", "weather", dataPathParam, 40);
WiFiManagerParameter firmwareVersion_parameter("firmwareVersion", "firmwareVersion", firmwareVersionPathParam, 40);
WiFiManagerParameter firmwareDownload_parameter("firmwareDownload", "firmwareDownload", firmwareDownloadPathParam, 40);


// READ
// https://www.instructables.com/id/ESP8266-Pro-Tips/
// https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/

void setupFileSystem()
{
  if (LittleFS.begin())
  {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");

        size_t size = configFile.size();
        if (size > 1024)
        {
          Serial.println("Config file size is too large");
          return;
        }

        Serial.print("Size is: ");
        Serial.println(size);
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        StaticJsonDocument<200> doc;
        auto error = deserializeJson(doc, buf.get());
        if (error)
        {
          Serial.println("Failed to parse config file");
          return;
        }

        const char *hostBuffer = doc["host"];
        host = hostBuffer;

        const char *portBuffer = doc["port"];
        portString = portBuffer;
        port = portString.toInt();
        if (port == 0)
        {
          Serial.println("Cannot parse port number.");
          port = 1880;
        }

        const char *dataPathBuffer = doc["dataPath"];
        dataPath = dataPathBuffer;

        const char *firmwareVersionBuffer = doc["firmwareVersion"];
        firmwareVersionCheckPath = firmwareVersionBuffer;

        const char *firmwareDownloadBuffer = doc["firmwareDownload"];
        firmwareDownloadPath = firmwareDownloadBuffer;

        Serial.print("File is: ");
        Serial.println(host);

        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
}

void rainIRQ()
// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
{
	raintime = millis(); // grab current time
	raininterval = raintime - rainlast; // calculate interval between this and last event

	if (raininterval > 10) // ignore switch-bounce glitches less than 10mS after initial edge
	{
		dailyrainin += 0.011; //Each dump is 0.011" of water
		rainHour[minutes] += 0.011; //Increase this minute's amount of rain

		rainlast = raintime; // set up for next event
	}
}

//Prints the various arrays for debugging
void displayArrays()
{
	//Windgusts in this hour
	Serial.println();
	Serial.print(minutes);
	Serial.print(":");
	Serial.println(seconds);

	Serial.print("Windgust last 10 minutes:");
	for(int i = 0 ; i < 10 ; i++)
	{
		if(i % 10 == 0) Serial.println();
		Serial.print(" ");
		Serial.print(windgust_10m[i]);
	}

	//Wind speed avg for past 2 minutes
	/*Serial.println();
	 Serial.print("Wind 2 min avg:");
	 for(int i = 0 ; i < 120 ; i++)
	 {
	 if(i % 30 == 0) Serial.println();
	 Serial.print(" ");
	 Serial.print(windspdavg[i]);
	 }*/

	//Rain for last hour
	Serial.println();
	Serial.print("Rain hour:");
	for(int i = 0 ; i < 60 ; i++)
	{
		if(i % 30 == 0) Serial.println();
		Serial.print(" ");
		Serial.print(rainHour[i]);
	}

}

//When the imp tells us it's midnight, reset the total amount of rain and gusts
void midnightReset()
{
	dailyrainin = 0; //Reset daily amount of rain

	windgustmph = 0; //Zero out the windgust for the day
	windgustdir = 0; //Zero out the gust direction for the day

	minutes = 0; //Reset minute tracker
	seconds = 0;
	lastSecond = millis(); //Reset variable used to track minutes

	minutesSinceLastReset = 0; //Zero out the backup midnight reset variable
}

//Calculates each of the variables that wunderground is expecting
void calcWeather()
{
	//current winddir, current windspeed, windgustmph, and windgustdir are calculated every 100ms throughout the day

	//Calc windspdmph_avg2m
	float temp = 0;
	for(int i = 0 ; i < 120 ; i++)
		temp += windspdavg[i];
	temp /= 120.0;
	windspdmph_avg2m = temp;

	//Calc winddir_avg2m, Wind Direction
	//You can't just take the average. Google "mean of circular quantities" for more info
	//We will use the Mitsuta method because it doesn't require trig functions
	//And because it sounds cool.
	//Based on: http://abelian.org/vlf/bearings.html
	//Based on: http://stackoverflow.com/questions/1813483/averaging-angles-again
	long sum = winddiravg[0];
	int D = winddiravg[0];
	for(int i = 1 ; i < WIND_DIR_AVG_SIZE ; i++)
	{
		int delta = winddiravg[i] - D;

		if(delta < -180)
			D += delta + 360;
		else if(delta > 180)
			D += delta - 360;
		else
			D += delta;

		sum += D;
	}
	winddir_avg2m = sum / WIND_DIR_AVG_SIZE;
	if(winddir_avg2m >= 360) winddir_avg2m -= 360;
	if(winddir_avg2m < 0) winddir_avg2m += 360;


	//Calc windgustmph_10m
	//Calc windgustdir_10m
	//Find the largest windgust in the last 10 minutes
	windgustmph_10m = 0;
	windgustdir_10m = 0;
	//Step through the 10 minutes
	for(int i = 0; i < 10 ; i++)
	{
		if(windgust_10m[i] > windgustmph_10m)
		{
			windgustmph_10m = windgust_10m[i];
			windgustdir_10m = windgustdirection_10m[i];
		}
	}

	//Total rainfall for the day is calculated within the interrupt
	//Calculate amount of rainfall for the last 60 minutes
	rainin = 0;
	for(int i = 0 ; i < 60 ; i++)
		rainin += rainHour[i];

}

//Returns the instataneous wind speed
float get_wind_speed()
{
	float deltaTime = millis() - lastWindCheck; //750ms

	deltaTime /= 1000.0; //Covert to seconds

	float windSpeed = (float)windClicks / deltaTime; //3 / 0.750s = 4

	windClicks = 0; //Reset and start watching for new wind
	lastWindCheck = millis();

	windSpeed *= 1.492; //4 * 1.492 = 5.968MPH

	/* Serial.println();
	 Serial.print("Windspeed:");
	 Serial.println(windSpeed);*/

	return(windSpeed);
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(int pinToRead)
{
	byte numberOfReadings = 8;
	unsigned int runningValue = 0;

	for(int x = 0 ; x < numberOfReadings ; x++)
		runningValue += analogRead(pinToRead);
	runningValue /= numberOfReadings;

	return(runningValue);
}

int get_wind_direction()
// read the wind direction sensor, return heading in degrees
{
	unsigned int adc;

	adc = averageAnalogRead(WDIR); // get the current reading from the sensor

	// The following table is ADC readings for the wind direction sensor output, sorted from low to high.
	// Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
	// Note that these are not in compass degree order! See Weather Meters datasheet for more information.

	if (adc < 380) return (113);
	if (adc < 393) return (68);
	if (adc < 414) return (90);
	if (adc < 456) return (158);
	if (adc < 508) return (135);
	if (adc < 551) return (203);
	if (adc < 615) return (180);
	if (adc < 680) return (23);
	if (adc < 746) return (45);
	if (adc < 801) return (248);
	if (adc < 833) return (225);
	if (adc < 878) return (338);
	if (adc < 913) return (0);
	if (adc < 940) return (293);
	if (adc < 967) return (315);
	if (adc < 990) return (270);
	return (-1); // error, disconnected?
}

//Reports the weather string to the Imp
void reportWeather()
{
	calcWeather(); //Go calc all the various sensors

	Serial.print("$,winddir=");
	Serial.print(winddir);
	Serial.print(",windspeedmph=");
	Serial.print(windspeedmph, 1);
	Serial.print(",windgustmph=");
	Serial.print(windgustmph, 1);
	Serial.print(",windgustdir=");
	Serial.print(windgustdir);
	Serial.print(",windspdmph_avg2m=");
	Serial.print(windspdmph_avg2m, 1);
	Serial.print(",winddir_avg2m=");
	Serial.print(winddir_avg2m);
	Serial.print(",windgustmph_10m=");
	Serial.print(windgustmph_10m, 1);
	Serial.print(",windgustdir_10m=");
	Serial.print(windgustdir_10m);
	Serial.print(",rainin=");
	Serial.print(rainin, 2);
	Serial.print(",dailyrainin=");
	Serial.print(dailyrainin, 2);

#ifdef LIGHTNING_ENABLED
	Serial.print(",lightning_distance=");
	Serial.print(lightning_distance);
#endif

	Serial.print(",");
	Serial.println("#,");

	//Test string
	//Serial.println("$,winddir=270,windspeedmph=0.0,windgustmph=0.0,windgustdir=0,windspdmph_avg2m=0.0,winddir_avg2m=12,windgustmph_10m=0.0,windgustdir_10m=0,humidity=998.0,tempf=-1766.2,rainin=0.00,dailyrainin=0.00,-999.00,batt_lvl=16.11,light_lvl=3.32,#,");
}

void wspeedIRQ()
// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
{
	if (millis() - lastWindIRQ > 10) // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
	{
		lastWindIRQ = millis(); //Grab the current time
		windClicks++; //There is 1.492MPH for each click per second.
	}
}

void  setupWeather(){
  	pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
	pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor

	pinMode(WDIR, INPUT);

  	seconds = 0;
	lastSecond = millis();

	// attach external interrupt pins to IRQ functions
	attachInterrupt(0, rainIRQ, FALLING);
	attachInterrupt(1, wspeedIRQ, FALLING);

	// turn on interrupts
	interrupts();

	Serial.println("Wimp Weather Station online!");
	reportWeather();
}

void getWindAndRain()
{
  	//Keep track of which minute it is
	if(millis() - lastSecond >= 1000)
	{
		lastSecond += 1000;

		//Take a speed and direction reading every second for 2 minute average
		if(++seconds_2m > 119) seconds_2m = 0;

		//Calc the wind speed and direction every second for 120 second to get 2 minute average
		windspeedmph = get_wind_speed();
		winddir = get_wind_direction();
		windspdavg[seconds_2m] = (int)windspeedmph;
		winddiravg[seconds_2m] = winddir;
		//if(seconds_2m % 10 == 0) displayArrays();

		//Check to see if this is a gust for the minute
		if(windspeedmph > windgust_10m[minutes_10m])
		{
			windgust_10m[minutes_10m] = windspeedmph;
			windgustdirection_10m[minutes_10m] = winddir;
		}

		//Check to see if this is a gust for the day
		//Resets at midnight each night
		if(windspeedmph > windgustmph)
		{
			windgustmph = windspeedmph;
			windgustdir = winddir;
		}

		//If we roll over 60 seconds then update the arrays for rain and windgust
		if(++seconds > 59)
		{
			seconds = 0;

			if(++minutes > 59) minutes = 0;
			if(++minutes_10m > 9) minutes_10m = 0;

			rainHour[minutes] = 0; //Zero out this minute's rainfall amount
			windgust_10m[minutes_10m] = 0; //Zero out this minute's gust

			minutesSinceLastReset++; //It's been another minute since last night's midnight reset
		}
	}

  	//If we go for more than 24 hours without a midnight reset then force a reset
	//24 hours * 60 mins/hr = 1,440 minutes + 10 extra minutes. We hope that Imp is doing it.
	if(minutesSinceLastReset > (1440 + 10))
	{
		midnightReset(); //Reset a bunch of variables like rain and daily total rain
		//Serial.print("Emergency midnight reset");
	}

}

void setup(void)
{
  Serial.begin(115200);

  pinMode(WIFICONFIG, INPUT);

  setupFileSystem();

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

void check_version_and_update()
{
  String mac = WiFi.macAddress();

  String fwCheckUrl = "http://" + host + ":" + String(port) + "/" + firmwareVersionCheckPath + "?devicetype=" + DeviceType + "&version=" + FW_VERSION;

  Serial.println("Checking for firmware updates.");
  Serial.print("MAC address: ");
  Serial.println(mac);
  Serial.print("Firmware version URL: ");
  Serial.println(fwCheckUrl);

  HTTPClient httpClient;
  httpClient.begin(client, fwCheckUrl);
  int httpCode = httpClient.GET();
  if (httpCode == 200)
  {
    String newFWVersion = httpClient.getString();

    Serial.print("Current firmware version: ");
    Serial.println(FW_VERSION);
    Serial.print("Available firmware version: ");
    Serial.println(newFWVersion);
    int newVersion = newFWVersion.toInt();
    int currentVersion = FW_VERSION.toInt();

    if (newVersion > currentVersion)
    {
      Serial.println("Preparing to update.");

      String fwGetUrl = "http://" + host + ":" + String(port) + "/" + firmwareDownloadPath + "?devicetype=" + DeviceType + "&version=" + FW_VERSION;
      Serial.println(fwGetUrl);

      t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwGetUrl);
      switch (ret)
      {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
      }
    }
  }
}

void loop(void)
{
  Serial.print("Host is: ");
  Serial.println(host);
  if (digitalRead(WIFICONFIG) == LOW || host == "")
  {
    wifiManager.addParameter(&host_parameter);
    wifiManager.addParameter(&port_parameter);
    wifiManager.addParameter(&dataPath_parameter);
    wifiManager.addParameter(&firmwareVersion_parameter);
    wifiManager.addParameter(&firmwareDownload_parameter);

    wifiManager.startConfigPortal();
    Serial.println("connected...yeey :)");

    host = host_parameter.getValue();
    Serial.println(host);

    portString = port_parameter.getValue();
    Serial.println(portString);

    dataPath = dataPath_parameter.getValue();
    Serial.println(dataPath);

    firmwareVersionCheckPath = firmwareVersion_parameter.getValue();
    Serial.println(firmwareVersionCheckPath);

    firmwareDownloadPath = firmwareDownload_parameter.getValue();
    Serial.println(firmwareDownloadPath);

    StaticJsonDocument<200> doc;

    doc["host"] = host;
    doc["port"] = portString;
    doc["dataPath"] = dataPath;
    doc["firmwareVersion"] = firmwareVersionCheckPath;
    doc["firmwareDownload"] = firmwareDownloadPath;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("Failed to open config file for writing");
      return;
    }

    serializeJson(doc, configFile);
  }

  Serial.println(host);

  const size_t capacity = JSON_OBJECT_SIZE(8);
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
  doc["firmwareVersion"] = FW_VERSION.toInt();

  String sensorValues;
  serializeJson(doc, sensorValues);

  Serial.println(sensorValues);

  if (!client.connect(host, port))
  {
    Serial.println("connection failed");
    return;
  }

  // We now create a URI for the request
  String url = "/weather";
  Serial.print("Requesting URL: ");
  Serial.println(url);

  HTTPClient httpClient;

  String postUrl = "http://" + String(host) + ":" + String(port) + url;

  httpClient.begin(client, postUrl);
  httpClient.addHeader("Content-Type", "application/json");
  auto httpCode = httpClient.POST(sensorValues);

  Serial.println(httpCode);
  String payload = httpClient.getString();
  Serial.println(payload); //Print request response payload
  httpClient.end();

  check_version_and_update();

  delay(2000);
}