#include <RFM69.h> 
#include <RFM69_ATC.h>
#include <RFM69_OTA.h> 
#include <LowPower.h> 
#include <SparkFunBME280.h>
#include <SPIFlash.h>    
#include <SPI.h>

#define NODEID            42
#define NETWORKID         100  
#define GATEWAYID         1

#define FREQUENCY         RF69_433MHZ
#define ENCRYPTKEY        "sampleEncryptKey" 

#define IS_RFM69HW 
#define SENDEVERYXLOOPS   8 

#define ENABLE_ATC    
#define ATC_RSSI -80

#define LED_PIN             9
#define BATTERY_PIN         A7
#define USE_MOSFET          0
#define BATTERY_ENABLE_PIN  A3

#define FLASH_SS            8

// #define SERIAL_EN      //uncomment this line to enable serial IO debug messages, leave out if you want low power
#ifdef SERIAL_EN
  #define SERIAL_BAUD   115200
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

#define SLEEP_COUNT   32     // 32

// Sense VBAT_COND signal (when powered externally should read ~3.25v/3.3v (1000-1023), when external power is cutoff it should start reading around 2.85v/3.3v * 1023 ~= 883 (ratio given by 10k+4.7K divider from VBAT_COND = 1.47 multiplier)
#define BATTERY_RATIO  (0.00322 * 1.49) // >>> fine tune this parameter to match your voltage when fully charged. details on how this works: https://lowpowerlab.com/forum/index.php/topic,1206.0.html


#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif


typedef struct {
  int nodeId; 
  double temperature; 
  double pressure; 
  double humidity;   
  unsigned int battery;
} Payload;

void setupBase();
void setupBME();
void setupFlash();
void setupRadio();
void blink(byte, int);
void sendData();