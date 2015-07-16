# Basic RHT03 TinyTX3 sensor

Licenced under the Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0) licence: 
http://creativecommons.org/licenses/by-sa/3.0/

This code implements a basic temperature and humidity sensor for the TinyTX3 board.

It is designed to consume a little power as possible by assuming that the sensor's power pin is connected to a digital out pin (10) on the ATTiny84, thus allowing the sensor to only be on and consume power when needed. The output pin of the sensor is connected to digital input pin 10 on the ATTiny.

Further, the ATTiny is put into a low power state between measures and measures are taken every five minutes.

The easiest way of connecting the RHT03 sensor to the TinyTX3 is by creating a small breakout board from stripboard as sketched in the accompanying Fritzing file.
