#define VERSION "0.1"
#define TAG "esp32phone"

#define MQTT_SOCKET_TIMEOUT 60
long otaUpdateStart = 0;
#define OTA_TIMEOUT (MQTT_SOCKET_TIMEOUT * 1000)

#include "main.h"
#include <WiFiMulti.h>
#include "Arduino.h"
#include <PubSubClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>
#include "esp_log.h"
#include "SPIFFS.h"
#include <Update.h>
#include <StreamString.h>
#include <HTTPUpdate.h>
#include "i2shandler.h"



WiFiMulti WiFiMulti;

bool currentlyUpdating=false;

// Add your MQTT Broker IP address
const char* mqtt_server = "mrxa.ga";
//const char* mqtt_server = "mrxa.ml";
int port = 8883;


const char* server_root_ca = 
"-----BEGIN CERTIFICATE-----\n" \
"MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n" \
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
"DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n" \
"PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n" \
"Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n" \
"rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n" \
"OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n" \
"xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n" \
"7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n" \
"aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n" \
"HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n" \
"SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n" \
"ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n" \
"AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n" \
"R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n" \
"JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n" \
"Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\n" \
"-----END CERTIFICATE-----";


//WiFiClient espClient;
// to enable secure communication tls
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

const char* mqtt_user = "mka";
const char* mqtt_pass = "Esp32veri§SecurEDaepeikik§§*";
String mqtt_id;

// for testing Pin
const int testPin = 4;


void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(CONFIG_ESP_WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
}


// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    yield();
    delay(500);
    Serial.print(F("."));
    now = time(nullptr);
  }

  Serial.println(F(""));
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}

void callback(char* topic, byte* message, unsigned int length) {

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32phone_.../cmd
  // Changes the output state according to the message
  if (String(topic) == (mqtt_id + "/cmd")) {
    Serial.println("Changing output to");
    ESP_LOGI(TAG, "message received");
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    String messageTemp;
    
    for (int i = 0; i < length; i++) {
      Serial.print((char)message[i]);
      messageTemp += (char)message[i];
    }
    Serial.print(". Message: ");
    Serial.println(messageTemp);
    ESP_LOGI(TAG, "Cmd Message received: %s", messageTemp.c_str());
  } else if (String(topic) == (mqtt_id + "/config")) {
      File file = SPIFFS.open("/config", FILE_WRITE);

      if (!file) {
        Serial.println("There was an error opening the file for writing");
        ESP_LOGE(TAG, "opening config file failed");
        return;
      }
      if (file.write(message, length)) {
        Serial.println("File was written");
        ESP_LOGI(TAG, "config file was written");
      } else {
        Serial.println("File write failed");
        ESP_LOGE(TAG, "failed to write config file");
      }

      file.close();
  } else if (String(topic) == (mqtt_id + "/update") ) {      
      String messageTemp;
      
      for (int i = 0; i < length; i++) {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
      }
      ESP_LOGI(TAG, "update triggered... '%s'", messageTemp.c_str());

      currentlyUpdating = true;
      WiFiClientSecure client;
      client.setCACert(server_root_ca);
      // OTA over ssl maybe slow 
      client.setTimeout(12000);

      t_httpUpdate_return ret = httpUpdate.update(client, messageTemp);
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          ESP_LOGI(TAG, "Http update failed no updates (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          ESP_LOGI(TAG, "Http update no updates");
          break;

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          ESP_LOGI(TAG, "Finished, Rebooting");
          break;
      }
      currentlyUpdating=false;
  } else {
    //ESP_LOGI(TAG, "unknown topic found (skipping): %s", topic);
  }

}

void checkMqttServers() {
  int count=0;
  // Loop until we're reconnected
  while (!mqttClient.connected() && count<2) {
    count++;
    ESP_LOGI(TAG, "Attempting MQTT connection... %i", count);
    // Attempt to connect
    if (mqttClient.connect(mqtt_id.c_str(), 
                          mqtt_user, mqtt_pass, 
                          String("offline/" + mqtt_id).c_str(), 2, true, "offline",  
                          false)) {
      ESP_LOGI(TAG, "connected, subscribing to topic..");
      // Subscribe
      mqttClient.subscribe(String(mqtt_id + "/#").c_str());
    } else {
      ESP_LOGE(TAG, "failed to connect, mqtt state: %i, trying again", mqttClient.state());
      // Wait x seconds before retrying
      delay(2000);
    }
  }
}


void setup(void) {

    ESP_LOGI(TAG, "Starting version " VERSION " ...");
    setup_wifi();

    mqtt_id = String("esp32phone_") + WiFi.macAddress();
    ESP_LOGI(TAG, "client: %s", mqtt_id.c_str());
    ESP_LOGI(TAG, "mqtt socket timeout: %d", MQTT_SOCKET_TIMEOUT);

    espClient.setCACert(server_root_ca);
    // OTA over ssl maybe slow 
    espClient.setTimeout(12000);
    mqttClient.setServer(mqtt_server, port);
    mqttClient.setCallback(callback);

    pinMode(testPin, OUTPUT);

    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to mount the file system");
        //return;
    }

    i2s_setup();
}

long lastMsg = 0;
bool wifiConnected=false;

void loop() {

    if ((WiFiMulti.run() == WL_CONNECTED)) {
        if (!wifiConnected) {
          Serial.println("WiFi connected");
          Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
          ESP_LOGI(TAG, "wifi done enabled %s", String(WiFi.localIP()).c_str());
          wifiConnected = true;
          setClock();
        }

      checkMqttServers();

      if (!currentlyUpdating) {
        mqttClient.loop();

        long now = millis();
        if (now - lastMsg > 30000) {
            lastMsg = now;
            mqttClient.publish(String(mqtt_id + "/version").c_str(), VERSION);
        }
      }
    } else {
      wifiConnected = false;
    }
}

