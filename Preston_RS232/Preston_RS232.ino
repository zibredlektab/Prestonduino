#include <U8g2lib.h>

#include "PrestonPacket.h"
#include "PrestonDuino.h"


U8G2_SSD1306_128X64_NONAME_2_HW_I2C oled (U8G2_R0);

PrestonDuino *mdr;

int iris = 0;

void setup() {

  mdr = new PrestonDuino(Serial);
  oled.begin();
  
  
}

char data[10];

void loop() {
  iris = mdr->getAperture();
  
  snprintf(data, sizeof(data), "%lu", iris);
  
  oled.firstPage();
  do {
    oled.setFont(u8g2_font_helvB12_tf);
    oled.setCursor(0, 14);
    oled.print(iris);

    
    oled.setCursor(0, 40);
    oled.print(millis());
    
  } while (oled.nextPage());
}
