# Dacha

Home automation for our 

The hub is a Raspberry Pi tethered to a GSM module, over serial connection. 

It is also connected to a 433mHz RF gateway that receives temperature, humidity readings along with battery condition from multiple sensors running on battery.

Sensors are using HopeRF RFM69 radios, controlled by Arduino compatible microcontrollers. The sensors are BME280, which are highly accurate in reading temperature and humidity.



