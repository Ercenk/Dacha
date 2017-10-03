#include <RFM69.h>    //get it here: https://github.com/LowPowerLab/RFM69
#include <RFM69_ATC.h>//get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h> //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>      //included with Arduino IDE (www.arduino.cc)
#include <LowPower.h> //get library from: https://github.com/LowPowerLab/LowPower
                      //writeup here: http://www.rocketscream.com/blog/2011/07/04/lightweight-low-power-arduino-library/
#include <DHT.h>

//****************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! ****
//****************************************************************************************************************
#define NODEID            55    //unique for each node on same network
#define NETWORKID         100  //the same on all nodes that talk to each other
#define GATEWAYID         1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ
//#define FREQUENCY_EXACT 917000000
#define IS_RFM69HW        //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ENCRYPTKEY        "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define SENDEVERYXLOOPS   8 //each loop sleeps 8 seconds, so send status message every this many sleep cycles (default "4" = 32 seconds)
//*********************************************************************************************
#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI -75
//*********************************************************************************************

#define DHTPIN 4 // D3
#define LED           9  // Moteinos have LEDs on D9
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define DUPLICATE_INTERVAL 9000 //avoid duplicates in 5 minutes
#define TEMPDELTA 0.04
#define HUMIDITYDELTA 0.04
#define BLINK_EN         //uncomment to make LED flash when messages are sent, leave out if you want low power

#define SERIAL_EN      //uncomment this line to enable serial IO debug messages, leave out if you want low power
#ifdef SERIAL_EN
  #define SERIAL_BAUD   115200
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

DHT dht(DHTPIN, DHTTYPE);

char sendBuf[61];
byte sendLen;
byte sendLoops=0;
unsigned long tempLastSent=0; 
unsigned long now = 0, time=0, lastSend = 0, temp = 0;
unsigned long previous = 0;

float humidity=0, prevHumidity=0;
float temperature=0, prevTemperature=0;

typedef struct {
  int nodeId; 
  float temperature; 
  float heatIndex; 
  float humidity;   
} Payload;
Payload payload;

void setup() {
#ifdef SERIAL_EN
  Serial.begin(SERIAL_BAUD);
#endif  

  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);

#ifdef FREQUENCY_EXACT
  radio.setFrequency(FREQUENCY_EXACT); //set frequency to some custom frequency
#endif

 radio.setPowerLevel(29);
#ifdef ENABLE_ATC
  radio.enableAutoPower(ATC_RSSI);
#endif
  radio.sendWithRetry(GATEWAYID, "START", 6);

  radio.sleep();

  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGln(buff);
  dht.begin();
}

void loop() {
  now = millis();
  humidity = dht.readHumidity();
  temperature = dht.readTemperature(true);
  DEBUGln(temperature);
  if (isnan(humidity) || isnan(temperature)) {
    DEBUG("Failed to read from DHT sensor!");
    return;
  }
 
  bool send = false;
  send = (abs(temperature - prevTemperature) >= TEMPDELTA) || 
        (abs(humidity - prevHumidity) >= HUMIDITYDELTA) || 
        (time-tempLastSent > DUPLICATE_INTERVAL);
   
  if (send)
  {
    float heatIndex = dht.computeHeatIndex(temperature, humidity);
    tempLastSent = time; //save timestamp of event
    DEBUG("sending: ");
    DEBUGln(heatIndex);
    payload.nodeId = NODEID;
    payload.temperature = temperature; 
    payload.heatIndex = heatIndex; 
    payload.humidity = humidity; 
    
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&payload), sizeof(payload)))
    {
     DEBUGln("\n..OK");
     #ifdef BLINK_EN
       Blink(LED,3);
     #endif
    }
    else DEBUGln("\n..NOK");
    radio.sleep();
  }

  prevTemperature = temperature;
  prevHumidity = humidity;
  
  time = time + 8000 + millis()-now + 480; //correct millis() resonator drift, may need to be tweaked to be accurate
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  DEBUGln("WAKEUP");
  
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

