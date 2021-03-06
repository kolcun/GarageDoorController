

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneButton.h>
#include "credentials.h"

#define MIKEGARAGECONTACT D1
#define DIANEGARAGECONTACT D2
#define MIKEDOORSENSORCLOSEPOSITION D7
#define DIANEDOORSENSORCLOSEPOSITION D0
#define MIKEDOORSENSOROPENPOSITION D5
#define DIANEDOORSENSOROPENPOSITION D6

#define HOSTNAME "GarageController"
#define MQTT_CLIENT_NAME "kolcun/outdoor/garagedoorcontroller"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

const char* overwatchTopic = MQTT_CLIENT_NAME"/overwatch";

char charPayload[50];
String mikeState = "UNKNOWN";
String dianeState = "UNKNOWN";

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
OneButton mikeDoorSensorClosed(MIKEDOORSENSORCLOSEPOSITION, false, false);
OneButton mikeDoorSensorOpen(MIKEDOORSENSOROPENPOSITION, false, false);
OneButton dianeDoorSensorClosed(DIANEDOORSENSORCLOSEPOSITION, false, false);
OneButton dianeDoorSensorOpen(DIANEDOORSENSOROPENPOSITION, false, false);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupOTA();
  setupButtons();
  setupMqtt();
  setupRelays();
  publishStates();

}

void loop() {
  ArduinoOTA.handle();
  if (!pubSubClient.connected()) {
    reconnect();
  }
  mikeDoorSensorClosed.tick();
  mikeDoorSensorOpen.tick();
  dianeDoorSensorClosed.tick();
  dianeDoorSensorOpen.tick();
  pubSubClient.loop();

}

void setupButtons() {
  mikeDoorSensorClosed.attachLongPressStart(mikeDoorClosed);
  mikeDoorSensorClosed.attachLongPressStop(mikeDoorOpening);
  mikeDoorSensorClosed.setPressTicks(300);

  mikeDoorSensorOpen.attachLongPressStart(mikeDoorOpen);
  mikeDoorSensorOpen.attachLongPressStop(mikeDoorClosing);
  mikeDoorSensorOpen.setPressTicks(300);
  
  dianeDoorSensorClosed.attachLongPressStart(dianeDoorClosed);
  dianeDoorSensorClosed.attachLongPressStop(dianeDoorOpening);
  dianeDoorSensorClosed.setPressTicks(300);

  dianeDoorSensorOpen.attachLongPressStart(dianeDoorOpen);
  dianeDoorSensorOpen.attachLongPressStop(dianeDoorClosing);
  dianeDoorSensorOpen.setPressTicks(300);
}

//Door has closed - in down position, not moving
void mikeDoorClosed() {
  mikeState = "close";
  Serial.println("Mike Door Closed");
  publishMikeState();
}

//Door has started opening - in down position, moving up
void mikeDoorOpening(){
  mikeState = "moving-opening";
  Serial.println("Mike Door Opening - moving up");
  publishMikeState();
}

//Door has opened - in up position, not moving.
void mikeDoorOpen() {
  mikeState = "open";
  Serial.println("Mike Door Open");
  publishMikeState();
}

//Door has started closing - in up position, moving down
void mikeDoorClosing(){
  mikeState = "moving-closing";
  Serial.println("Mike Door Closing - moving down");
  publishMikeState();
}

//Door has closed - in down position, not moving
void dianeDoorClosed() {
  dianeState = "close";
  Serial.println("Diane Door Closed");
  publishDianeState();
}

//Door has started opening - in down position, moving up
void dianeDoorOpening(){
  dianeState = "moving-opening";
  Serial.println("Diane Door Opening - moving up");
  publishDianeState();
}

//Door has opened - in up position, not moving.
void dianeDoorOpen() {
  dianeState = "open";
  Serial.println("Diane Door Open");
  publishDianeState();
}

//Door has started closing - in up position, moving down
void dianeDoorClosing(){
  dianeState = "moving-closing";
  Serial.println("Diane Door Closing - moving down");
  publishDianeState();
}

void publishStates() {
  publishMikeState();
  publishDianeState();
}

void publishMikeState() {
  pubSubClient.publish(MQTT_CLIENT_NAME"/mike/state", (uint8_t*) mikeState.c_str(), mikeState.length(), true);
}

void publishDianeState() {
  pubSubClient.publish(MQTT_CLIENT_NAME"/diane/state", (uint8_t*) dianeState.c_str(), dianeState.length(), true);
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

  if (newTopic == MQTT_CLIENT_NAME"/mike/set") {
    //allow opening - if the state is closed
    if (newPayload == "open" && mikeState == "close") {
      triggerMikeGarage();

      //allow closing - if the state is open
    } else if (newPayload == "close" && mikeState == "open") {
      triggerMikeGarage();
    //allow force trigering  
    } else if (newPayload == "force") {
      triggerMikeGarage();
    }
  }

  if (newTopic == MQTT_CLIENT_NAME"/diane/set") {
    //allow opening - if the state is closed
    if (newPayload == "open" && dianeState == "close") {
      triggerDianeGarage();

      //allow closing - if the state is open
    } else if (newPayload == "close" && dianeState == "open" ) {
      triggerDianeGarage();
    //allow force trigering  
    } else if (newPayload == "force") {
      triggerDianeGarage();
    }
  }
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
  digitalWrite(MIKEGARAGECONTACT, LOW);
  delay(250);
  digitalWrite(MIKEGARAGECONTACT, HIGH);
}

void triggerDianeGarage() {
  Serial.println("Trigger Diane Garage");
  digitalWrite(DIANEGARAGECONTACT, LOW);
  delay(250);
  digitalWrite(DIANEGARAGECONTACT, HIGH);
}
