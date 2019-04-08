//#include <Arduino.h>
#include <GxEPD.h>
#include <GxGDEW0213I5F/GxGDEW0213I5F.cpp>      // 2.13" b/w
//#include <GxGDEW0213I5F/GxGDEW0213I5F.h>  // 2.13" b/w 104x212 flexible
#include <ArduinoJson.h>

#include GxEPD_BitmapExamples

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>

//#include <WiFi.h>
//#include <HTTPClient.h>

//#include <WebSocketsClient.h>

//needed for library
//#include <ESPAsyncWebServer.h>
//#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <Servo.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//static const int servoPin = 5;

//Servo servo1;

#define SLACK_BOT_TOKEN "xoxb-245039482758-452890210951-J7b18WYSxjJmSwYcbgRcdQYd"
//#define WIFI_SSID "wifi-name"
//#define WIFI_PASSWORD "wifi-password"

// if Slack changes their SSL fingerprint, you would need to update this:
//#define SLACK_SSL_FINGERPRINT "AC 95 5A 58 B8 4E 0B CD B3 97 D2 88 68 F5 CA C1 0A 81 E3 6E"

#define WORD_SEPERATORS "., \"'()[]<>;:-+&?!\n\t"

RTC_DATA_ATTR int currentSlot = 0;
RTC_DATA_ATTR int bootCount = 0;
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */

//AsyncWebServer server(80);
//DNSServer dns;
//WebSocketsClient webSocket;
// mapping suggestion for ESP32, e.g. LOLIN32, see .../variants/.../pins_arduino.h for your board
// NOTE: there are variants with different pins for SPI ! CHECK SPI PINS OF YOUR BOARD
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V

// pins_arduino.h, e.g. LOLIN32
//static const uint8_t SS    = 5;
//static const uint8_t MOSI  = 23;
//static const uint8_t MISO  = 19;
//static const uint8_t SCK   = 18;

