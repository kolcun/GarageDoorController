//TODO
//with only a single sensor at the close position, can only really know if the door is closed or open
//can't use the 'moving' state until I get a second sensor at the top

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "credentials.h"

#define MIKEGARAGECONTACT D1
#define DIANEGARAGECONTACT D2
#define OPEN 1
#define CLOSED 0
#define HOSTNAME "GarageController"
#define MQTT_CLIENT_NAME "kolcun/outdoor/garagedoorcontroller"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

const char* overwatchTopic = MQTT_CLIENT_NAME"/overwatch";

char charPayload[50];
String mikeState = "UNKNOWN";
String dianeState = "UNKNOWN";

//const byte interruptPin = D6;
//volatile byte interruptCounter = 0;
//int numberOfInterrupts = 0;
//long debouncing_time = 100; //Debouncing Time in Milliseconds
//volatile unsigned long last_micros;

//void ICACHE_RAM_ATTR handleInterrupt();

////1=open 0=closed
//int mikeSensorState = -1;
//bool interruptOccured = false;

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

//  pinMode(interruptPin, INPUT);
//  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, CHANGE);

  setupOTA();
  setupMqtt();
  setupRelays();
  determineInitialState();

  //publish initial states
  pubSubClient.publish(MQTT_CLIENT_NAME"/mike/state", mikeState.c_str());
  pubSubClient.publish(MQTT_CLIENT_NAME"/diane/state", dianeState.c_str());

}

void loop() {
  ArduinoOTA.handle();
  if (!pubSubClient.connected()) {
    reconnect();
  }
  pubSubClient.loop();

//  if(interruptOccured){
//    interruptOccured = false;
//    publishStates();
//  }

  //watch for sensor changes
  //update states based on sensor changes

  //  if (interruptCounter > 0) {
  //
  //    interruptCounter--;
  //    numberOfInterrupts++;
  //
  //    Serial.print("An interrupt has occurred. Total: ");
  //    Serial.println(numberOfInterrupts);
  //  }

}

//void ICACHE_RAM_ATTR handleInterrupt() {
//  if ((long)(micros() - last_micros) >= debouncing_time * 1000) {
//    //    garageClosed();
//    Serial.println("interrupt");
//    interruptOccured = true;
//    if (mikeSensorState == OPEN) {
//      mikeSensorState = CLOSED;
//      mikeState = "closed";
//    } else if (mikeSensorState == CLOSED) {
//      mikeSensorState = OPEN;
//      mikeState = "open";
//    }
//    last_micros = micros();
//  }
//
//}


void determineInitialState() {
  dianeState = "closed";
  mikeState = "closed";
//  mikeSensorState = digitalRead(interruptPin);
//  if(mikeSensorState == CLOSED){
//    mikeState = "closed";
//  }else if (mikeSensorState == OPEN){
//    mikeState = "open";
//  }
  
}

void publishStates() {
  pubSubClient.publish(MQTT_CLIENT_NAME"/mike/state", mikeState.c_str());
  pubSubClient.publish(MQTT_CLIENT_NAME"/diane/state", dianeState.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  Serial.println();
  newPayload.toCharArray(charPayload, newPayload.length() + 1);


  //temporary - in place of sensors
  if (newTopic == MQTT_CLIENT_NAME"/mike/sensoropen") {
    mikeState = "open";
  }
  if (newTopic == MQTT_CLIENT_NAME"/mike/sensorclosed") {
    mikeState = "closed";
  }


  if (newTopic == MQTT_CLIENT_NAME"/mike/set") {
    //allow opening - if the state is closed or moving
    if (newPayload == "open" && ( mikeState == "closed" || mikeState == "moving")) {
      triggerMikeGarage();

      //allow closing - if the state is opene or moving
    } else if (newPayload == "close" && ( mikeState == "open" || mikeState == "moving")) {
      triggerMikeGarage();
    }
  }

  if (newTopic == MQTT_CLIENT_NAME"/diane/set") {
    //allow opening - if the state is closed or moving
    if (newPayload == "open" && ( dianeState == "closed" || dianeState == "moving")) {
      triggerDianeGarage();

      //allow closing - if the state is opene or moving
    } else if (newPayload == "close" && ( dianeState == "open" || dianeState == "moving")) {
      triggerDianeGarage();
    }
  }

  publishStates();
}

void setupRelays() {
  pinMode(MIKEGARAGECONTACT, OUTPUT);
  digitalWrite(MIKEGARAGECONTACT, HIGH);
  pinMode(DIANEGARAGECONTACT, OUTPUT);
  digitalWrite(DIANEGARAGECONTACT, HIGH);

}

void setupOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt() {
  pubSubClient.setServer(MQTT_SERVER, 1883);
  pubSubClient.setCallback(mqttCallback);
  if (!pubSubClient.connected()) {
    reconnect();
  }
}

void reconnect() {
  bool boot = true;
  int retries = 0;
  while (!pubSubClient.connected()) {
    if (retries < 10) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (pubSubClient.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASSWD)) {
        Serial.println("connected");
        // Once connected, publish an announcement...
        if (boot == true) {
          pubSubClient.publish(overwatchTopic, "Rebooted");
          boot = false;
        } else {
          pubSubClient.publish(overwatchTopic, "Reconnected");
        }
        pubSubClient.subscribe(MQTT_CLIENT_NAME"/mike/set");
        pubSubClient.subscribe(MQTT_CLIENT_NAME"/diane/set");
        //temporary
        pubSubClient.subscribe(MQTT_CLIENT_NAME"/mike/sensoropen");
        pubSubClient.subscribe(MQTT_CLIENT_NAME"/mike/sensorclosed");
      } else {
        Serial.print("failed, rc=");
        Serial.print(pubSubClient.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    else {
      ESP.restart();
    }
  }
}

void triggerMikeGarage() {
  Serial.println("Trigger Mike Garage");
  mikeState = "moving";

  digitalWrite(MIKEGARAGECONTACT, LOW);
  delay(250);
  digitalWrite(MIKEGARAGECONTACT, HIGH);
}

void triggerDianeGarage() {
  Serial.println("Trigger Diane Garage");
  dianeState = "moving";

  digitalWrite(DIANEGARAGECONTACT, LOW);
  delay(250);
  digitalWrite(DIANEGARAGECONTACT, HIGH);
}
