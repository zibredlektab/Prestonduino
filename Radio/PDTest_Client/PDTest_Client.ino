#include <SPI.h>
#include <Wire.h>
#include <PDClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include <FlashStorage.h>
#include <FlashAsEEPROM.h>
//#include <Fonts/pixelmix4pt7b.h>
//#include <Fonts/Roboto_Medium_26.h>
#include <Fonts/FreeSerifItalic9pt7b.h>
//#include <Fonts/Roboto_34.h>


#define CHAR_FONT &FreeSerifItalic9pt7b
#define XLARGE_FONT CHAR_FONT//&Roboto_34
#define LARGE_FONT CHAR_FONT//&Roboto_Medium_26
#define SMALL_FONT CHAR_FONT//&pixelmix4pt7b

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

#define FLASHTIME 500
#define MODESTOREDELAY 2000
#define BSAFETY 3000

#define DATA DATA_FOCUS + DATA_IRIS + DATA_ZOOM + DATA_NAME

#define BIG true
#define SMALL false
#define X_OFFSET_BIG 5
#define Y_OFFSET_BIG 48
#define X_OFFSET_SMALL 90
#define Y_OFFSET_BTM 15
#define Y_OFFSET_TOP 30

bool ignoreerrors = true;

bool editingchannel = false;

int displaymode;
bool changingmodes = true;
long long int timemodechanged;
FlashStorage(storedmode, int);
bool showna = true;

/*
 * Display modes:
 *  0 - F, zi
 *  1 - I, fz
 *  2 - Z, if
 */

PDClient *pd;

Adafruit_SH1107 oled(64, 128, &Wire);

FlashStorage(storedchannel, int); // channel is stored as 0x10 + the actual channel, to allow for the channel to be 0

int channel;


void setup() {

  channel = storedchannel.read(); // get the previously-set channel from memory
  
  if (channel == 0) {
    // no channel has ever been set (channel 0 would be saved as 0x10)
    storedchannel.write(0x1A);
    channel = 0xA;
  } else {
    channel -= 0x10;
  }
  
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

  pd = new PDClient(channel);
  
  oled.print("PDClient initialized.\n");
  oled.display();

  pd->subscribe(DATA); //FIZ data + lens name, for default display

  oled.setTextWrap(false);

  displaymode = storedmode.read();

  timemodechanged = millis();
}

void loop() {
  readButtons();
  
  pd->onLoop();
  drawScreen();

  if (changingmodes) {
    if (timemodechanged + MODESTOREDELAY < millis()) {
      if (millis() > 6000) {
        storedmode.write(displaymode);
      }
      changingmodes = false;
      showna = false;
    }
  }
}

void readButtons() {
  bool adown = !digitalRead(BUTTON_A);
  bool bdown = !digitalRead(BUTTON_B);
  bool cdown = !digitalRead(BUTTON_C);
  static long long int bdowntime;
  static bool adownlastloop = false;
  static bool bdownlastloop = false;
  static bool cdownlastloop = false;
  bool aprocess = false;
  bool bprocess = false;
  bool cprocess = false;

  if (adown) {
    if (!adownlastloop) {
      adownlastloop = true;
      aprocess = true;
    }
  } else {
    adownlastloop = false;
  }

  if (cdown) {
    if (!cdownlastloop) {
      cdownlastloop = true;
      cprocess = true;
    }
  } else {
    cdownlastloop = false;
  }

  if (bdown) {
    if (!bdownlastloop) {
      bdownlastloop = true;
      bprocess = true;
    }
  } else {
    bdownlastloop = false;
  }
  

    if (!editingchannel) {
    
      if (bprocess) {
        bdowntime = millis();
      } else if (bdown) {
        if (bdowntime + BSAFETY < millis()) {
          // B button must be held down for BSAFETY seconds to engage editing
          editingchannel = true;
        }
      }
      
      if (aprocess) {
        changeMode(-1);
      } else if (cprocess) {
        changeMode(1);
      }
      
      
    } else {
      
      if (bprocess) {
          // prevent immediate exit of channel editing by holding down B too long - must be released for at least one loop
        editingchannel = false;
        setChannel();
      }

      if (aprocess) {
        changeChannel(-1);
      } else if (cprocess){
        changeChannel(1);
      }
    }
  
}

