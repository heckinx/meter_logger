// #include <U8g2lib.h>
// #include <Wire.h>
#include <WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "time.h"
#define DEBUG

const char* ssid = "RW";
const char* password = "5l3t5j0MTZHz";
// const char* ssid = "TP-Link_A894";
// const char* password = "1223334444";

const char* ntpServer = "pool.ntp.org";

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;

Ticker timer;

// U8G2_ST7567_ENH_DG128064I_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

JsonDocument doc;

const int dataInterval = 5;

// MQTT config
#define MQTT_HOST IPAddress(192, 168, 33, 203)
#define MQTT_PORT 1883
#define MQTT_ID "ESP32_meters"

#define DEVICE_ID "A3JEO36GA9"

int pulsePin1 = 39; // Dom 1
int pulsePin2 = 40; // PC
int pulsePin3 = 41; // Dom 2
int pulsePin4 = 42; // Growatt

volatile unsigned long addPulses1 = 0;
unsigned long pulseCount1 = 74600040;
double power1 = 0;
volatile unsigned long startMillis1 = 0;
volatile unsigned long lastDeltaT1 = 0;

volatile unsigned long addPulses2 = 0;
unsigned long pulseCount2 = 23107670;
double power2 = 0;
volatile unsigned long startMillis2 = 0;
volatile unsigned long lastDeltaT2 = 0;

volatile unsigned long addPulses3 = 0;
unsigned long pulseCount3 = 27655360;
double power3 = 0;
volatile unsigned long startMillis3 = 0;
volatile unsigned long lastDeltaT3 = 0;

volatile unsigned long addPulses4 = 0;
unsigned long pulseCount4 = 119452830;
double power4 = 0;
volatile unsigned long startMillis4 = 0;
volatile unsigned long lastDeltaT4 = 0;

unsigned long getTime();
void updateData();
// void lcdSetup();
void inttr1();
void inttr2();
void inttr3();
void inttr4();
void publish(const char* topic, const char* measurement, JsonDocument data);
void netSetup();
void connectToWifi();
void connectToMqtt();
void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info);
void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void setupOTA();

void setup() {
  Serial.begin(115200);

  EEPROM.begin(512);

  // delay(2000);

  EEPROM.get(0, pulseCount1);
  // EEPROM.get(64, pulseCount2);
  EEPROM.get(128, pulseCount3);
  EEPROM.get(192, pulseCount4);

  // EEPROM.put(0, pulseCount1);
  EEPROM.put(64, pulseCount2);
  // EEPROM.put(128, pulseCount3);
  // EEPROM.put(192, pulseCount4);
  EEPROM.commit();

  // Serial.println(pulseCount1);
  // Serial.println(pulseCount2);

  // lcdSetup();
  netSetup();

  connectToWifi();

  pinMode(pulsePin1, INPUT_PULLUP);
  pinMode(pulsePin2, INPUT_PULLUP);
  pinMode(pulsePin3, INPUT_PULLUP);
  pinMode(pulsePin4, INPUT_PULLUP);

  attachInterrupt(pulsePin1, inttr1, FALLING);
  attachInterrupt(pulsePin2, inttr2, FALLING);
  attachInterrupt(pulsePin3, inttr3, FALLING);
  attachInterrupt(pulsePin4, inttr4, FALLING);
}

long testMillisStart = 0;

