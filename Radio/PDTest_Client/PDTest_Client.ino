#include <SPI.h>
#include <Wire.h>
#include <PDClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/pixelmix4pt7b.h>
#include <Fonts/Roboto_Medium_26.h>
#include <Fonts/FreeSerifItalic9pt7b.h>
#include <Fonts/Roboto_34.h>

#define XLARGE_FONT &Roboto_34
#define LARGE_FONT &Roboto_Medium_26
#define SMALL_FONT &pixelmix4pt7b
#define CHAR_FONT &FreeSerifItalic9pt7b

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

#define BTNDELAY 250
#define BIG true
#define SMALL false
#define X_OFFSET_SMALL 90
#define Y_OFFSET_BTM 15
#define Y_OFFSET_TOP 30


unsigned long long timenow = 0;
unsigned long long lastpush = 0;
int wait = 4000;

bool ignoreerrors = true;

int displaymode = 1;
/*
 * Display modes:
 *  0 - F, zi
 *  1 - I, fz
 *  2 - Z, if
 */

PDClient *pd;

Adafruit_SH110X oled = Adafruit_SH110X(64, 128, &Wire);


void setup() {
  
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  if (!digitalRead(BUTTON_B)) {
    ignoreerrors = false;
  }
  
  oled.begin(0x3C, true);
  oled.setTextWrap(true);
  oled.setRotation(3);
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setFont(SMALL_FONT);
  oled.setCursor(0, 30);
  
  Serial.begin(115200);
  oled.print("Starting Serial...\n");
  oled.display();
  while(!Serial && millis() < 3000);
  Serial.println();
  oled.print("Serial started, or timed out.\n");
  oled.display();

  pd = new PDClient(0xA);
  
  oled.print("PDClient initialized.\n");
  oled.display();

  pd->subscribe(DATA_FOCUS + DATA_IRIS + DATA_ZOOM + DATA_NAME); //FIZ data + lens name, for default display

  oled.setTextWrap(false);
}

void loop() {
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
  
  if (er > 0 && !ignoreerrors) {
    drawError(er);
  } else {
  
    uint16_t ap = pd->getAperture();
    uint16_t fl = pd->getFocalLength();
    uint32_t fd = pd->getFocusDistance();
    const char* br = pd->getLensBrand();
    const char* sr = pd->getLensSeries();
    const char* nm = pd->getLensName();
    const char* nt = pd->getLensNote();
    
    switch (displaymode) {
      case 0 : {
        drawFocus(fd, BIG);
        drawZoom(fl, SMALL);
        drawIris(ap, SMALL);
        break;
      }

      
      case 1 : {
        drawIris(ap, BIG);
        drawFocus(fd, SMALL);
        drawZoom(fl, SMALL);
        break;
      }

      

      case 2 : {
        drawZoom(fl, BIG);
        drawIris(ap, SMALL);
        drawFocus(fd, SMALL);
        break;
        
      }
    }
    
    drawName(br,sr,nm,nt);
    
  }

  
  drawChannel(ch);
  oled.display();
}

void drawName(const char* br, const char* sr, const char* nm, const char* nt) {
    oled.setFont(SMALL_FONT);
    oled.setCursor(0, 8);
    oled.print(sr);
    oled.print(" ");
    oled.print(nm);
    oled.print("\n");
    oled.print(nt);
}

void drawFocus(uint32_t fd, bool big) {

  uint8_t x, y;
  
  unsigned int ft, in;
  focusMath(fd, &ft, &in);


  if (big) {
    x = 5;
    y = 48;
    oled.setFont(XLARGE_FONT);
  
  } else {
    x = X_OFFSET_SMALL;
    if (oled.getCursorY() > 42) {
      y = Y_OFFSET_TOP;
    } else {
      y = oled.getCursorY() + Y_OFFSET_BTM;
    }

    oled.setFont(SMALL_FONT);
  }

  oled.setCursor(x, y);
  if (ft < 1000) {      
    oled.print(ft);
    oled.print("'");
    if (ft < 100) {
      oled.print(in);
      oled.print(F("\""));
    }
  } else {
    oled.setCursor(getCenteredX("INF"), y);
    oled.print("INF");
  }
  
}

