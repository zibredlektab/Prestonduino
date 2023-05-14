#include <InputDebounce.h>
#include <SPI.h>
#include <Wire.h>
#include <PDClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include "Fonts/pixelmix4pt7b.h"
#include "Fonts/Roboto_Medium_26.h"
#include "Fonts/Roboto_34.h"
#define XLARGE_FONT &Roboto_34
#define LARGE_FONT &Roboto_Medium_26
#define SMALL_FONT &pixelmix4pt7b

#define BIG true
#define SMALL false
#define X_OFFSET 5
#define Y_OFFSET 48
#define Y_OFFSET_BTM 15
#define Y_OFFSET_TOP 30

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5
#define BUTTON_DEBOUNCE_DELAY   20   // [ms]

PDClient *pd;

Adafruit_SH1107 oled(64, 128, &Wire);

static InputDebounce btn_a;
static InputDebounce btn_b;
static InputDebounce btn_c;
static InputDebounce up;
static InputDebounce down;

int channel = 0xA;
bool changingchannel = false;
unsigned long long int timechannelchanged;

int dialog = 0; // see below
/*
 * Dialogs:
 * 0 - none
 * 1 - menu
 * 2 - map now?
 * 3 - WFO selection
 * 4 - Mapping
 * 5 - OneRing settings
 */

int menuselected = 0;
bool submenu = false;
int choffset = 0;


static char wholestops[10][4] = {"1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11\0", "16\0", "22\0"};
uint8_t curmappingav = 1;

void drawRect(int x, int y, int w, int h, int color = 0, bool trace = true, int tracecolor = 1) {
  oled.fillRect(x, y, w, h, color) ;
  if (trace) {
    oled.drawRect(x, y, w, h, tracecolor);
  }
}

void callback_pressed(uint8_t pinIn) {
  //Serial.print("Button pressed: ");
  //Serial.print(pinIn);
  //Serial.print(", dialog = ");
  //Serial.println(dialog);

  if (pinIn == 18) {
    pd->setIris(pd->getIris() + 1000);
  } else if (pinIn == 17) {
    pd->setIris(pd->getIris() - 1000);
  } else {
  
    switch(dialog) {
      case 0: {
        switch(pinIn) {
          case BUTTON_A: {
            pd->setIris(pd->getIris() + 1000);
            break;
          }
          case BUTTON_B: {
            pd->setIris(pd->getIris() - 1000);
            break;
          }
        }
        //dialog = 1; // open main menu
        break;
      }

      case 1: { // Main menu
        switch(pinIn) {
          case BUTTON_A: { // down nav
            doNav(-1);
            break;
          }
          
          case BUTTON_B: { // "ok"
            switch (menuselected) {
              case 0: {
                if (submenu) {
                  //changeChannel(choffset);
                  choffset = 0;
                }
                submenu = !submenu;
                break;
              }
              
              case 1: {
                submenu = !submenu;
                break;
              }
              
              case 2: {
                dialog = 5;
                menuselected = 0;
                break;
              }
              
              case 3: {
                menuselected = 0;
                Serial.println("closing menu");
                dialog = 0;
                break;
              }
            }
            break;
          }
          
          case BUTTON_C: { // up nav
            doNav(1);
            break;
          }
        }

        break;
      }

      case 2: { // Map lens now?
        switch(pinIn) {
          case BUTTON_A: {
            pd->mapLater();
            dialog = 0;
            break;
          }
          
          case BUTTON_C: {
            Serial.println("map now");
            pd->startMap();
            dialog = 3;
            break;
          }
        }
        
        break;
      }

      case 3: { // Select WFO
        switch(pinIn) {
          case BUTTON_A: {
            curmappingav--;
            if (curmappingav < 0) {
              curmappingav = 0;
            }
            break;
          }

          case BUTTON_B: {
            Serial.println("WFO selected");
            dialog = 4;
            break;
          }
          
          case BUTTON_C: {
            curmappingav++;
            if (curmappingav > 9) {
              curmappingav = 9;
            }
            break;
          }
        }
        break;
      }
        
      case 4: { // Set lens
        switch(pinIn) {
          case BUTTON_A: { // Finish
            pd->finishMap();
            curmappingav = 1;
            dialog = 0;
            break;
          }

          case BUTTON_B: { // ok
            Serial.println("Saving AV position");
            pd->mapLens(curmappingav++);
            if (curmappingav == 10) {
              pd->finishMap();
              curmappingav = 1;
              dialog = 0;
            }
            break;
          }
          
          case BUTTON_C: { // Back
            curmappingav--;
            break;
          }
        }
        
        break;
      }

      case 5: { // OneRing menu
        switch(pinIn) {
          case BUTTON_A: { // down nav
            doNav(-1);
            break;
          }
          
          case BUTTON_B: { // "ok"
            switch (menuselected) {
              case 0: {
                if (submenu) {
                  changeChannel(choffset);
                  choffset = 0;
                }
                submenu = !submenu;
                break;
              }
              
              case 1: {
                submenu = !submenu;
                break;
              }
              
              case 2: {
                dialog = 5;
                menuselected = 0;
                break;
              }
              
              case 3: {
                menuselected = 0;
                Serial.println("closing menu");
                dialog = 0;
                break;
              }
            }
            break;
          }
          
          case BUTTON_C: { // up nav
            doNav(1);
            break;
          }
        }

        break;
      }
    }
  }
}



