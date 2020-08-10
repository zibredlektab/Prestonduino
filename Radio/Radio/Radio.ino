#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#include <U8x8lib.h>

#define SERVER_ADDRESS 0
#define CLIENT_ADDRESS 1


RH_RF95 driver;
RHReliableDatagram manager(driver, SERVER_ADDRESS);

U8X8_SSD1306_128X64_NONAME_HW_I2C oled (NULL);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  if (!manager.init()) {
    Serial.println("init failed");
  }
  oled.begin();
  
  oled.setFont(u8x8_font_amstrad_cpc_extended_r);
}

uint8_t buf[5];
uint8_t fromaddress;
uint8_t reply[] = "Got it!";

void loop() {
  
  if (manager.available()) {
    oled.clearDisplay();
    uint8_t len = sizeof(buf);
    if (manager.recvfrom(buf, &len, &fromaddress)){
      //manager.sendtoWait(reply, sizeof(reply), fromaddress);
      Serial.print("got a request from: 0x");
      Serial.print(fromaddress, HEX);
      Serial.print(": ");
      for (int i = 0; i < len; i++) {
        Serial.print((char)buf[i]);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
  
  oled.setCursor(0, 0);
  oled.print(fromaddress);

  oled.setCursor(0, 20);
  oled.print((char*)buf);


}
