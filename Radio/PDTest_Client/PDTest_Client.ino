#include <PDClient.h>
#include <U8g2lib.h>

#define LARGE_FONT u8g2_font_profont22_tr
#define MED_FONT u8g2_font_profont12_tr
#define SMALL_FONT u8g2_font_profont12_tr

unsigned long long timenow = 0;
int wait = 4000;

PDClient *pd;

U8G2_SSD1322_NHD_128X64_1_3W_HW_SPI oled(U8G2_R3, 4, 3);
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled (U8G2_R3);

uint8_t count = 0;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println();

  pd = new PDClient(0xA);
  
  oled.begin();

  oled.setFont(LARGE_FONT);

  /*
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(17, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  */

}

void loop() {
  /*uint8_t newch = readSwitch(0);
  if (newch != pd->getChannel()) {
    pd->setChannel(newch);
  }*/
  
  pd->onLoop();
  if (count == 0) {
    //pd->subscribe(39);
    Serial.println(F("Subscribed."));
    count++;
  }

  drawScreen();
  
}

void drawScreen() {
  
  uint16_t ap = 460;//pd->getAperture();
  uint16_t fl = 30;//pd->getflstr();
  uint32_t fd = 21900;//pd->getFocusDistance();
  uint8_t ch = 0xA;//pd->getChannel();
  uint8_t er = 0x0;//pd->getErrorState();
  const char* br = "Panavision";//pd->getLensBrand();
  const char* sr = "Primo Zoom";//pd->getLensSeries();
  const char* nm = "24-250mm";//pd->getLensName();
  const char* nt = "foo";//pd->getLensNote();
  
  //oled.clearBuffer();
  oled.firstPage();
  do {
    drawChannel(ch);
    
    if (er > 0) {
      drawError(er);
    } else {
  
      
      if (0){//pd->isZoom()) {
        // Zoom displays series name & focal range
        oled.setCursor(getCenteredX(sr), 20);
        oled.print(sr);
        oled.setCursor(getCenteredX(nm), 30);
        oled.print(nm);
      } else {
        // Prime displays brand & series name
        oled.setCursor(getCenteredX(br), 20);
        oled.print(br);
        oled.setCursor(getCenteredX(sr), 30);
        oled.print(sr);
      }
      
      oled.setFont(LARGE_FONT);
      
      char flstr[10];
      sprintf(flstr, "%d", fl);
      uint8_t flx = oled.getStrWidth(flstr);
      
      flx += 12;
      flx = 64-flx;
      flx /= 2;
      
      oled.setCursor(flx, 56);
      oled.print(fl);
      oled.setFont(MED_FONT);
      oled.print(F("mm"));
  
 
      unsigned int ft, in;
      focusMath(fd, &ft, &in);
      
      oled.setFont(LARGE_FONT);
  
      if (ft < 1000) {

        char fdstr[10];
        sprintf(fdstr, "%d%d", ft, in);
        uint8_t fdx = oled.getStrWidth(fdstr);
        fdx += 9;
        fdx = 64-fdx;
        fdx /= 2;

        
        oled.setFontPosTop();
        oled.setCursor(fdx, 66);
        oled.print(ft);
        oled.setFont(SMALL_FONT);
        oled.print(F("'"));
        oled.setFont(LARGE_FONT);
        oled.print(in);
        oled.setFont(SMALL_FONT);
        oled.print(F("\""));
        oled.setFontPosBaseline();
      } else {
        
        oled.setCursor(getCenteredX("INF"), 82);
        oled.print(F("INF"));
      }
  


  
      double irisbaserounded, irisfraction;
      irisMath(ap, &irisbaserounded, &irisfraction);

      uint8_t iriswidth = 32; //T, one digit, fraction
      char irislabel[4];
      double temp; // temporary place to store the (unused) fractional part of iris

      if (modf(irisbaserounded, &temp) > 0) {
        iriswidth += 24;
        sprintf(irislabel, "%d.%d", (int)irisbaserounded, (int)((modf(irisbaserounded, &temp)*10)+.1));
      } else {
        sprintf(irislabel, "%d", (int)irisbaserounded);
      }
      
      uint8_t irisx = (64-iriswidth)/2;
      
      oled.setCursor(irisx, 108);
      oled.setFont(SMALL_FONT);
      oled.print(F("T")); //5pix
      oled.setCursor(irisx + 8, 110);
      oled.setFont(LARGE_FONT);
      oled.print(irislabel);

      uint8_t fractionx = irisx + iriswidth - 12;
      
      oled.setCursor(fractionx + 5, 101);
      oled.setFont(SMALL_FONT);
      oled.print((int)(irisfraction*10));
      oled.drawHLine(fractionx, 102, 16); //16pix
      oled.setCursor(fractionx + 2, 112); 
      oled.print(F("10"));
    }
  } while(oled.nextPage());
  //oled.sendBuffer();
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
  oled.drawHLine(0, 10, 64);
  oled.setFont(SMALL_FONT);
  oled.setCursor(getCenteredX("Camera Z"), 8);
  oled.print(F("Camera "));
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
  //Serial.print(F("AV is "));
  //Serial.println(avnumber);

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

uint8_t getCenteredX(char* str) {
  uint8_t width = oled.getStrWidth(str);
  return (64-width)/2;
}
