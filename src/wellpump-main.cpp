/*
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
// Time between power readings, makign this under 1second main loop is about 1 sec
#define MEASUREMENT_INTERVAL    700

//wifi and mqtt connections
//const char* ssid = "toods";
//const char* password = "forest2home";
const char* ssid = "toods";
const char* password = "forest2home";
//const char* mqtt_server = "mqtt.thingspeak.com";
const char* mqtt_server = "10.1.1.4";
const char* mqtt_user = "wellpump";
const char* mqtt_password = "unset";
const char* mqtt_ch_current = "Sensor/wellsensor/current";
const char* mqtt_ch_temp_humi = "Sensor/wellsensor/temp_humi";



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

//global sensors variable
double I;
long milliamps;
long temperature;
long humidity;


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
    if (client.connect("WellSensor-1",mqtt_user,mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_server, "connected");
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

        //double current = power.getCurrent(SAMPLES_X_MEASUREMENT);
        I = power.getCurrent(SAMPLES_X_MEASUREMENT);

        Serial.print(F("[ENERGY] Current now: "));
        Serial.print(long(I * 1000));
        Serial.println(F("milliamps"));

        last_check = millis();

    }
  }

void get_temperature_humidity() {
  temperature=12;
  humidity=30;
}

//----- Setup function. ------------------------
void setup() {

  int i = 0; // counter for wasting readings
  int j = 70 ; // how many to waste
  power.initCurrent(currentCallback, ADC_BITS, REFERENCE_VOLTAGE, CURRENT_RATIO);
  pinMode(LED, OUTPUT);   // LED pin as output.
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(cbMqttRcvd);

  //Waste first 100 readings and wait for current to turn on
  Serial.println("Calibrating DC on sensor  ");
  while (i < j ) {
    i++;
    powerMonitorLoop();
    Serial.print(":");Serial.print((String)I);
    delay(900);
  }

}


//----- Loop routine. --------------------------
void loop() {

  // This is for counting the loops in 1 sec intervals
  static unsigned int loopcounter=0;
  static unsigned int wakeup=1;
  static unsigned int publish_current=0;
  static unsigned long current_on=0;
  static unsigned long max_current_ma=0;

  //read temperature and humidity every loop in global vars
  get_temperature_humidity();

  //read power and convert I into milliamps cast into integer every loop
  powerMonitorLoop();
  milliamps=I*1000;
  //count how long current is on if its greater than 2 amps and what the max is
  if ( milliamps > 1000 ){
    current_on++; // current on time in seconds
    if ( milliamps > max_current_ma ) {
      max_current_ma = milliamps;
    }
  } else {
    if ( current_on) {
      publish_current=1;

    }

  }

  if( loopcounter >300  ) {
    wakeup=1;
    loopcounter=0;
  }

  if (  wakeup  || publish_current ) {

    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    if ( publish_current ) {
      snprintf (msg, 75, "On:%ld Maxma:%ld", current_on,max_current_ma);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(mqtt_ch_current, msg);
      current_on=0;
      publish_current=0;
      max_current_ma=0;
    }
    if ( wakeup ) {

      snprintf (msg, 75, "T:%ld H:%ld", temperature,humidity);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(mqtt_ch_temp_humi, msg);
      wakeup=0;
      temperature=0;humidity=0;
    }
  }
  //delay 900 since the powerloop will take 100ms
  delay(900);

}