void doNav(int dir) {
  if (submenu) {
    switch (menuselected) {
      case 0: {
        choffset += dir;
        if (channel + choffset < 0) {
          choffset = 0xF - channel;
        } else if (channel + choffset > 0xF) {
          choffset = channel * -1;
        }
        break;
      }
      case 1: {
        break;
      }
    }
  } else {
    menuselected -= dir;
    if (menuselected >= 4) {
      menuselected = 0;
    } else if (menuselected < 0) {
      menuselected = 3;
    }
  }
}

void setup() {
  btn_a.registerCallbacks(NULL, callback_pressed);
  btn_b.registerCallbacks(NULL, callback_pressed);
  btn_c.registerCallbacks(NULL, callback_pressed);
  up.registerCallbacks(NULL, callback_pressed);
  down.registerCallbacks(NULL, callback_pressed);
  
  btn_a.setup(BUTTON_A, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
  btn_b.setup(BUTTON_B, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
  btn_c.setup(BUTTON_C, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
  up.setup(18, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
  down.setup(17, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);

  oled.begin(0x3C, true);
  oled.setTextWrap(true);
  oled.setRotation(3);
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setFont(SMALL_FONT);
  oled.setCursor(0, 30);
  
  Serial.begin(9600);
  oled.print("Starting Serial...\n");
  oled.display();
  while(!Serial && millis() < 3000);
  Serial.println();
  oled.print("Serial started, or timed out.\n");
  oled.display();

  pd = new PDClient(channel);
  
  oled.print("PDClient initialized.\n");
  oled.display();
  oled.setTextWrap(false);
}

unsigned long long ledtime;
bool ledon;

void loop() {

  if (millis() > ledtime+500) {
    ledon ? digitalWrite(13, LOW) : digitalWrite(13, HIGH);
    ledon = !ledon;
    ledtime = millis();
  }

  readButtons();
  pd->onLoop();

  if (pd->isNewLens() && !pd->isLensMapped()) dialog = 2;

  drawScreen();

  if (changingchannel) {
    //if (timechannelchanged + STOREDELAY < millis()) {
      if (millis() > 6000) {
        //storedchannel.write(channel + 0x10);
        setChannel();
      }
      changingchannel = false;
    //}
  }
}

void readButtons() {
  unsigned long now = millis();
  
  // poll button state
  btn_a.process(now);
  btn_b.process(now);
  btn_c.process(now);
  up.process(now);
  down.process(now);
}

void changeChannel(int addend) {
  changingchannel = true;
  timechannelchanged = millis();
  channel += addend;
  if (channel > 0xF) channel = 0;
  if (channel < 0) channel = 0xF;
}

void drawScreen() {
  uint8_t ch = pd->getChannel();
  uint8_t er = pd->getErrorState();
  oled.clearDisplay();
  if (dialog > 0) {
    drawDialog();
  } else {
    if (er > 0) {
      //drawError(er);
    } else {
    
      uint16_t ap = pd->getIris();
      const char* br = pd->getLensBrand();
      const char* sr = pd->getLensSeries();
      const char* nm = pd->getLensName();
      const char* nt = pd->getLensNote();
      
      drawIris(ap);

      drawName(br,sr,nm,nt);
      
    }
  
    drawChannel(channel);
    //drawBatt();
  }
  oled.display();
}

void drawDialog() {
  switch(dialog) {
    case 1: { // Settings
    drawRect(3, 3, 100, 61); // draw main dialog box
      oled.setCursor(30, 12);
      oled.setFont(SMALL_FONT);
      oled.print("Settings");


      if (submenu) {
        switch (menuselected) {
          case 0: {
            oled.drawRect(59, 15, 13, 12, 1);
            break;
          } case 1: {
            oled.drawRect(58, 27, 25, 12, 1);
            break;
          }
        }
      } else {
        oled.drawRect(3, 15 + (menuselected * 12), 100, 12, 1);
      }

      oled.setCursor(7, 24);
      oled.print("Channel - ");
      oled.print(channel + choffset, HEX);
      oled.setCursor(7, 60);
      oled.print("Back");
      drawNav();
      break;
    }

    case 2: { // Map now?
      drawRect(3, 3, 90, 61); // draw main dialog box
      oled.setCursor(25, 12);
      oled.setFont(SMALL_FONT);
      oled.print("New lens:");
      oled.setCursor(10, 24);
      oled.print(pd->getLensBrand());
      oled.print(" ");
      oled.print(pd->getLensSeries());
      oled.setCursor(10, 36);
      oled.print(pd->getLensName());
      oled.print(" ");
      oled.print(pd->getLensNote());
      oled.setCursor(14, 60);
      oled.setFont(SMALL_FONT);
      oled.print("Map iris now?");
    
      drawButton(0, "yes");
      drawButton(2, "no");
      break;
    }

    case 3: { // WFO
      oled.fillRect(0, 10, 128, 54, 0); // black out background
      drawRect(3, 12, 82, 48); // draw main dialog box
      oled.setCursor(12, 22);
      oled.setFont(SMALL_FONT);
      oled.print("Select WFO:");
      oled.setCursor(22, 46);
      oled.print("T");
      oled.setCursor(28, 46);
      oled.setFont(LARGE_FONT);
      oled.print(wholestops[curmappingav]);
      oled.setCursor(10, 55);
      oled.setFont(SMALL_FONT);
      oled.print("and press ok");
    
      drawNav();
      break;
    }

    case 4: { // Mapping
      oled.fillRect(0, 10, 128, 54, 0); // black out background
      drawRect(3, 12, 82, 48); // draw main dialog box
      oled.setCursor(12, 22);
      oled.setFont(SMALL_FONT);
      oled.print("Set lens to:");
      oled.setCursor(22, 46);
      oled.print("T");
      oled.setCursor(28, 46);
      oled.setFont(LARGE_FONT);
      oled.print(wholestops[curmappingav]);
      oled.setCursor(10, 55);
      oled.setFont(SMALL_FONT);
      oled.print("and press ok");
    
      drawButton(0, "back");
      drawButton(1, "ok");
      drawButton(2, "finish");
      break;
    }
  }
}


#define MARGIN 4

void drawButton(int pos, const char* text) {
  uint16_t w;
  oled.getTextBounds (text, 0, 0, NULL, NULL, &w, NULL);

  uint16_t x = 130 - (w + MARGIN * 2);
  uint16_t y = 7 + (18 * pos);
  
  drawRect(x, y, 100, 17);
  oled.setCursor(x + MARGIN, y + 11);
  oled.print(text);
}

void drawNavButton(bool down) {
  uint16_t boxy = 7 + (down * 36);
  drawRect(109, boxy, 100, 17);

  uint16_t arrowpointy = 13 + (down * 41);
  uint16_t arrowbasey = 18 + (down * 31);
  oled.drawLine(114, arrowbasey, 119, arrowpointy, 1);
  oled.drawLine(119, arrowpointy, 124, arrowbasey, 1);
}

void drawNav() {
  drawNavButton(0);
  drawNavButton(1);
  drawButton(1, "ok");
}


void setChannel() {
  if (pd->setChannel(channel)) {
    //pd->subscribe(DATA);
  } else {
    Serial.println(F("failed to set new channel"));
  }
}
