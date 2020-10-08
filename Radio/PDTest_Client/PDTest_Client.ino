#include <PDClient.h>
#include <U8g2lib.h>

#define LARGE_FONT u8g2_font_logisoso18_tr
#define MED_FONT u8g2_font_helvB12_tr
#define SMALL_FONT u8g2_font_pcsenior_8r

unsigned long long timenow = 0;
int wait = 4000;

PDClient *pd;

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled (U8G2_R3);

int count = 0;

void setup() {
  SerialUSB.begin(115200);
  while(!Serial);
  SerialUSB.println("hello");
  
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
    pd->subscribe(7);
    count++;
  }

  drawScreen();
  
}

void drawScreen() {


  uint16_t ap = pd->getAperture();
  uint16_t fl = pd->getFocalLength();
  uint32_t fd = pd->getFocusDistance();
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


      unsigned int ft, in;
      focusMath(fd, &ft, &in);
      
      oled.setCursor(4, 84);
      oled.setFont(LARGE_FONT);

      if (ft < 1000) {
        oled.print(ft);
        oled.print(F("'"));
        oled.print(in);
        oled.print(F("\""));
      } else {
        oled.print(F("INF"));
      }


      double irisbaserounded, irisfraction;
      irisMath(ap, &irisbaserounded, &irisfraction);
      uint8_t fractionoffset = 0;
      
      oled.setCursor(0, 108);
      oled.setFont(SMALL_FONT);
      oled.print(F("T"));
      oled.setCursor(8, 110);
      oled.setFont(LARGE_FONT);
      if (modf(irisbaserounded, NULL) > 0) {
        oled.print((int)irisbaserounded);
        oled.print(F("."));
        oled.print((int)((modf(irisbaserounded, NULL)*10)+.1));
        fractionoffset = 20;
      } else {
        oled.print((int)irisbaserounded);
      }
      
      oled.setCursor(25 + fractionoffset, 98);
      oled.setFont(SMALL_FONT);
      oled.print((int)(irisfraction*10));
      oled.drawHLine(20 + fractionoffset, 102, 18);
      oled.setCursor(22 + fractionoffset, 110); 
      oled.print(F("10"));
  
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
      }*/
    }
  } while(oled.nextPage());
}

void drawError(uint8_t errorstate) {
  oled.setFont(MED_FONT);
  oled.setCursor(0,30);
  
  switch (errorstate) {
    case 0: // no errors, why are we here
    case 1: // server communication error
      oled.print(F("No Tx?"));
      break;
    case 2: // mdr communication error
      oled.print(F("No"));
      oled.setCursor(0,44);
      oled.print(F("MDR?"));
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



void irisMath (uint16_t iris, double* irisbaserounded, double* irisfraction) {
  double irisdec, irisbase, avnumber, avbase;

  irisdec = (double)iris/100;
  avnumber = log(sq(irisdec))/log(2); // AV number for iris (number of stops)
  SerialUSB.print(F("AV is "));
  SerialUSB.println(avnumber);

  avnumber = roundf(avnumber*10);
  avnumber /= 10;

  *irisfraction = modf(avnumber, &avbase); // Fractional part of AV number (aka 10ths of stop)
  irisbase = sqrt(pow(2.0, avbase)); // Convert AV number back to F stop

  irisbase += 0.001; // Fudge some rounding errors

  if (irisbase >= 10) { // T stops above 10 are rounded to 0 decimal places
    *irisbaserounded = floor(irisbase);
  } else { // T stops below 10 are rounded to 1 place
    *irisbaserounded = floor(irisbase*10) / 10;
  }
}

void focusMath (uint32_t focus, unsigned int* ft, unsigned int* in) {
  double focusft = (double)focus / 305.0; // mm to ft
  
  double focusin, focase;
  focusin = modf(focusft, &focase); // separate whole ft from fractional ft
  focusin *= 12; // fractional ft to in

  *ft = focase+.1; // make sure the casting always rounds properly
  *in = focusin+.1;
}