void changeMode(int addend) {
  changingmodes = true;
  timemodechanged = millis();
  showna = true;
  displaymode += addend;
  if (displaymode > 2) displaymode = 0;
  if (displaymode < 0) displaymode = 2;
}

void changeChannel(int addend) {
  channel += addend;
  if (channel > 0xF) channel = 0;
  if (channel < 0) channel = 0xF;
  storedchannel.write(channel + 0x10);
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

  
  drawChannel(channel);
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
    x = X_OFFSET_BIG;
    y = Y_OFFSET_BIG;
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
  if (ft <= 1000) {      
    oled.print(ft);
    oled.print("'");
    if (ft >= 100 && big) {
      oled.setCursor(oled.getCursorX() - 4, oled.getCursorY());
      oled.setFont(SMALL_FONT);
    }
    oled.print(in);
    oled.print(F("\""));
  } else {
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
    x = X_OFFSET_BIG;
    y = Y_OFFSET_BIG;
    uint8_t irisx = x;
    
    oled.setCursor(irisx, y);

    if (ap == 0) {
      oled.setFont(XLARGE_FONT);
      oled.print("No Iris");
    } else {
    
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

    if (ap == 0) {
      if (showna) {
        oled.print("No Iris");
      }
    } else {
      oled.print("*");
      oled.print(irislabel);
      oled.setCursor(x, y + 10);
      oled.print((int)(irisfraction*10));
      oled.print("/");
      oled.print("10");
    }
  }
}


void drawZoom(uint8_t fl, bool big) {
  uint8_t x, y;

  if (big) {
    x = X_OFFSET_BIG;
    y = Y_OFFSET_BIG;
    
    oled.setFont(XLARGE_FONT);
    oled.setCursor(x, y);

    if (fl == 0) {
      oled.print("No Zoom");
    } else {
      oled.print(fl);
      oled.setFont(SMALL_FONT);
      oled.print(F("mm"));
    
      if (pd->isZoom()) {
        oled.drawLine(x, y + 7, x + 62, y + 7, SH110X_WHITE); // horiz scale
        uint8_t zoompos = map(fl, pd->getWFl(), pd->getTFl(), x, x + 62);
        oled.drawLine(zoompos, y + 4, zoompos, y + 10, SH110X_WHITE); // pointer
      }
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

    if (fl == 0) {
      if (showna) {
        oled.print("No Zoom");
      }
    } else {
      oled.print(fl);
      oled.print("mm");
    }
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
  static long long int flashtranstime; // the time at which the last flash transition happened
  static bool flashing = false; // whether the channel indicator has begun flashing
  static bool transitioning = true; // whether the channel indicator is currently switching between light/dark
  static uint16_t textcolor, bgcolor;

  if (editingchannel) { // globally, channel editing is occurring
    
    if (!flashing) { // channel editing began since the last cycle, we have not yet started flashing
      flashing = true;
      transitioning = true; // flashing should begin with an immediate transition
    }
    
    if (!transitioning) {
      if (flashtranstime + FLASHTIME < millis()) {
        transitioning = true;
      }
      
    } else {
      // transition! swap bg and text colors.

      if (textcolor == SH110X_BLACK) {
        textcolor = SH110X_WHITE;
        bgcolor = SH110X_BLACK;
      } else {
        textcolor = SH110X_BLACK;
        bgcolor = SH110X_WHITE;
      }
      
      flashtranstime = millis();
      transitioning = false;
    }
    
  } else {
    // global channel editing is not occuring, so stop flashing and set default colors.
    flashing = false;
    textcolor = SH110X_BLACK;
    bgcolor = SH110X_WHITE;
  }
  
  oled.drawRect(115, 0, 12, 12, SH110X_WHITE);
  oled.fillRect(116, 0, 11, 11, bgcolor);
  oled.setFont(SMALL_FONT);
  oled.setTextColor(textcolor);
  oled.setCursor(118, 8);
  oled.print(channel, HEX);
  
  oled.setTextColor(SH110X_WHITE); // reset the text color to white for the next function
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

void setChannel() {
  if (channel != pd->getChannel()) {
    if (pd->setChannel(channel)) {
      pd->subscribe(DATA);
    } else {
      Serial.println(F("failed to set new channel"));
    }
  }
}
