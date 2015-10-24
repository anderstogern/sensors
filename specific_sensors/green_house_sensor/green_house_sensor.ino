/*
Code for measuring air temperature and relative humidity using a RHT03 connected to a TinyTX3.

By Anders S. Tøgern, heavily inspired by Nathan Chantrell's TinyTX3: https://github.com/nathanchantrell/TinyTX

Licenced under the Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0) licence:
http://creativecommons.org/licenses/by-sa/3.0/
*/

#include <JeeLib.h> // https://github.com/jcw/jeelib
#include <dht.h>

#define myNodeID         10           // RF12 node ID in the range 1-30
#define network          210          // RF12 Network group
#define freq             RF12_433MHZ  // Frequency of RFM12B module

//#define USE_ACK                // Enable ACKs, comment out to disable
#define RETRY_PERIOD     5     // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT      5     // Maximum number of times to retry
#define ACK_TIME         10    // Number of milliseconds to wait for an ack

#define MINUTE           60 * 1000

#define RHT03_POWER_PIN  10
#define RHT03_PIN         9

#define BATT_PIN          A2
#define VMIN              0.5 // (Min input voltage to regulator according to datasheet or guessing. (?) )
#define VMAX              3 // (Known or desired voltage of full batteries. If not, set to Vlim.)
#define R1                1e6
#define R2                600e3
#define VREF              1.1

typedef struct {
  int temp;	// Temperature reading
  int humidity; // Humidity reading
  int supplyV;	// Supply voltage
  int supplyVPcnt; // Battery level in percent
} Payload;

Payload tinytx;

ISR(WDT_vect) { Sleepy::watchdogEvent(); } // interrupt handler for JeeLabs Sleepy power saving

dht DHT;

// Wait a few milliseconds for proper ACK
#ifdef USE_ACK
  static byte waitForAck() {
  MilliTimer ackTimer;
  while (!ackTimer.poll(ACK_TIME)) {
    if (rf12_recvDone() && rf12_crc == 0 &&
      rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
      return 1;
    }
    return 0;
  }
#endif

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//-------------------------------------------------------------------------------------------------
static void rfwrite(){
  #ifdef USE_ACK
    for (byte i = 0; i <= RETRY_LIMIT; ++i) {  // tx and wait for ack up to RETRY_LIMIT times
      rf12_sleep(-1);              // Wake up RF module
      while (!rf12_canSend())
        rf12_recvDone();
      rf12_sendStart(RF12_HDR_ACK, &tinytx, sizeof tinytx); 
      rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
      byte acked = waitForAck();  // Wait for ACK
      rf12_sleep(0);              // Put RF module to sleep
      if (acked) { return; }      // Return if ACK received

      Sleepy::loseSomeTime(RETRY_PERIOD * 500);     // If no ack received wait and try again
    }
  #else
    rf12_sleep(-1);              // Wake up RF module
    while (!rf12_canSend())
      rf12_recvDone();
    rf12_sendStart(0, &tinytx, sizeof tinytx); 
    rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
    rf12_sleep(0);              // Put RF module to sleep
    return;
  #endif
}

float readVcc() {
  //From MySensors (http://forum.mysensors.org/topic/715/battery-level-measurement)
  bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC

  int sensorValue = 0;

  Sleepy::loseSomeTime(100);
  for (int i = 0; i < 5; i++) {
    sensorValue += analogRead(BATT_PIN);
    Sleepy::loseSomeTime(20);
  }
  
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
  
  sensorValue /= 5;

  float Vmax = ((R1 + R2) / R2) * VREF;
  float Vbat = (Vmax / 1023) * sensorValue;

  return Vbat;
}

void setup() {
  analogReference(INTERNAL);
  
  PRR = bit(PRTIM1); // only keep timer 0 going
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable and power down the ADC to save power
  
  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  rf12_sleep(0);                          // Put the RFM12 to sleep
  
  pinMode(RHT03_POWER_PIN, OUTPUT);
  digitalWrite(RHT03_POWER_PIN, LOW);
  
  pinMode(BATT_PIN, INPUT);
}

void loop() {
  digitalWrite(RHT03_POWER_PIN, HIGH);
  
  Sleepy::loseSomeTime(2000);
  
  while (DHT.read22(RHT03_PIN) != DHTLIB_OK) {
    Sleepy::loseSomeTime(100);
  }
  
  tinytx.temp = DHT.temperature * 100;
  tinytx.humidity = DHT.humidity * 100;

  digitalWrite(RHT03_POWER_PIN, LOW);
  digitalWrite(RHT03_PIN, LOW);          //IMPORTANT: In order to turn RHT03 completely off during sleep its output pin must be pulled low (is pulled high in lib)
  
  float Vbat = readVcc();
  tinytx.supplyV = static_cast<int>(Vbat * 1000); // Get supply voltage
  tinytx.supplyVPcnt = static_cast<int>(((Vbat - VMIN) / (VMAX-VMIN)) * 10000.); //per cent * 100
  
  rfwrite(); // Send data via RF
  
  for (int i=0; i<30; i++) //Sleep for 30 minutes (30 * 1 minute)
    Sleepy::loseSomeTime(MINUTE); //JeeLabs power save function: enter low power mode for 60 seconds (valid range 16-65000 ms)
}

