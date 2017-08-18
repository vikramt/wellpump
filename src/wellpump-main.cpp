/*LED_Breathing.ino Arduining.com  20 AUG 2015
Using NodeMCU Development Kit V1.0
Going beyond Blink sketch to see the blue LED breathing.
A PWM modulation is made in software because GPIO16 can't
be used with analogWrite().
*/

#include <Arduino.h>
#include <EmonLiteESP.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#define LED     D0        // Led in NodeMCU at pin GPIO16 (D0).
// LED breathing definitions
#define BRIGHT    350     //max led intensity (1-500)
#define INHALE    2250    //Inhalation time in milliseconds.
#define PULSE     INHALE*1000/BRIGHT
#define REST      1000    //Rest Between Inhalations.

// EmonLiteESP definitions needed
#define ADC_BITS 10
#define REFERENCE_VOLTAGE 3.3
#define CURRENT_RATIO 30
#define MAINS_VOLTAGE  230

// MQTT server definitions
#define mqtturl io.adafruit.com
#define mqttusername viktaraf
#define mqttkey aec055e3559542aa94fbb217e3e45840
#define mqtttopic firstone

// Number of samples each time you measure
#define SAMPLES_X_MEASUREMENT   1000
// Time between readings, this is not specific of the library but on this sketch
#define MEASUREMENT_INTERVAL    10000


// Instantiate the lib and set its callback
EmonLiteESP power;
unsigned int currentCallback() {
    return analogRead(A0);
}

void powerMonitorLoop() {

    static unsigned long last_check = 0;

    if ((millis() - last_check) > MEASUREMENT_INTERVAL) {

        double current = power.getCurrent(SAMPLES_X_MEASUREMENT);

        Serial.print(F("[ENERGY] Power now: "));
        Serial.print(int(current * MAINS_VOLTAGE));
        Serial.println(F("W"));

        last_check = millis();

    }
  }

//----- Setup function. ------------------------
void setup() {
  power.initCurrent(currentCallback, ADC_BITS, REFERENCE_VOLTAGE, CURRENT_RATIO);
  pinMode(LED, OUTPUT);   // LED pin as output.
  Serial.begin(9600);
}


//----- Loop routine. --------------------------
void loop() {
  powerMonitorLoop();
  delay(1);

}
