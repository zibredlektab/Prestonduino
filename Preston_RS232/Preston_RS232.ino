
#include "PrestonPacket.h"
#include "PrestonDuino.h"


#include <U8g2lib.h>

#define SAMPLES 100

PrestonDuino *mdr;

//U8G2_SSD1306_128X64_NONAME_2_HW_I2C oled (U8G2_R0);

//U8G2_SSD1322_NHD_128X64_F_3W_HW_SPI oled(U8G2_R3, 0, 1);

int count;

void setup() {

  Serial.begin(115200);

 // oled.begin();
  mdr = new PrestonDuino(Serial1);

  
  delay(100);

  mdr->mode(0x10, 0x3);
  //byte lensdata[] = {0x2, 0xFF, 0xFF};
  //mdr->data(lensdata, 3);
}

unsigned long focustotal = 0;
void loop() {
  uint16_t focus = 0;
  if (count++ < SAMPLES) {
    focustotal += analogRead(A1);
  } else {
    focus = focustotal/SAMPLES;
    focustotal = 0;
    focus = map(focus, 0, 1023, 0, 0xFFFF);
    count = 0;
    Serial.print("focus is 0x");
    Serial.println(focus, HEX);
    uint8_t focusmin = focus % 0xFF00;
    uint8_t focusmaj = (focus - focusmin) / 0x100;
    Serial.print("0x");
    Serial.print(focusmaj, HEX);
    Serial.print(" 0x");
    Serial.println(focusmin, HEX);
    byte lensdata[] = {0x2, focusmaj, focusmin};
    mdr->data(lensdata, 3);
  }
  char* modedesc = "Focus:";
  //int readout = mdr->getFocusDistance();

  /*
  oled.clearBuffer();
  
  oled.setFont(u8g2_font_logisoso18_tf);
  //command_reply lensdata = mdr->data(mode);

  oled.setCursor(11,20);
  oled.print(modedesc);
  
  oled.setCursor(11, 60);

  oled.print(readout);



  oled.sendBuffer();*/
  
}
