/**
 * ESP8266 12F + CS2105GO-S10 Smoke Sersor with MQTT integration.
 * To be powered by MP2307-based step down converter (CS2105GO-S10 link pin to MP2307 enable pin).
 **/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>

//Atomic OTA Updates
#define ATOMIC_FS_UPDATE

//Pull PIN 4 LOW for OTA
#define OTA_TRIGGER_PIN  4

#include "config.h"

String ProgramVersion  = "0.1";

WiFiClient client;
HTTPClient http;
PubSubClient mqttClient(client);
char rssi_str[6];

void gotoDeepSleep(int SleepDelay){
  Serial.print("Sleeping " + String(SleepDelay) + " seconds\n");
  ESP.deepSleepInstant(SleepDelay * 1000000,WAKE_RF_DEFAULT);
}

void update_started() {
  Serial.println("HTTP update process started");
}

void update_finished() {
  Serial.println("HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("HTTP update fatal error code %d\n", err);
}

int checkForUpdates() {
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    pinMode(OTA_TRIGGER_PIN, INPUT_PULLUP);
    delay(100);
    
    if ( digitalRead(OTA_TRIGGER_PIN) != LOW ) {
      return 1;
    }

    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    Serial.print("\nChecking for update from " + String(UpdateURL) + ". Current FW version " + String(FWVersion) + "\n");
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, String(UpdateURL),String(FWVersion));

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        delay(5000);
        ESP.restart();
        break;
    }

  return 0;
}

int connect_wifi (){
  int retries = 0;
  int wifiStatus = WiFi.status();

  WiFi.forceSleepWake();
  delay(1);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);

  WiFi.begin( WifiSSID, WifiPass, WifiChannel, WifiBSSID, true );
  Serial.printf("[WIFI] ... quick connect, status: %d\nConnecting.", wifiStatus);

  while( wifiStatus != WL_CONNECTED ) {
    retries++;
    Serial.write('.');
    if( retries == 100 ) {
       Serial.printf("\n[WIFI] ... normal connect, status: %d\nConnecting.", wifiStatus);
       // Quick connect is not working, reset WiFi and try regular connection
      WiFi.disconnect();
      delay( 10 );
      WiFi.forceSleepBegin();
      delay( 10 );
      WiFi.forceSleepWake();
      delay( 10 );
      WiFi.begin( WifiSSID, WifiPass );
    }
    
    if( retries >= 600 ) {
      // Giving up after 30 seconds and going back to sleep
      WiFi.disconnect( true );
      delay( 1 );
      WiFi.mode( WIFI_OFF );
      gotoDeepSleep(sleepTimeS);
      return 1; // Not expecting this to be called, the previous call will never return.
    }
    delay( 50 );
    wifiStatus = WiFi.status();
  }

  Serial.print("Connected\n");

return wifiStatus;
}

int getRSSI(){
  int32_t rssi=WiFi.RSSI();

  itoa(rssi, rssi_str, 10); // Convert integer to string
  Serial.println("WiFi RSSI: " + String(rssi) + "dBm");
  
  return 0; 
}

void setup() {
  //Prepare MQTT client id and topics
  String mqtt_client_id = ESPName + " Smoke Detector";
  String mqtt_status_topic = "/sensor/" + ESPName + "/smoke-detector/status";
  String mqtt_rssi_topic = "/sensor/" + ESPName + "/smoke-detector/rssi";

  Serial.begin(115200);
  Serial.print("\n" + ESPName + " Smoke Detector started\n");

  //Debug WiFi off
  Serial.setDebugOutput(false);

  //Disable WiFi status LED to save battery
  wifi_status_led_uninstall();

  // Connect WiFi
  connect_wifi();

  // Configure MQTT server
  mqttClient.setServer(mqtt_server, mqtt_port);

  // Connect to MQTT Broker and publish the "ON" state immediately
  if (mqttClient.connect(mqtt_client_id.c_str(), mqtt_user, mqtt_pass)) {
     // Publish as a retained message so Home Assistant registers it even if it missed the exact millisecond
    mqttClient.publish(mqtt_status_topic.c_str(), "ON", true);

    getRSSI();

     // Publish as a retained message so Home Assistant registers it even if it missed the exact millisecond
     mqttClient.publish(mqtt_rssi_topic.c_str(), rssi_str, true);
  }

  //Check for OTA update and update if OTA_TRIGGER_PIN is LOW.
  checkForUpdates();
  
  gotoDeepSleep(sleepTimeS);
}

void loop() {
// Nothing Here
}
//EOF