void loop() {
  ArduinoOTA.handle();
  //timeClient.update();
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void updateData() {
  // testMillisStart = millis();
  //noInterrupts();
  if (addPulses1 > 0) {
    pulseCount1 += addPulses1;
    addPulses1 = 0;
    EEPROM.put(0, pulseCount1);
  }
  if (addPulses2 > 0) {
    pulseCount2 += addPulses2;
    addPulses2 = 0;
    EEPROM.put(64, pulseCount2);
  }
  if (addPulses3 > 0) {
    pulseCount3 += addPulses3;
    addPulses3 = 0;
    EEPROM.put(128, pulseCount3);
  }
  if (addPulses4 > 0) {
    pulseCount4 += addPulses4;
    addPulses4 = 0;
    EEPROM.put(192, pulseCount4);
  }

  EEPROM.commit();
  //interrupts();


  if(millis() - startMillis1 > lastDeltaT1) {
    if(millis() - startMillis1 > 60000) {
      power1 = 0.00f;
    } else {
      power1 = 3600.00f/((millis()-startMillis1)/1000.00f);
    }
  } else if (lastDeltaT1 > 0) {
    power1 = 3600.00f/(lastDeltaT1/1000.00f);
  }

  if(millis() - startMillis2 > lastDeltaT2) {
    if(millis() - startMillis2 > 60000) {
      power2 = 0.00f;
    } else {
      power2 = 3600.00f/((millis()-startMillis2)/1000.00f);
    }
  } else if (lastDeltaT2 > 0) {
    power2 = 3600.00f/(lastDeltaT2/1000.00f);
  }

  if(millis() - startMillis3 > lastDeltaT3) {
    if(millis() - startMillis3 > 60000) {
      power3 = 0.00f;
    } else {
      power3 = 3600.00f/((millis()-startMillis3)/1000.00f);
    }
  } else if (lastDeltaT3 > 0) {
    power3 = 3600.00f/(lastDeltaT3/1000.00f);
  }

  if(millis() - startMillis4 > lastDeltaT4) {
    if(millis() - startMillis4 > 60000) {
      power4 = 0.00f;
    } else {
      power4 = 3600.00f/((millis()-startMillis4)/1000.00f);
    }
  } else if (lastDeltaT4 > 0) {
    power4 = 3600.00f/(lastDeltaT4/1000.00f);
  }

  doc["data"]["meter1_power"] = (double)power1;
  doc["data"]["meter1_power_total"] = (double)pulseCount1 / 1000.00f;
  doc["data"]["meter2_power"] = power2;
  doc["data"]["meter2_power_total"] = (double)pulseCount2 / 1000.00f;
  doc["data"]["meter3_power"] = power3;
  doc["data"]["meter3_power_total"] = (double)pulseCount3 / 1000.00f;
  doc["data"]["meter4_power"] = power4;
  doc["data"]["meter4_power_total"] = (double)pulseCount4 / 1000.00f;

  if(getTime() != 0) publish("data/esp32_node", "meters", doc);

  char powerStr1[10];
  dtostrf(power1, 2, 2, powerStr1);
  char usageStr1[20];
  dtostrf((double)pulseCount1 / 1000.00f, 3, 3, usageStr1);

  char powerStr2[10];
  dtostrf(power2, 2, 2, powerStr2);
  char usageStr2[20];
  dtostrf((double)pulseCount2 / 1000.00f, 3, 3, usageStr2);

  char powerStr3[10];
  dtostrf(power3, 2, 2, powerStr3);
  char usageStr3[20];
  dtostrf((double)pulseCount3 / 1000.00f, 3, 3, usageStr3);

  char powerStr4[10];
  dtostrf(power4, 2, 2, powerStr4);
  char usageStr4[20];
  dtostrf((double)pulseCount4 / 1000.00f, 3, 3, usageStr4);


  Serial.printf("Power1: %s, Pulse Count: %d, Usage: %skWh \n", powerStr1, pulseCount1, usageStr1);
  Serial.printf("Power2: %s, Pulse Count: %d, Usage: %skWh \n", powerStr2, pulseCount2, usageStr2);
  Serial.printf("Power3: %s, Pulse Count: %d, Usage: %skWh \n", powerStr3, pulseCount3, usageStr3);
  Serial.printf("Power4: %s, Pulse Count: %d, Usage: %skWh \n\n", powerStr4, pulseCount4, usageStr4);


  // Serial.println(millis() - testMillisStart);
}

// void lcdSetup() {
//   Wire.begin(1, 2);
//   u8g2.setI2CAddress(0x7E);
//   u8g2.begin();
//   u8g2.setContrast(200);
//   u8g2.clearBuffer();  // clear the internal memory
//   u8g2.setFont(u8g2_font_ncenB08_tr);
//   u8g2.setFontPosTop();
//   u8g2.setPowerSave(1);
// }

void IRAM_ATTR inttr1() {
  long deltaT1 = millis() - startMillis1;
  // Serial.println(deltaT1);
  if(deltaT1 > 100) {
    addPulses1 += 1L;
    lastDeltaT1 = deltaT1;
    startMillis1 = millis();
    // Serial.println(lastDeltaT1);
  }
  
}

void IRAM_ATTR inttr2() {
  long deltaT2 = millis() - startMillis2;
  // Serial.println(deltaT2);
  if(deltaT2 > 100) {
    addPulses2 += 1L;
    lastDeltaT2 = deltaT2;
    startMillis2 = millis();
    // Serial.println(lastDeltaT1);
  }

}

void IRAM_ATTR inttr3() {
  long deltaT3 = millis() - startMillis3;
  if(deltaT3 > 100) {
    addPulses3 += 1L;
    lastDeltaT3 = deltaT3;
    startMillis3 = millis();
    //Serial.println(lastDeltaT1);
  }

}

void IRAM_ATTR inttr4() {
  long deltaT4 = millis() - startMillis4;
  if(deltaT4 > 100) {
    addPulses4 += 1L;
    lastDeltaT4 = deltaT4;
    startMillis4 = millis();
    //Serial.println(lastDeltaT4);
  }

}

void publish(const char* topic, const char* measurement, JsonDocument data) {

  char buffer[512];

  data["data"]["timestamp"] = String(getTime()) + "000";
  data["data"]["device"] = "esp32_logger_home";
  data["data"]["measurement"] = measurement;

  // sprintf(buffer, "%s,device=ESP32,deviceId=%s %s %s", measurement, DEVICE_ID, value, timestamp);

  serializeJson(data, buffer);
  mqttClient.publish(topic, 1, false, buffer);
#ifdef DEBUG
  Serial.println(topic);
  Serial.println(buffer);
#endif
}

void netSetup() {
  WiFi.onEvent(onWifiConnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(onWifiDisconnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_13dBm);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  configTime(0, 0, ntpServer);
}

void connectToWifi() {
  if (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
    Serial.println("Connecting to Wi-Fi...");
#endif
    WiFi.begin(ssid, password);
  }
}

void connectToMqtt() {
#ifdef DEBUG
  Serial.println("Connecting to MQTT...");
#endif

  mqttClient.connect();
}

void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
#ifdef DEBUG
  Serial.println("Connected to Wi-Fi.");
  Serial.print("IP address: ");
  Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
#endif
  // timeClient.begin();
  setupOTA();
  connectToMqtt();
}

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
#ifdef DEBUG
  Serial.println("Disconnected from Wi-Fi.");
#endif
  wifiReconnectTimer.once(30, connectToWifi);
  timer.detach();
}

void onMqttConnect(bool sessionPresent) {
#ifdef DEBUG
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
#endif
  // uint16_t packetIdSub = mqttClient.subscribe("test/lol", 2);
  // Serial.print("Subscribing at QoS 2, packetId: ");
  // Serial.println(packetIdSub);
  timer.attach(dataInterval, updateData);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
#ifdef DEBUG
  Serial.println("Disconnected from MQTT.");
#endif

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  char m_str[len + 1];
  for (int i = 0; i < len; i++) {
    m_str[i] = (char)payload[i];
  }
  m_str[len] = '\0';
#ifdef DEBUG
  Serial.println("Publish received.");
  Serial.println(m_str);
#endif
}

void setupOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("METERS");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      noInterrupts();
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      interrupts();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      interrupts();
    });

  ArduinoOTA.begin();
}