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


// Number of samples each time you measure
#define SAMPLES_X_MEASUREMENT   1000
// Time between readings, this is not specific of the library but on this sketch
#define MEASUREMENT_INTERVAL    10000

//wifi and mqtt connections
const char* ssid = "toods";
const char* password = "forest2home";
//const char* mqtt_server = "mqtt.thingspeak.com";
//const char* mqtt_server = "10.1.1.4";
const char* mqtt_url = "";


//Instantiate the wifi client and mqtt client
//-----------------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Instantiate the lib and set its callback
//---------------------------------------------
EmonLiteESP power;
unsigned int currentCallback() {
    return analogRead(A0);
}


// Startup wifi network connection
//----------------------
void setup_wifi() {
  delay(20);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Callback for mqtt message received
//-------------------------------
void cbMqttRcvd(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

//Mqtt - continue to reconnect
//----------------------------------
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "WellSensor-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect("WellSensor-1","wellpump","unset")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("channels", "3");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// This just does one reading of power-------------------
//
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
  setup_wifi();
  client.setServer("10.1.1.4", 1883);
  client.setCallback(cbMqttRcvd);
}


//----- Loop routine. --------------------------
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  powerMonitorLoop();
  long now = millis();
  if (now - lastMsg > 20000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("channels", msg);
  }
  delay(1);

}
