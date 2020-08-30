#include <PDClient.h>
#include <U8g2lib.h>

#define LARGE_FONT u8g2_font_logisoso18_tf
#define MED_FONT u8g2_font_6x12_tn
#define SMALL_FONT u8g2_font_u8glib_4_tf


PDClient *pd;

U8G2_SSD1306_128X64_NONAME_2_HW_I2C oled (U8G2_R0);

void setup() {
  Serial.begin(115200);
  oled.begin();

  oled.setFont(LARGE_FONT);
  
  pd = new PDClient(map(analogRead(A0), 0, 1023, 0, 0xF));



}

void loop() {
  pd->onLoop();
  drawScreen();
}

void drawScreen() {
  oled.firstPage();
  do {
    if (pd->getErrorState() > 0) {
      drawError(pd->getErrorState());
    } else {
      drawChannel(pd->getChannel());
      //draw lens name
      //draw data descriptor
      //draw data
    }
  } while (oled.nextPage());
}

void drawError(uint8_t errorstate) {
  switch (errorstate) {
    case 0: // no errors, why are we here
    case 1: // server communication error
      oled.print("No Tx?");
      break;
    case 2: // mdr communication error
      oled.print("No MDR?");
      break;
    case 3: // mdr NAK
      oled.print("NAK: check MDR request");
      break;
    case 4: // mdr ERR
      oled.print("ERR: check MDR request");
      break;
    default: // other error
      oled.print("Unknown error: 0x");
      oled.print(errorstate, HEX);
      break;
  }
}

void drawChannel(uint8_t channel) {
  //draw a square in the upper-right corner, then in the square:
  oled.drawFrame(108, -1, 21, 21);
  oled.setFont(SMALL_FONT);
  oled.setCursor(118, 6);
  oled.print("CAM");
  //then below that:
  oled.setFont(MED_FONT);
  oled.setCursor(118, 18);
  oled.print(channel);
}
