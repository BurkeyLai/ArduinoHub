#include <IRremoteESP8266.h>
#include <IRsend.h>

#define IR_LED_PIN D1 // 將 LED 連接到 NodeMCU 的 D1 腳位

IRsend irsend(4);

void setup() {
  Serial.begin(115200);
  pinMode(IR_LED_PIN, OUTPUT);
}

void loop() {
  // 控制外接 LED 閃爍
  digitalWrite(IR_LED_PIN, HIGH);
  // irsend.sendNEC(0xF7C03F, 32);
  delay(1000); // 延遲一秒
  digitalWrite(IR_LED_PIN, LOW);
  delay(1000); // 再次延遲一秒
}

