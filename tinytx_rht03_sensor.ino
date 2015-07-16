/*
Code for measuring air temperature and relative humidity using a RHT03 connected to t TinyTX3.
By Anders S. Tøgern, heavily inspired by Nathan Chantrell's TinyTX3: https://github.com/nathanchantrell/TinyTX

Licenced under the Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0) licence:
http://creativecommons.org/licenses/by-sa/3.0/
*/

#include <SoftwareSerial.h>
#include <JeeLib.h> // https://github.com/jcw/jeelib
#include <dht.h>

#define myNodeID         11           // RF12 node ID in the range 1-30
#define network          210          // RF12 Network group
#define freq             RF12_433MHZ  // Frequency of RFM12B module

#define USE_ACK                // Enable ACKs, comment out to disable
#define RETRY_PERIOD     5     // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT      5     // Maximum number of times to retry
#define ACK_TIME         10    // Number of milliseconds to wait for an ack

#define MINUTE           60 * 1000

#define RHT03_POWER_PIN  10
#define RHT03_PIN        9

typedef struct {
  int temp;	// Temperature reading
  int humidity; // Humidity reading
  int supplyV;	// Supply voltage
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

      Sleepy::loseSomeTime(RETRY_PERIOD * 1000);     // If no ack received wait and try again
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

//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------
long readVcc() {
  bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
  long result;
  // Read 1.1V reference against Vcc
  #if defined(__AVR_ATtiny84__) 
    ADMUX = _BV(MUX5) | _BV(MUX0); // For ATtiny84
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // For ATmega328
  #endif 
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate Vcc in mV
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
  return result;
}

void setup() {
  PRR = bit(PRTIM1); // only keep timer 0 going
  
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
  
  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  rf12_sleep(0);                          // Put the RFM12 to sleep
  
  pinMode(RHT03_POWER_PIN, OUTPUT);
  digitalWrite(RHT03_POWER_PIN, LOW);
}

void loop() {
  digitalWrite(RHT03_POWER_PIN, HIGH);
  
  Sleepy::loseSomeTime(2000);
  
  while (DHT.read22(RHT03_PIN) != DHTLIB_OK) {
    Sleepy::loseSomeTime(500);
  }
  
  tinytx.temp = DHT.temperature * 100;
  tinytx.humidity = DHT.humidity * 100;
  
  digitalWrite(RHT03_POWER_PIN, LOW);
  
  tinytx.supplyV = readVcc(); // Get supply voltage
  
  rfwrite(); // Send data via RF
  
  for (int i=0; i<5; i++) //Sleep for 5 minutes (5 * 1 minute)
    Sleepy::loseSomeTime(MINUTE); //JeeLabs power save function: enter low power mode for 60 seconds (valid range 16-65000 ms)
}
