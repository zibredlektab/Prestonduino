#include <PDClient.h>
#include <U8g2lib.h>

#define LARGE_FONT u8g2_font_logisoso18_tf
#define MED_FONT u8g2_font_helvB12_tf
#define SMALL_FONT u8g2_font_pcsenior_8f

unsigned long long timenow = 0;
int wait = 4000;

PDClient *pd;

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled (U8G2_R1);

int count = 0;

void setup() {
  Serial.begin(115200);
  oled.begin();

  oled.setFont(LARGE_FONT);
  
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(17, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  
  pd = new PDClient(0xA);

}

void loop() {
  /*uint8_t newch = readSwitch(0);
  if (newch != pd->getChannel()) {
    pd->setChannel(newch);
  }*/
  
  pd->onLoop();
  if (count == 0) {
    pd->subAperture();
    count++;
  }

  drawScreen();
  
}

void drawScreen() {

  /*uint8_t* fizdata = pd->getFIZData();
  for (int i = 0; i < 7; i++) {
    Serial.print(fizdata[i], HEX);
    Serial.print(" ");
  }
  Serial.println();*/

 /* uint16_t ap = pd->getAperture();
  uint16_t fl = 40;//pd->getFocalLength();
  uint32_t fd = 40;//pd->getFocusDistance();
  uint8_t ch = pd->getChannel();
  uint8_t er = pd->getErrorState();
  
  oled.firstPage();
  do {
    drawChannel(ch);
    
    if (er > 0) {
      drawError(er);
    } else {
  
      
      oled.setCursor(0, 20);
      oled.print(F("Primo Zm")); // Manufacturer OR series
      oled.setCursor(0, 30);
      oled.print(F("24-275mm")); // Series OR focal length range
      
      oled.setCursor(6, 56);
      oled.setFont(LARGE_FONT);
      oled.print(fl);
      oled.setFont(MED_FONT);
      oled.print(F("mm"));
      
      oled.setCursor(4, 84);
      oled.setFont(LARGE_FONT);
      oled.print(fd);
      
      oled.setCursor(6, 110);
      oled.setFont(MED_FONT);
      oled.print(F("T"));
      oled.setCursor(20, 110);
      oled.setFont(LARGE_FONT);
      oled.print(ap);
      
      oled.setCursor(8, 122);
      oled.setFont(SMALL_FONT);
      oled.print(F("& 0/10"));
  
      /*
      oled.setFont(MED_FONT);
      oled.setCursor(5,20);
      //oled.print(pd->getLensName());
      uint8_t datamode = readSwitch(0);
      switch (datamode) {
        case 0: // Iris
        oled.print("Iris");
          break;
        case 1: // Focus
        oled.print("Focus");
          break;
        case 2: // Zoom
        oled.print("Zoom");
          break;
        case 3: // Aux
        oled.print("Aux");
          break;
        case 4: // Distance
        oled.print("Dist");
          break;
        default: // Other?
          break;
      }
    }
  } while(oled.nextPage());*/
}

void drawError(uint8_t errorstate) {
  oled.setFont(MED_FONT);
  oled.setCursor(10,30);
  
  switch (errorstate) {
    case 0: // no errors, why are we here
    case 1: // server communication error
      oled.print(F("No Tx?"));
      break;
    case 2: // mdr communication error
      oled.print(F("No MDR?"));
      break;
    case 3: // mdr NAK
      oled.print(F("NAK: check MDR request"));
      break;
    case 4: // mdr ERR
      oled.print(F("ERR: check MDR request"));
      break;
    default: // other error
      oled.print(F("Unknown error: 0x"));
      oled.print(errorstate, HEX);
      break;
  }
}

void drawChannel(uint8_t channel) {
  //draw a square in the upper-right corner, then in the square:
  oled.drawHLine(0, 9, 64);
  oled.setFont(SMALL_FONT);
  oled.setCursor(12, 6);
  oled.print(F("CAM "));
  //then below that:
  oled.print(channel, HEX);
}

uint8_t readSwitch(uint8_t which) {
  // read state of a rotary switch
  if (which == 0) {
    // CH
    which += 14;
  } else if (which == 1) {
    // MODE
    which += 2;
  }
  byte state = 0;
  for (int i = 0; i < 4; i++) {
    if (digitalRead(i+which)) {
      // note that logic is inverted bc of pullup resistors
      bitClear(state, i);
    } else {
      bitSet(state, i);
    }
  }
  return state;
}

uint8_t readChannel() {
  
}
