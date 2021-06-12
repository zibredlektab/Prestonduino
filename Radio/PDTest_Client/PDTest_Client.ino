#include <SPI.h>
#include <Wire.h>
#include <PDClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/Roboto_Condensed_10.h>
#include <Fonts/Roboto_Medium_26.h>
#include <Fonts/Roboto_Condensed_12.h>

#define LARGE_FONT &Roboto_Medium_26
#define MED_FONT &Roboto_Condensed_12
#define SMALL_FONT &Roboto_Condensed_10

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

#define BTNDELAY 250

unsigned long long timenow = 0;
unsigned long long lastpush = 0;
int wait = 4000;
int currot = 2;

int displaymode = 0x02;
/*
 * Display modes:
 *  high byte: (not implemented yet)
 *    0 - no name display
 *    1 - camera name
 *    2 - lens name
 *    3 - camera and lens name
 *  low byte:
 *    0 - Vertical, FIZ (always has full display)
 *    1 - Horizontal, F
 *    2 - H, I
 *    3 - H, Z
 *    4 - H, Distance (won't show lens name)
 */

PDClient *pd;

Adafruit_SH110X oled = Adafruit_SH110X(64, 128, &Wire);


void setup() {
  
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  
  oled.begin(0x3C, true);
  oled.setTextWrap(true);
  oled.setRotation(currot);
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setFont(MED_FONT);
  oled.setCursor(0, 30);
  oled.print("Starting...\n");
  oled.display();
  /* todo
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);*/
  
  Serial.begin(115200);
  oled.print("Starting Serial...\n");
  oled.display();
  while(!Serial && millis() < 3000);
  Serial.println();
  oled.print("Serial started, or timed out.\n");
  oled.display();

  pd = new PDClient(readSwitch(0));
  
  oled.print("PDClient initialized.\n");
  oled.display();

  pd->subscribe(B100111); //FIZ data + lens name, for default display

  oled.setTextWrap(false);
}

void loop() {
  if (readSwitch(0) != pd->getChannel()) {
    delay(200);
    changeChannel(readSwitch(0));
  }

  readButtons();
  
  pd->onLoop();
  drawScreen();
  
}

void readButtons() {

  if (lastpush + BTNDELAY < millis()) {
  
    if (!digitalRead(BUTTON_A)) {
      displaymode--;
    } else if (!digitalRead(BUTTON_C)) {
      displaymode++;
    }
    
    lastpush = millis();
  
    if (displaymode > 2) displaymode = 0;
    if (displaymode < 0) displaymode = 2;
  }
}

void drawScreen() {

  uint8_t ch = pd->getChannel();
  uint8_t er = pd->getErrorState();
  
  oled.clearDisplay();
  
  if (er > 0) {
    drawError(er);
  } else {
  
    uint16_t ap = pd->getAperture();
    uint16_t fl = pd->getFocalLength();
    uint32_t fd = pd->getFocusDistance();
    const char* br = pd->getLensBrand();
    const char* sr = pd->getLensSeries();
    const char* nm = pd->getLensName();
    const char* nt = pd->getLensNote();
    
    if (br == "Other" || sr == "Other") {
      sr = nt; // If brand or series == other, use note instead of series
    }
    switch (displaymode) {
      case 0 : {

        currot = 2;
        oled.setRotation(currot);
        
        oled.setFont(LARGE_FONT);
        
        char flstr[10];
        sprintf(flstr, "%d", fl);
        uint8_t flx = getGFXStrWidth(flstr);
        
        flx += 12;
        flx = 64-flx;
        flx /= 2;
        
        oled.setCursor(flx, 56);
        oled.print(fl);
        oled.setFont(MED_FONT);
        oled.print(F("mm"));
    
        
        drawFocus(fd, 85);
    
        drawIris(ap, 2, 115);

        break;
      }

      
      case 1 : {
        currot = 3;
        oled.setRotation(currot);

        oled.setFont(MED_FONT);
        oled.setCursor(5, 45);
        oled.print("F");

        drawFocus(fd, 50);
        
        break;
      }

      

      case 2 : {
        currot = 3;
        oled.setRotation(currot);

        oled.setFont(MED_FONT);
        oled.setCursor(5, 45);
        oled.print("I");

        drawIris(ap, 40, 50);
        
        break;
        
      }
    }
    
    drawName(br,sr,nm,nt);
    
  }

  
  drawChannel(ch);
  oled.display();
}

