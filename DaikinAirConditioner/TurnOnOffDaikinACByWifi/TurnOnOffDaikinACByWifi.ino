/* Copyright 2017 sillyfrog
*
* An IR LED circuit *MUST* be connected to the ESP8266 on a pin
* as specified by kIrLed below.
*
* TL;DR: The IR LED needs to be driven by a transistor for a good result.
*
* Suggested circuit:
*     https://github.com/crankyoldgit/IRremoteESP8266/wiki#ir-sending
*
* Common mistakes & tips:
*   * Don't just connect the IR LED directly to the pin, it won't
*     have enough current to drive the IR LED effectively.
*   * Make sure you have the IR LED polarity correct.
*     See: https://learn.sparkfun.com/tutorials/polarity/diode-and-led-polarity
*   * Typical digital camera/phones can be used to see if the IR LED is flashed.
*     Replace the IR LED with a normal LED if you don't have a digital camera
*     when debugging.
*   * Avoid using the following pins unless you really know what you are doing:
*     * Pin 0/D3: Can interfere with the boot/program mode & support circuits.
*     * Pin 1/TX/TXD0: Any serial transmissions from the ESP8266 will interfere.
*     * Pin 3/RX/RXD0: Any serial transmissions to the ESP8266 will interfere.
*   * ESP-01 modules are tricky. We suggest you use a module with more GPIOs
*     for your first time. e.g. ESP-12 etc.
*/

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>
#include <EEPROM.h>

#define ONOFF_LED_PIN D1

/////////////////////// Wifi ///////////////////////
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#ifndef STASSID
#define STASSID "burke"  //無線分享器的名稱
#define STAPSK  "0912404007"    //密碼
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);
////////////////////////////////////////////////////

/////////////////////// TimeClient ///////////////////////
#include <WiFiUdp.h>
#include <NTPClient.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000); // UTC+8 时区，更新间隔为60秒

int getHour() {
  timeClient.update();
  return timeClient.getHours();
}

int getMinute() {
  timeClient.update();
  return timeClient.getMinutes();
}
//////////////////////////////////////////////////////////


// Daikin

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRDaikinESP ac(kIrLed);  // Set the GPIO to be used to sending the message
int isOn;
int pressOnOff = 0;
int currentTemperature = 25, upperBoundTemp = 32, lowerBoundTemp = 10;
int currentFan = 1, upperBoundFan = 7, lowerBoundFan = 1;
const int EEPROM_SIZE = 512;
const int EEPROM_TEMP_ADDR = 0;
const int EEPROM_FAN_ADDR = sizeof(currentTemperature);

/////////////////////// Wifi ///////////////////////
void handleRoot() {  //訪客進入主網頁時顯示的內容
  server.send(200, "text/plain", "Hello From ESP8266 !");
}

void handleNotFound() {  //找不到網頁時顯示的內容
  server.send(404, "text/plain", "File Not Found");
}
////////////////////////////////////////////////////


void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(ONOFF_LED_PIN, OUTPUT);

/////////////////////// TimeClient ///////////////////////
  timeClient.begin();
//////////////////////////////////////////////////////////

  ac.begin();
  isOn = 0;

  currentTemperature = EEPROM.read(EEPROM_TEMP_ADDR);
  if (currentTemperature == 255) {
    currentTemperature = 25;
  }

  currentFan = EEPROM.read(EEPROM_FAN_ADDR);
  if (currentFan == 255) {
    currentFan = 1;
  }

