/*
Using NodeMCU Development Kit V1.0
Going beyond Blink sketch to see the blue LED breathing.
A PWM modulation is made in software because GPIO16 can't
be used with analogWrite().
*/

#include <FS.h>
#include <Arduino.h>
#include <EmonLiteESP.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <EEPROM.h>
#include <SimpleDHT.h>
#include <ArduinoJson.h>

#define LED     D0        // Led in NodeMCU at pin GPIO16 (D0).

// Measured calibrations
// this one is delay at the end of loop so its about 1 sec per loop
// to count the current on time in loops i.e seconds
#define LOOPdelay 880
//this is multiplier to make the MaxAmps read
// what the multimeter measures maybe due to burden resister via voltage divider
#define CurrentCalibration 1.6

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
//send a publish every so often
#define MIN_PUBLISH_SEC 299

// DHT11 settings and instantiate the sensor
int pinDHT11 = D3;
SimpleDHT11 dht11;

static unsigned int wakeup=1;
static  bool resetwifi=0;

//wifi and mqtt connections
const char* ssid = "";
const char* wifipassword = "";
//const char* mqtt_server = "mqtt.thingspeak.com";
//const char* mqtt_server = "10.1.1.4";
char mqtt_server[20] = "";
char mqtt_port[8] = "1883";
//const char* mqtt_user = "wellpump";
char mqtt_user[20]="wellpump";
//const char* mqtt_password = "unset";
char mqtt_password[20] = "";
//const char* mqtt_topic_current = "From/wellpump/current";
char mqtt_topic_current[40] = "From/wellpump/current";
//const char* mqtt_topic_temp_humi = "From/wellpump/temp_humi";
char mqtt_topic_temp_humi[40] = "From/wellpump/temp_humi";
char mqtt_topic_intopic[40] = "To/wellpump/intopic";
char mqtt_topic_help[40] = "From/wellpump/help";


// WifiManager stuff
//
//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


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
byte temperature=254;
byte humidity=254;


