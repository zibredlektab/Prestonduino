
#include "PrestonPacket.h"
#include "PrestonDuino.h"


#include <U8g2lib.h>

PrestonDuino *mdr;

U8G2_SSD1306_128X64_NONAME_2_HW_I2C oled (U8G2_R0);


void setup() {

  oled.begin();
  mdr = new PrestonDuino(Serial1);
  
  delay(100);

  //mdr->mode(0x10, 0x2);
  //byte lensdata[] = {0x2, 0xFF, 0xFF};
  //mdr->data(lensdata, 3);
}


void loop() {
  int sliderval = analogRead(A3);
  uint32_t readout;
  char* modedesc;
  if (sliderval >= 0 && sliderval < 340) {
    readout = mdr->getAperture();
    modedesc = "Iris:";
  } else if (sliderval >= 340 && sliderval < 680) {
    readout = mdr->getFocusDistance();
    modedesc = "Focus:";
  } else if (sliderval >= 680 && sliderval < 1000) {
    readout = mdr->getFocalLength();
    modedesc = "Zoom:";
  } else {
    modedesc = mdr->getLensName();
    readout = "";
  }

  
  oled.firstPage();
  do {
    
    oled.setFont(u8g2_font_logisoso18_tf);
    //command_reply lensdata = mdr->data(mode);

    oled.setCursor(11,20);
    oled.print(modedesc);
    
    oled.setCursor(11, 60);

    //for (int i = 1; i < lensdata.replystatus; i++) {
      oled.print(readout);
    //}



  } while (oled.nextPage());
  
}