void drawIris(uint16_t ap, bool big) {
  uint8_t x, y;
    
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
  
  if (big) {
    x = 10;
    y = 48;
    uint8_t irisx = x;
    
    oled.setCursor(irisx, y);
    oled.setFont(SMALL_FONT);
    oled.print(F("T")); //5pix
    oled.setCursor(irisx + 8, y);
    oled.setFont(XLARGE_FONT);
    oled.print(irislabel);
  
    uint8_t fractionx = oled.getCursorX() + 1;
    
    oled.setCursor(fractionx + 5, y - 10);
    oled.setFont(SMALL_FONT);
    oled.print((int)(irisfraction*10));
    oled.drawFastHLine(fractionx, y - 8, 16, SH110X_WHITE); 
    oled.setCursor(fractionx + 3, y);
    oled.print(F("10"));
    
  } else {
    x = X_OFFSET_SMALL;
    if (oled.getCursorY() > 42) {
      y = Y_OFFSET_TOP;
    } else {
      y = oled.getCursorY() + Y_OFFSET_BTM;
    }

    oled.setCursor(x, y);
    oled.setFont(SMALL_FONT);
    oled.print("*");
    oled.print(irislabel);
    oled.setCursor(x, y + 10);
    oled.print((int)(irisfraction*10));
    oled.print("/");
    oled.print("10");
  }
}


void drawZoom(uint8_t fl, bool big) {
  uint8_t x, y;

  if (big) {
    x = 8;
    y = 45;
    
    oled.setFont(XLARGE_FONT);
    oled.setCursor(x, y);
    oled.print(fl);
    oled.setFont(SMALL_FONT);
    oled.print(F("mm"));
  
    if (pd->isZoom()) {
      oled.drawLine(x, y + 7, x + 62, y + 7, SH110X_WHITE); // horiz scale
      uint8_t zoompos = map(fl, pd->getWFl(), pd->getTFl(), x, x + 62);
      oled.drawLine(zoompos, y + 4, zoompos, y + 10, SH110X_WHITE); // pointer
    }
  } else {
    x = X_OFFSET_SMALL;
    if (oled.getCursorY() > 42) {
      y = Y_OFFSET_TOP;
    } else {
      y = oled.getCursorY() + Y_OFFSET_BTM;
    }

    oled.setCursor(x, y);
    oled.setFont(SMALL_FONT);
    oled.print(fl);
    oled.print("mm");
  }
}





void drawError(uint8_t errorstate) {
  oled.setFont(SMALL_FONT);
  oled.setTextWrap(true);
  oled.setCursor(15,15);

  if (errorstate == ERR_NOTX) {
    // server communication error
    oled.print(F("No Tx?"));
  } else if (errorstate == ERR_NODATA) {
    // no data
    oled.print(F("ERR: no data recieved"));
  } else if (errorstate == ERR_NOMDR) {
    // mdr communication error
    oled.print(F("No MDR?"));
  } else if (errorstate == ERR_MDRERR) {
    // mdr NAK or ERR
    oled.print(F("Check MDR request"));
  } else if (errorstate == ERR_RADIO) {
    oled.print(F("Radio setup failed"));
  } else {
  
    // other error
    oled.print(F("Unknown error: b"));
    oled.print(errorstate, BIN);
  }
  oled.setTextWrap(false);
}

void drawChannel(uint8_t channel) {
  oled.setTextColor(SH110X_BLACK);
  oled.fillRect(115, 0, 12, 12, SH110X_WHITE);
  oled.setFont(SMALL_FONT);
  oled.setCursor(118, 8);
  oled.print(channel, HEX);
  oled.setTextColor(SH110X_WHITE);
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