// Startup wifi network connection
//----------------------
void setup_wifi() {
  delay(20);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, wifipassword);

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
  static bool id=0;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (!strncmp((char*)payload,"help",4)) {
    char commands[]="commands are - help,restart,resetwifi,status,id";
    Serial.println(commands);
    client.publish(mqtt_topic_help, commands);
    delay(10);

  }
  if (!strncmp((char*)payload,"restart",7)) {
    Serial.println(" Restarting..");
    delay(1000);
    ESP.reset();
  }

  if (!strncmp((char*)payload,"resetwifi",9)) {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    delay(2000);
    Serial.println(" reset settings connect to wifi  AP..");
    delay(1000);
    ESP.reset();
  }

  if (!strncmp((char*)payload,"status",6)) {
    wakeup=1;
    Serial.println("Waking up..");
  }
  // switch on led with id commands
  if (!strncmp((char*)payload,"id",2)) {
    id=!id;
    Serial.println("Inverting id");
  }
  if ( id ) {
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
    String clientId = "WellSensor";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect

    if (client.connect("WellSensor",mqtt_user,mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_server, "connected");
    //  ... and resubscribe
      if ( client.subscribe(mqtt_topic_intopic)) {
        Serial.print("Subscribed topic:");
        Serial.println(mqtt_topic_intopic);
      }else {
        Serial.println("Subscribe failed.");
      }

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// read the dht11
void get_temperature_humidity() {
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(100);
    temperature=254;humidity=254;

  }
}

// This just does one reading of power-------------------
//
void powerMonitorLoop() {

    static unsigned long last_check = 0;

    if ((millis() - last_check) > MEASUREMENT_INTERVAL) {

        //double current = power.getCurrent(SAMPLES_X_MEASUREMENT);
        I = power.getCurrent(SAMPLES_X_MEASUREMENT);

        // Serial.print(F("[ENERGY] Current now: "));
        // Serial.print(long(I * 1000));
        // Serial.println(F("milliamps"));

        last_check = millis();

    }
  }



//----- Setup function. ------------------------
void setup() {

  int i = 0; // counter for wasting readings
  int j = 16 ; // how many to waste
  power.initCurrent(currentCallback, ADC_BITS, REFERENCE_VOLTAGE, CURRENT_RATIO);
  pinMode(LED, OUTPUT);   // LED pin as output.
  EEPROM.begin(512);
  Serial.begin(9600);


  //clean FS, for testing
  //SPIFFS.format();
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(mqtt_topic_current, json["mqtt_topic_current"]);
          strcpy(mqtt_topic_temp_humi, json["mqtt_topic_temp_humi"]);


        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "enter mqtt server ip", mqtt_server, 20, "use ip address");
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 8);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_password("password", "enter mqtt password", mqtt_password, 20);
  WiFiManagerParameter custom_mqtt_topic_current("current", "topic From/wellpump/current", mqtt_topic_current, 40);
  WiFiManagerParameter custom_mqtt_topic_temp_humi("temp_humi", "topic From/wellpump/temp_humi", mqtt_topic_temp_humi, 40);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic_current);
  wifiManager.addParameter(&custom_mqtt_topic_temp_humi);

  //reset settings - for testing
  if ( resetwifi){
    wifiManager.resetSettings();
    resetwifi=0;
  }


  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(200);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...to wifi yup :)");

  //setup_wifi(); // THis is replaced by wifimanager above

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_topic_current, custom_mqtt_topic_current.getValue());
  strcpy(mqtt_topic_temp_humi, custom_mqtt_topic_temp_humi.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_topic_current"] = mqtt_topic_current;
    json["mqtt_topic_temp_humi"] = mqtt_topic_temp_humi;


    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.print("local ip:");
  Serial.println(WiFi.localIP());
  Serial.print("MQtt:");
  Serial.print(mqtt_server);
  Serial.print(mqtt_port);
  Serial.print(mqtt_user);
  Serial.print(mqtt_password);
  Serial.print(mqtt_topic_current);
  Serial.print(mqtt_topic_temp_humi);

  int portnum = atoi(mqtt_port);
  client.setServer(mqtt_server, portnum);
  client.setCallback(cbMqttRcvd);

  //Waste first 100 readings and wait for current to turn on
  Serial.println("Calibrating  DC offset and  sensors  ");
  while (i < j ) {
    digitalWrite(LED, LOW);
    i++;
    powerMonitorLoop();
    //every other time also read dht11
    if ( (i & 0x01) == 0) {
      get_temperature_humidity();
    }
    Serial.print(":");Serial.print((String)I);
    delay(400);
    digitalWrite(LED, HIGH);
    delay(400);
  }

}


//----- Loop routine. --------------------------
void loop() {

  // This is for counting the loops in 1 sec intervals
  static unsigned int loopcounter=MIN_PUBLISH_SEC;

  static unsigned int publish_current=0;
  static unsigned long current_on=0;
  static unsigned long max_current_ma=0;

  //read temperature and humidity every loop in global vars
  get_temperature_humidity();
  loopcounter++;
  //read power and convert I into milliamps cast into integer every loop
  powerMonitorLoop();
  milliamps=I*1000;
  //count how long current is on if its greater than 1 amps and what the max is
  if ( milliamps > 700 ){
    current_on++; // current on time in seconds
    if ( milliamps > max_current_ma ) {
      max_current_ma = milliamps;
    }
  } else {
    if ( current_on > 1 ) {
      publish_current=1;

    }

  }

  if( loopcounter >MIN_PUBLISH_SEC  ) {
    wakeup=1;
    loopcounter=0;
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(1);

  if (  wakeup  || publish_current ) {



    if ( publish_current ) {
      //remove the last 1 second for the loop
      current_on--;
      //use the calibration from the define
      max_current_ma = max_current_ma*CurrentCalibration ;
      snprintf (msg, 75, "On:%ld,MaxAmps:%ld", current_on,max_current_ma);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(mqtt_topic_current, msg);
      current_on=0;
      publish_current=0;
      max_current_ma=0;
    }
    if ( wakeup ) {

      snprintf (msg, 75, "T:%ld,H:%ld", temperature,humidity);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(mqtt_topic_temp_humi, msg);
      wakeup=0;
      temperature=0;humidity=0;
    }
  }
  //delay 900 since the powerloop will take 100ms
  delay(LOOPdelay);

}