//GxIO_Class io(SPI, /*CS=5*/ 17, /*DC=*/ 16, /*RST=*/ 4); // arbitrary selection of 17, 16
//GxEPD_Class display(io, /*RST=*/ 4, /*BUSY=*/ 0); // arbitrary selection of (16), 4
GxIO_Class io(SPI, /*CS=5*/ 5, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

// We need to send an incrementing id with each outgoing message
long nextCmdId = 1;
bool connected = false;
unsigned long lastPing = 0;

BLECharacteristic *characteristicTX;
#define SERVICE_UUID   "ab0828b1-198e-4351-b779-901fa0e0371e"
#define CHARACTERISTIC_UUID_RX  "4ac8a682-9736-4e5d-932b-e9b31405049c"
#define CHARACTERISTIC_UUID_TX  "0972EF8C-7613-4075-AD52-756F33D4DA91"
bool deviceConnected = false;

//**
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

//** callback for receiving data
class CharacteristicCallbacks: public BLECharacteristicCallbacks {
     void onWrite(BLECharacteristic *characteristic) {
          std::string rxValue = characteristic->getValue();

          if (rxValue.length() > 0) {

               for (int i = 0; i < rxValue.length(); i++) {
                 Serial.print(rxValue[i]);
               }
               Serial.println();

               if (rxValue.find("L1") != -1) {
                 //digitalWrite(LED, HIGH);
               }
               else if (rxValue.find("L0") != -1) {
                 //digitalWrite(LED, LOW);
               }

               else if (rxValue.find("B1") != -1) {
                 //digitalWrite(BUZZER, HIGH);
               }
               else if (rxValue.find("B0") != -1) {
                 //digitalWrite(BUZZER, LOW);
               }
          }
     }
};

void setupBLE() {
  // Create the BLE Device
  BLEDevice::init("ESP32-BLE");

  // Create the BLE Server
  BLEServer *server = BLEDevice::createServer();

  // set callback so we can know when somebody connects to us
  server->setCallbacks(new ServerCallbacks());

  // Create the BLE Service
  BLEService *service = server->createService(SERVICE_UUID);

  // Create a BLE Characteristic for us to transmit data
  characteristicTX = service->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  characteristicTX->addDescriptor(new BLE2902());

  // Create a BLE Characteristic for us to receive data
  BLECharacteristic *characteristic = service->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  characteristic->setCallbacks(new CharacteristicCallbacks());

  // Start the service
  service->start();

  // Start advertising (descoberta do ESP32)
  server->getAdvertising()->start();
}

void sendBLE(char *string) {
  characteristicTX->setValue(string);
  characteristicTX->notify();
}


/*
void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void processSlackMessage(char *payload) {
  char *nextWord = NULL;
  bool zebra = false;
  for (nextWord = strtok(payload, WORD_SEPERATORS); nextWord; nextWord = strtok(NULL, WORD_SEPERATORS)) {
    if (strcasecmp(nextWord, "#gong") == 0) {
        Serial.println("Bang a gong!");
        servo1.write(20);
        servo1.write(0);
    }

    if (nextWord[0] == '#') {
      int color = strtol(&nextWord[1], NULL, 16);
      Serial.println("Color");
      Serial.print(color);
      if (color) {
        //drawColor(color, zebra);
      }
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      processSlackMessage((char*)payload);
      break;
  }
}

bool connectToSlack() {
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN);//, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");
  http.end();

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);
  return true;
}
*/

void stop(int color) {
  display.setRotation(1);
  display.setCursor(0,30);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(color);
  display.setTextSize(1);
  display.print("Stop looking..");
  display.setTextSize(2);
  display.setCursor(0,80);
  display.print("At my cup...");
  //display.drawRect(10,10,50,50, GxEPD_BLACK);
  //display.drawCircle(100, 30, 20, GxEPD_BLACK);
  display.update();
  //display.powerDown();
}

void urge(int color) {
  display.setRotation(1);
  display.setCursor(0,30);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(color);
  display.setTextSize(1);
  display.print("URGE TO KILL");
  display.setTextSize(3);
  display.setCursor(0,80);
  display.print("RISING");
  //display.drawRect(10,10,50,50, GxEPD_BLACK);
  //display.drawCircle(100, 30, 20, GxEPD_BLACK);
  display.update();
  //display.powerDown();
}

void meetingssuck(int color) {
  display.setRotation(1);
  display.setCursor(0,30);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(color);
  display.setTextSize(2);
  display.print("MEETINGS");
  display.setCursor(0,80);
  display.print("SUCK");
  //display.drawRect(10,10,50,50, GxEPD_BLACK);
  //display.drawCircle(100, 30, 20, GxEPD_BLACK);
  display.update();
  //display.powerDown();
}

void nerdsrule(int color) {
  display.setRotation(1);
  display.setCursor(0,30);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(color);
  display.setTextSize(3);
  display.print("NERDS");
  display.setCursor(0,80);
  display.print("RULE");
  //display.drawRect(10,10,50,50, GxEPD_BLACK);
  //display.drawCircle(100, 30, 20, GxEPD_BLACK);
  display.update();
  //display.powerDown();
}

void cleardisplay() {
  /*display.fillScreen(GxEPD_BLACK);
  display.update();*/
  //display.fillScreen(GxEPD_WHITE);
  //display.update();
}

void setup() {
  Serial.begin(115200);
  display.init(115200); // enable diagnostic output on Serial

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  if (bootCount == 1) cleardisplay();
  /*
    First we configure the wake up source
    We set our ESP32 to wake up every 5 seconds
    */
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
    " Seconds");

    /*
  Next we decide what all peripherals to shut down/keep on
  By default, ESP32 will automatically power down the peripherals
  not needed by the wakeup source, but if you want to be a poweruser
  this is for you. Read in detail at the API docs
  http://esp-idf.readthedocs.io/en/latest/api-reference/system/deep_sleep.html
  Left the line commented as an example of how to configure peripherals.
  The line below turns off all RTC peripherals in deep sleep.
  */
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //Serial.println("Configured all RTC Peripherals to be powered down in sleep");

//AsyncWiFiManager wifiManager(&server,&dns);
//wifiManager.autoConnect("AutoConnectAP");


  //connected = connectToSlack();
  Serial.printf("slot: %i", currentSlot);
  //servo1.attach(servoPin);
  //servo1.write(0);

  if (currentSlot == 0) {
    currentSlot = 1;
    //cleardisplay();
    //urge(GxEPD_WHITE);
    meetingssuck(GxEPD_BLACK);
    Serial.flush();
  } else if (currentSlot == 1) {
    //delay(10 * 1000);
    currentSlot = 2;
    //cleardisplay();
    //meetingssuck(GxEPD_WHITE);
    nerdsrule(GxEPD_BLACK);
    Serial.flush();
  } else if (currentSlot == 2) {
    currentSlot = 3;
    //cleardisplay();
    //nerdsrule(GxEPD_WHITE);
    stop(GxEPD_BLACK);
    Serial.flush();
  } else {
    currentSlot = 0;
    //cleardisplay();
    //stop(GxEPD_WHITE);
    urge(GxEPD_BLACK);
    Serial.flush();
  }

display.powerDown();
  esp_deep_sleep_start();

}