/////////////////////// Wifi ///////////////////////
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // pinMode(LED_BUILTIN, OUTPUT); //設定GPIO2為OUTPUT
  // digitalWrite(LED_BUILTIN, HIGH);  //熄滅LED

  // 等待WiFi連線
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  delay(5000);
  timeClient.update();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //在監控視窗顯示取得的IP
  Serial.print("Current time: ");
  Serial.println(timeClient.getFormattedTime());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);  //綁定主網頁會觸發的副程式

  server.on("/on-off", []() {      //網頁 /on-off 會執行的程式
    server.send(200, "text/plain", "AC ON / OFF");
    pressOnOff = 1;
  });

  server.on("/temp-up", []() { // 調高溫度
    if (currentTemperature < upperBoundTemp) {
      currentTemperature++;
      ac.setTemp(currentTemperature);
#if SEND_DAIKIN
      ac.send();
#endif  // SEND_DAIKIN
      EEPROM.write(EEPROM_TEMP_ADDR, ac.getTemp());
      EEPROM.commit();
      Serial.println("Increased Temperature:");
      Serial.println(currentTemperature);
    }
    server.send(200, "text/plain", "Temperature increased");
  });

  server.on("/temp-down", []() { // 調低溫度
    if (currentTemperature > lowerBoundTemp) {
      currentTemperature--;
      ac.setTemp(currentTemperature);
#if SEND_DAIKIN
      ac.send();
#endif  // SEND_DAIKIN
      EEPROM.write(EEPROM_TEMP_ADDR, ac.getTemp());
      EEPROM.commit();
      Serial.println("Decreased Temperature:");
      Serial.println(currentTemperature);
    }
    server.send(200, "text/plain", "Temperature decreased");
  });

  server.on("/get-temp", []() { // server send 溫度
    String temperature = String(currentTemperature);
    server.send(200, "text/plain", temperature);
  });

  server.on("/fan-up", []() { // 調高風扇
    if (currentFan < upperBoundFan) {
      currentFan++;
      if (currentFan == 6) {
        ac.setFan(10); // FanAuto
      } else if (currentFan == 7) {
        ac.setFan(11); // FanQuiet
      } else {
        ac.setFan(currentFan);
      }
      
#if SEND_DAIKIN
      ac.send();
#endif  // SEND_DAIKIN
      EEPROM.write(EEPROM_FAN_ADDR, currentFan);
      EEPROM.commit();
      Serial.println("Increased Fan:");
      Serial.println(currentFan);
    }
    server.send(200, "text/plain", "Fan increased");
  });

  server.on("/fan-down", []() { // 調低風扇
    if (currentFan > lowerBoundFan) {
      currentFan--;
      if (currentFan == 6) {
        ac.setFan(10); // FanAuto
      } else {
        ac.setFan(currentFan);
      }
      
#if SEND_DAIKIN
      ac.send();
#endif  // SEND_DAIKIN
      EEPROM.write(EEPROM_FAN_ADDR, currentFan);
      EEPROM.commit();
      Serial.println("Decreased Fan:");
      Serial.println(currentFan);
    }
    server.send(200, "text/plain", "Fan decreased");
  });

  server.on("/get-fan", []() { // server send 風扇
    String fan = String(currentFan);
    
    if (currentFan == 6) {
      server.send(200, "text/plain", "Auto"); // FanAuto
    } else if (currentFan == 7) {
      server.send(200, "text/plain", "Quiet"); // FanQuiet
    } else {
      server.send(200, "text/plain", fan);
    }
  });

  server.on("/set-off-timer", HTTP_GET, []() {
    int hour = getHour(), minute = getMinute(), set_timer = 0;
    if (server.hasArg("hour")) { // 检查是否有 "hour" 参数
      String hourStr = server.arg("hour");
      hour += hourStr.toInt();
      if (hour > 23) {
        hour = 23;
      } else if (hour < 0) {
        hour = 0;
      }
      set_timer = 1;
      Serial.print("Received hour: ");
      Serial.println(hour);
    }
    if (server.hasArg("minute")) { // 检查是否有 "minute" 参数
      String minuteStr = server.arg("minute");
      minute += minuteStr.toInt();
      if (minute > 59) {
        int add_hour = (int)(minute / 60);
        hour += add_hour;
        minute -= 60 * add_hour;
      } else if (minute < 0) {
        minute = 0;
      }
      set_timer = 1;
      Serial.print("Received minute: ");
      Serial.println(minute);
    }
    if (set_timer == 1) {
      ac.enableOffTimer(hour * 60 + minute);
#if SEND_DAIKIN
      ac.send();
#endif  // SEND_DAIKIN
      // 向客户端发送响应
      int offTime = ac.getOffTime();

      // server.send(200, "text/plain", "Set: [" + String(hour) + " : " + String(minute) + "], Current: [" + String(getHour()) + " : " + String(getMinute()) + "]");
      server.send(200, "text/plain", "Set: [" + String((int)(offTime / 60)) + " : " + String((int)(offTime % 60)) + "], Current: [" + String(getHour()) + " : " + String(getMinute()) + "]");
      // server.send(200, "text/plain", "Off at: " + String((int)(offTime / 60)) + " : " + String((int)(offTime % 60)));
      // server.send(200, "text/plain", "Off at: " + String(offTime));
    }
  });

  server.onNotFound(handleNotFound);  //綁定找不到網頁時會觸發的副程式

  server.begin();
  Serial.println("HTTP server started");
