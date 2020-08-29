#include <PDServer.h>

//#include <U8x8lib.h>

PDServer *pd;

//U8X8_SSD1306_128X64_NONAME_HW_I2C oled (NULL);

void setup() {
  // put your setup code here, to run once:
  pd = new PDServer();
  Serial.print("My address is 0x");
  Serial.println(pd->getAddress(), HEX);
 // oled.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  pd->onLoop();
/*  oled.setFont(u8x8_font_8x13_1x2_f);
  oled.setCursor(0,0);
  oled.print("hello there");*/


}
