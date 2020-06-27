#include <ESP8266WiFi.h>
//#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#define DEV_NAME    "Ebay RGB Light"
#define SW_VERSION  "1.0.0.0"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

// Definitions
#define RED_PIN 14
#define GREEN_PIN 12
#define BLUE_PIN 13

#define INTERVAL 5000 // 5 sec delay between publishing

// Your WiFi credentials.
// Set password to "" for open networks.
const char* ssid = "XXXXXX";
const char* pass = "XXXXX";
const char* mqtt_server = "XXXXXXXS";
const char* mqtt_login = "XXXXXXX";
const char* mqtt_pw = "XXXXXXX";

// MQTT libraries
WiFiClient espClient;
PubSubClient client(espClient);

// Global Vars
unsigned char redPwmValue   = 255;
unsigned char greenPwmValue = 255;
unsigned char bluePwmValue  = 255;

unsigned long previousMillis = 0;
unsigned char newRssiPercent = 0;
unsigned char currentRssiPercent = 0;

// MQTT topics                                                // Expected format
#define MQTT_TOPIC_POWER_CMD   "RgbLightBulb/Power/Cmd"       // ON/OFF
#define MQTT_TOPIC_POWER_STATE "RgbLightBulb/Power/State"
#define MQTT_TOPIC_WHITE_CMD   "RgbLightBulb/White/Cmd"       // ON/OFF
#define MQTT_TOPIC_WHITE_STATE "RgbLightBulb/White/State"
#define MQTT_TOPIC_COLOR_CMD   "RgbLightBulb/Color/Cmd"       // RRR,GGG,BBB
#define MQTT_TOPIC_COLOR_STATE "RgbLightBulb/Color/State"

#define MQTT_TOPIC_WIFI_RSSI "RgbLightBulb/Wifi/RSSI"
#define MQTT_TOPIC_WIFI_SS   "RgbLightBulb/Wifi/SS"           // 0-100, scaled RSSI
#define MQTT_TOPIC_WIFI_SSID "RgbLightBulb/Wifi/SSID"
#define MQTT_TOPIC_WIFI_IP   "RgbLightBulb/Wifi/LocalIP"
#define MQTT_TOPIC_NAME      "RgbLightBulb/Name"
#define MQTT_TOPIC_VERSION   "RgbLightBulb/SwVer"

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

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

//
// Description:
//  Return the quality (Received Signal Strength Indicator)
//  of the WiFi network.
//  Returns a number between 0 and 100 if WiFi is connected.
//  Returns -1 if WiFi is disconnected.
//
unsigned char getQuality() {
  if (WiFi.status() != WL_CONNECTED)
    return 0;
  int dBm = WiFi.RSSI();
  if (dBm <= -100)
    return 0;
  if (dBm >= -50)
    return 100;
  return 2 * (dBm + 100);
}

//
// Description:
//  Function handles MQTT messages. Parses the command, reactes, 
//  and loops back command in MQTT STATE field.
//
void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if (strcmp(topic, MQTT_TOPIC_POWER_CMD) == 0)
  {
    // Should be expecting "ON" or "OFF"
    if (length >= 2)
    {
      if ((payload[0] == 'O') &&
          (payload[1] == 'N'))
      {
        analogWrite(RED_PIN,   redPwmValue);
        analogWrite(GREEN_PIN, greenPwmValue);
        analogWrite(BLUE_PIN,  bluePwmValue);

        client.publish(MQTT_TOPIC_POWER_STATE, "ON");
      }
      else
      {
        analogWrite(RED_PIN,   0);
        analogWrite(GREEN_PIN, 0);
        analogWrite(BLUE_PIN,  0);

        client.publish(MQTT_TOPIC_POWER_STATE, "OFF");
      }
    }
  }
  else if (strcmp(topic, MQTT_TOPIC_WHITE_CMD) == 0)
  {
    // Should be expecting "ON" or "OFF"
    if (length >= 2)
    {
      if ((payload[0] == 'O') &&
          (payload[1] == 'N'))
      {
        analogWrite(RED_PIN, 255);
        analogWrite(GREEN_PIN, 255);
        analogWrite(BLUE_PIN, 255);

        client.publish(MQTT_TOPIC_WHITE_STATE, "ON");
      }
      else
      {
        analogWrite(RED_PIN, 0);
        analogWrite(GREEN_PIN, 0);
        analogWrite(BLUE_PIN, 0);

        client.publish(MQTT_TOPIC_WHITE_STATE, "OFF");
      }
    }
  }
  else if (strcmp(topic, MQTT_TOPIC_COLOR_CMD) == 0)
  {
    if (length > 0)
    {
      // convert payload to String
      String value = String((char*)payload);    
      Serial.println(value);
  
      // Parse out RGB values, in format RRR,GGG,BBB
      redPwmValue = value.substring(0,3).toInt();
      greenPwmValue = value.substring(4,7).toInt();
      bluePwmValue = value.substring(8,11).toInt();

      analogWrite(RED_PIN,   redPwmValue);
      analogWrite(GREEN_PIN, greenPwmValue);
      analogWrite(BLUE_PIN,  bluePwmValue);

      client.publish(MQTT_TOPIC_COLOR_STATE, (char*) payload);
    }
  }
}

void reconnect() 
{
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266RGBLightBulb";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_login, mqtt_pw))
    {
      Serial.println("Connected to MQTT Server");
      
      // Once connected, Publish connection data over MQTT
      client.publish(MQTT_TOPIC_WIFI_SSID, (char*) WiFi.SSID().c_str());
      client.publish(MQTT_TOPIC_WIFI_IP, (char*) WiFi.localIP().toString().c_str());
      client.publish(MQTT_TOPIC_NAME, DEV_NAME);
      client.publish(MQTT_TOPIC_VERSION, SW_VERSION);
      
      // Sync current state with MQTT
      client.publish(MQTT_TOPIC_POWER_STATE, "ON");
      client.publish(MQTT_TOPIC_WHITE_STATE, "ON");
      client.publish(MQTT_TOPIC_COLOR_STATE, "255,255,255");
      
      // Subscribe to topics we want to receive commands to
      client.subscribe(MQTT_TOPIC_POWER_CMD);
      client.subscribe(MQTT_TOPIC_WHITE_CMD);
      client.subscribe(MQTT_TOPIC_COLOR_CMD);
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");

  // Init output pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // Keep wemos led off
  digitalWrite(LED_BUILTIN, HIGH);

  // Init PWM pins so that the light turns on(white) when first powered on. So if 
  // someone uses the wall switch and powers off the device, it can be used like a 
  // normal light bulb.
  analogWriteRange(256);
  analogWrite(RED_PIN,   redPwmValue);
  analogWrite(GREEN_PIN, greenPwmValue);
  analogWrite(BLUE_PIN,  bluePwmValue);
  Serial.println("Light Bulb turned on");
  
  setup_wifi();
  
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp8266_rgbLightBulb");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Connect to MQTT server
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  // check interval ~5sec
  if(millis() - previousMillis > INTERVAL) 
  {
    // Only send update when RSSI has changed
    newRssiPercent = getQuality();
    if (newRssiPercent != currentRssiPercent)
    {
      currentRssiPercent = newRssiPercent;
      
      // Publish Signal Strength and raw RSSI data to MQTT server
      client.publish(MQTT_TOPIC_WIFI_SS, (char*) String(currentRssiPercent).c_str());
      client.publish(MQTT_TOPIC_WIFI_RSSI, (char*) String(WiFi.RSSI()).c_str());
    }
    previousMillis = millis();
  }
  
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();
}