void showPartialUpdatePaged();
void showBlackBoxCallback(uint32_t v);
void showValueBoxCallback(uint32_t v);

void loop() {
  /*delay(30000);
  if (currentSlot == 0) {
    currentSlot = 1;
    cleardisplay();
    meetingssuck();
    Serial.flush();
    //esp_deep_sleep_start();
  } else if (currentSlot == 1) {
    //delay(10 * 1000);
    currentSlot = 2;
    cleardisplay();
    nerdsrule();
    Serial.flush();
    //esp_deep_sleep_start();
  } else {
    currentSlot = 0;
    cleardisplay();
    urge();
    Serial.flush();
    //esp_deep_sleep_start();
  }*/

  //webSocket.loop();
  //showPartialUpdatePaged();
  //showBlackBoxCallback( GxEPD_BLACK);

  //delay(10 * 1000);

/*
  if (connected) {
    // Send ping every 5 seconds, to keep the connection alive
    if (millis() - lastPing > 5000) {
      Serial.println("connect ping");
      sendPing();
      lastPing = millis();
    }
  } else {
    Serial.println("disconnected..");
    // Try to connect / reconnect to slack
    connected = connectToSlack();
    if (!connected) {
      delay(500);
    }
  }*/
}

void showBlackBoxCallback(uint32_t v)
{
  uint16_t box_x = 10;
  uint16_t box_y = 15;
  uint16_t box_w = 70;
  uint16_t box_h = 20;
  display.fillRect(box_x, box_y, box_w, box_h, v);
}

void showValueBoxCallback(const uint32_t v)
{
  uint16_t box_x = 10;
  uint16_t box_y = 15;
  uint16_t box_w = 70;
  uint16_t box_h = 20;
  uint16_t cursor_y = box_y + box_h - 6;
  display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
  display.setCursor(box_x, cursor_y);
  display.print(v / 100);
  display.print(v % 100 > 9 ? "." : ".0");
  display.print(v % 100);
}

void showPartialUpdatePaged()
{
  uint16_t box_x = 10;
  uint16_t box_y = 15;
  uint16_t box_w = 70;
  uint16_t box_h = 20;
  uint16_t cursor_y = box_y + box_h - 6;
  uint32_t value = 1395;
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(0);
  // draw background
  display.drawExampleBitmap(BitmapExample1, sizeof(BitmapExample1));
  delay(2000);

  // partial update to full screen to preset for partial update of box window
  // (this avoids strange background effects)
  display.drawExampleBitmap(BitmapExample1, sizeof(BitmapExample1), GxEPD::bm_flip_x | GxEPD::bm_partial_update);
  delay(1000);

  // show where the update box is
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    display.drawPagedToWindow(showBlackBoxCallback, box_x, box_y, box_w, box_h, GxEPD_BLACK);
    //display.drawPagedToWindow(showBlackBoxCallback, box_x, box_y, box_w, box_h, GxEPD_BLACK);
    delay(1000);
    display.drawPagedToWindow(showBlackBoxCallback, box_x, box_y, box_w, box_h, GxEPD_WHITE);
  }
  // show updates in the update box
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    for (uint16_t i = 1; i <= 10; i++)
    {
      uint32_t v = value * i;
      display.drawPagedToWindow(showValueBoxCallback, box_x, box_y, box_w, box_h, v);
      delay(500);
    }
    delay(1000);
    display.drawPagedToWindow(showBlackBoxCallback, box_x, box_y, box_w, box_h, GxEPD_WHITE);
  }
  display.setRotation(0);
  display.powerDown();
}