////////////////////////////////////////////////////
}


void loop() {
  server.handleClient();
  MDNS.update();
  timeClient.update();

  if (pressOnOff == 1) {
    pressOnOff = 0;

    if (isOn == 1) {
      isOn = 0;
      ac.off();
      Serial.println("Turn Off");
    } else {
      digitalWrite(ONOFF_LED_PIN, HIGH);

      isOn = 1;
      Serial.println("Sending...");

      // Set up what we want to send. See ir_Daikin.cpp for all the options.
      ac.on();
      if (currentFan == 6) {
        ac.setFan(10); // FanAuto
      } else if (currentFan == 7) {
        ac.setFan(11); // FanQuiet
      } else {
        ac.setFan(currentFan);
      }
      ac.setMode(kDaikinCool);
      ac.setTemp(currentTemperature);
      ac.setSwingVertical(false);
      ac.setSwingHorizontal(false);

      // Set the current time to 1:33PM (13:33)
      // Time works in minutes past midnight
      // ac.setCurrentTime(13 * 60 + 33);
      ac.setCurrentTime(getHour() * 60 + getMinute());
      // Turn off about 1 hour later at 2:30PM (14:30)
      // ac.enableOffTimer(14 * 60 + 30);
      Serial.println("Turn On");

      delay(1000);
      digitalWrite(ONOFF_LED_PIN, LOW);
    }

    // Display what we are going to send.
    Serial.println(ac.toString());

    // Now send the IR signal.
#if SEND_DAIKIN
    ac.send();
    Serial.println("Execute Command!");
#endif  // SEND_DAIKIN

    // delay(15000);
  }

/*
  while (Serial.available() > 0) {
    if (Serial.read() == '\n') {
      if (isOn == 1) {
        isOn = 0;
				ac.off();
				Serial.println("Turn Off");
			} else {
        isOn = 1;
        Serial.println("Sending...");

        // Set up what we want to send. See ir_Daikin.cpp for all the options.
        ac.on();
        ac.setFan(1);
        ac.setMode(kDaikinCool);
        ac.setTemp(25);
        ac.setSwingVertical(false);
        ac.setSwingHorizontal(false);

        // Set the current time to 1:33PM (13:33)
        // Time works in minutes past midnight
        ac.setCurrentTime(13 * 60 + 33);
        // Turn off about 1 hour later at 2:30PM (14:30)
        ac.enableOffTimer(14 * 60 + 30);
        Serial.println("Turn On");
      }

      // Display what we are going to send.
      Serial.println(ac.toString());

      // Now send the IR signal.
#if SEND_DAIKIN
      ac.send();
      Serial.println("Execute Command!");
#endif  // SEND_DAIKIN

      // delay(15000);
    }
  }
*/
}