void drawName(const char* br, const char* sr, const char* nm, const char* nt) {
  switch (currot) {
    case 0:
    case 2: {
      oled.setFont(SMALL_FONT);
      if (pd->isZoom()) {
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
      break;
    }

    case 1:
    case 3 : {
      oled.setFont(MED_FONT);
      oled.setCursor(5, 22);
      oled.print(sr);
      oled.print(" ");
      oled.print(nm);
      break;
    }
  }
}

void drawFocus(uint32_t fd, uint8_t y) {
  unsigned int ft, in;
  focusMath(fd, &ft, &in);

  
  oled.setFont(LARGE_FONT);

  if (ft < 1000) {

    char fdstr[10];
    sprintf(fdstr, "%d%d", ft, in);
    int8_t fdx = getGFXStrWidth(fdstr);
    fdx += 9;
    fdx = oled.width()-fdx;
    fdx /= 2;

    
    oled.setCursor(fdx, y);
    oled.print(ft);
    oled.setFont(MED_FONT);
    oled.setCursor(oled.getCursorX(), oled.getCursorY()-10);
    oled.print(F("'"));
    oled.setFont(LARGE_FONT);
    oled.setCursor(oled.getCursorX(), oled.getCursorY()+10);
    oled.print(in);
    oled.setFont(MED_FONT);
    oled.setCursor(oled.getCursorX(), oled.getCursorY()-10);
    oled.print(F("\""));
  } else {
    
    oled.setCursor(getCenteredX("INF"), y);
    oled.print(F("INF"));
  }
}

void drawIris(uint16_t ap, uint8_t x, uint8_t y) {
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
  
  uint8_t irisx = x;
  
  oled.setCursor(irisx, y); //108
  oled.setFont(SMALL_FONT);
  oled.print(F("T")); //5pix
  oled.setCursor(irisx + 6, y + 2); //110
  oled.setFont(LARGE_FONT);
  oled.print(irislabel);

  uint8_t fractionx = irisx + 45;
  
  oled.setCursor(fractionx + 5, y - 9); //101
  oled.setFont(SMALL_FONT);
  oled.print((int)(irisfraction*10));
  oled.drawFastHLine(fractionx, y - 8, 16, SH110X_WHITE); 
  oled.setCursor(fractionx + 2, y + 2); //112
  oled.print(F("10"));
}

void drawError(uint8_t errorstate) {
  oled.setFont(MED_FONT);
  
  oled.setRotation(3);
  oled.setTextWrap(true);
  oled.setCursor(15,15);

  if (errorstate & 1) {
    // server communication error
    oled.print(F("No Tx?"));
  } else if (errorstate & 2) {
    // no data
    oled.print(F("ERR: no data recieved"));
  } else if (errorstate & 4) {
    // mdr communication error
    oled.print(F("No MDR?"));
  } else if (errorstate & 8) {
    // mdr NAK or ERR
    oled.print(F("Check MDR request"));
  } else {
    // other error
    oled.print(F("Unknown error: b"));
    oled.print(errorstate, BIN);
  }

  oled.setRotation(currot);
  oled.setTextWrap(false);
  
}

void drawChannel(uint8_t channel) {
  oled.drawFastHLine(0, 10, oled.width(), SH110X_WHITE);
  oled.setFont(SMALL_FONT);
  oled.setCursor(getCenteredX("Camera Z"), 8);
  oled.print(F("Camera "));
  oled.print(channel, HEX);
}

uint8_t readSwitch(uint8_t which) {
  return 0xA;/*
  // read state of a rotary switch
  if (which == 0) {
    // CH
    which += 4;
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
  return state;*/
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
  double focusft = (double)focus / 305.0; // mm to fractional ft
  
  double focusin, focusbase;
  focusin = modf(focusft, &focusbase); // separate whole ft from fractional ft
  focusin *= 12; // fractional ft to in

  *ft = focusbase+.1; // make sure the casting always rounds properly
  *in = focusin+.1;
}

int8_t getCenteredX(const char* str) {
  int8_t width = getGFXStrWidth(str);
  return (oled.width()-width)/2;
}

uint16_t getGFXStrWidth(const char* str) {
  uint16_t width = 0;
  oled.getTextBounds(str, oled.getCursorX(), oled.getCursorY(), NULL, NULL, &width, NULL);
  return width;
}

void changeChannel(uint8_t newch) {
  oled.clearDisplay();
  oled.setCursor(getCenteredX("Changing"),30);
  oled.print(F("Changing"));
  oled.setCursor(getCenteredX("channel..."),45);
  oled.print(F("channel..."));
  oled.display();
  
  pd->unsub();
  if (pd->setChannel(newch)) {
    pd->subscribe(39);
  } else {
    Serial.println(F("failed to set new channel"));
  }
}
