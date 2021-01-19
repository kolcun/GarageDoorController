//TODO
//with only a single sensor at the close position, can only really know if the door is closed or open
//can't use the 'moving' state until I get a second sensor at the top

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneButton.h>
#include "credentials.h"

#define MIKEGARAGECONTACT D1
#define DIANEGARAGECONTACT D2
#define MIKEDOORSENSOR D6
#define DIANEDOORSENSOR D7

#define HOSTNAME "GarageController"
#define MQTT_CLIENT_NAME "kolcun/outdoor/garagedoorcontroller"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWD;

const char* overwatchTopic = MQTT_CLIENT_NAME"/overwatch";

char charPayload[50];
String mikeState = "open";
String dianeState = "open";

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
OneButton mikeDoorSensor(MIKEDOORSENSOR, false, false);
//OneButton dianeDoorSensor(DIANEDOORSENSOR, false, false);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupButtons();
  setupOTA();
  setupMqtt();
  setupRelays();
  publishStates();

}

void loop() {
  ArduinoOTA.handle();
  if (!pubSubClient.connected()) {
    reconnect();
  }
  mikeDoorSensor.tick();
//  dianeDoorSensor.tick();
  pubSubClient.loop();

}

void setupButtons() {
  mikeDoorSensor.attachLongPressStart(mikeDoorSensorClosed);
  mikeDoorSensor.attachLongPressStop(mikeDoorSensorOpened);
  mikeDoorSensor.setPressTicks(300);
//  dianeDoorSensor.attachLongPressStart(dianeDoorSensorClosed);
//  dianeDoorSensor.attachLongPressStop(dianeDoorSensorOpened);
//  dianeDoorSensor.setPressTicks(300);
}

void mikeDoorSensorClosed() {
  mikeState = "closed";
  Serial.println("Mike Door Sensor Closed (door closed)");
  publishStates();
}

void mikeDoorSensorOpened() {
  mikeState = "open";
  Serial.println("Mike Door Sensor Closed (door opening)");
  publishStates();
}

void dianeDoorSensorClosed() {
  dianeState = "closed";
  Serial.println("diane Door Sensor Closed (door closed)");
  publishStates();
}

void dianeDoorSensorOpened() {
  dianeState = "open";
  Serial.println("diane Door Sensor Closed (door opening)");
  publishStates();
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

  if (newTopic == MQTT_CLIENT_NAME"/mike/set") {
    //allow opening - if the state is closed
    if (newPayload == "open" && mikeState == "closed") {
      triggerMikeGarage();

      //allow closing - if the state is open
    } else if (newPayload == "close" && mikeState == "open") {
      triggerMikeGarage();
    }
  }

  if (newTopic == MQTT_CLIENT_NAME"/diane/set") {
    //allow opening - if the state is closed
    if (newPayload == "open" && dianeState == "closed") {
      triggerDianeGarage();

      //allow closing - if the state is open
    } else if (newPayload == "close" && dianeState == "open" ) {
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

//void flipMikeState() {
//  if (mikeState == "open") {
//    mikeState = "closed";
//  } else if (mikeState = "closed") {
//    mikeState = "open";
//  }
//}
//
//void flipDianeState() {
//  if (dianeState == "open") {
//    dianeState = "closed";
//  } else if (dianeState = "closed") {
//    dianeState = "open";
//  }
//}
