/*LED_Breathing.ino Arduining.com  20 AUG 2015
Using NodeMCU Development Kit V1.0
Going beyond Blink sketch to see the blue LED breathing.
A PWM modulation is made in software because GPIO16 can't
be used with analogWrite().
*/
#include <Arduino.h>
#include <EmonLiteESP.h>

#define LED     D0        // Led in NodeMCU at pin GPIO16 (D0).

#define BRIGHT    350     //max led intensity (1-500)
#define INHALE    2250    //Inhalation time in milliseconds.
#define PULSE     INHALE*1000/BRIGHT
#define REST      1000    //Rest Between Inhalations.
// EmonLiteESP definitions needed
#define ADC_BITS 10
#define REFERENCE_VOLTAGE 3.3
#define CURRENT_RATIO 30

// Instantiate the lib and set its callback
EmonLiteESP power;
unsigned int currentCallback() {
    return analogRead(A0);
}
//----- Setup function. ------------------------
void setup() {
  power.initCurrent(currentCallback, ADC_BITS, REFERENCE_VOLTAGE, CURRENT_RATIO);
  pinMode(LED, OUTPUT);   // LED pin as output.
  Serial.begin(9600);
}
int adcvalue;
//----- Loop routine. --------------------------
void loop() {
  //ramp increasing intensity, Inhalation:
  for (int i=1;i<BRIGHT;i++){
    digitalWrite(LED, LOW);          // turn the LED on.
    delayMicroseconds(i*10);         // wait
    digitalWrite(LED, HIGH);         // turn the LED off.
    delayMicroseconds(PULSE-i*10);   // wait
    delay(0);                        //to prevent watchdog firing.
  }
  //ramp decreasing intensity, Exhalation (half time):
  for (int i=BRIGHT-1;i>0;i--){
    digitalWrite(LED, LOW);          // turn the LED on.
    delayMicroseconds(i*10);          // wait
    digitalWrite(LED, HIGH);         // turn the LED off.
    delayMicroseconds(PULSE-i*10);  // wait
    i--;
    delay(0);                        //to prevent watchdog firing.
  }
  delay(REST);

}
