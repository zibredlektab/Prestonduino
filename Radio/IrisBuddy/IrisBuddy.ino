#include <InputDebounce.h>
#include <SPI.h>
#include <Wire.h>
#include <PDClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "Adafruit_seesaw.h"

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

#define LENSCHANGEDELAY 7000

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5
#define BUTTON_DEBOUNCE_DELAY   20   // [ms]

#define SEESAW_ADDR          0x36

Adafruit_seesaw ss;
int32_t encoder_position;

PDClient *pd;

Adafruit_SH1107 oled(64, 128, &Wire);

static InputDebounce btn_a;
static InputDebounce btn_b;
static InputDebounce btn_c;
static InputDebounce up;
static InputDebounce down;

int channel = 0xA;
bool changingchannel = false;
unsigned long long int timechannelchanged = 0;
unsigned long long int lenschangenotice = 0;

int prevdialog = 0; // for returning to previous states
int dialog = 0; // User-navigable interfaces
/*
 * Dialogs:
 * 0 - none
 * 1 - menu
 * 2 - new lens
 * 3 - Fully closed selection
 * 4 - WFO selection
 * 5 - Mapping
 * 6 - lens change
 */

int menuselected = 0;
bool submenu = false;
int choffset = 0;

static char wholestops[10][4] = {"1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11\0", "16\0", "22\0"};
uint8_t curmappingav = 9; // which point we are currently mapping, starts at fully closed
uint8_t maxstop = 0; // which stop is the maximum for this lens, defaults to 0 until the user actually sets it

void drawRect(int x, int y, int w, int h, int color = 0, bool trace = true, int tracecolor = 1) {
  oled.fillRect(x, y, w, h, color) ;
  if (trace) {
    oled.drawRect(x, y, w, h, tracecolor);
  }
}

void changeDialog(int newdialog, bool savestate = true) {
  /*
  switch(newdialog) {
    case 0: {
      Serial.println("Changing dialog to normal view");
      break;
    }
    case 1: {
      Serial.println("Changing dialog to main menu");
      break;
    }
    case 2: {
      Serial.println("Changing dialog to new lens");
      break;
    }
    case 3: {
      Serial.println("Changing dialog to WFO");
      break;
    }
    case 4: {
      Serial.println("Changing dialog to mapping");
      break;
    }
    case 5: {
      Serial.println("Changing dialog to lens change");
      break;
    }
  }*/
  if (savestate) prevdialog = dialog;
  dialog = newdialog;
}

void callback_pressed(uint8_t pinIn) {
  Serial.print("----Button ");
  switch (pinIn) {
    case BUTTON_A: {
      Serial.print("A ");
      break;
    } case BUTTON_B: {
      Serial.print("B ");
      break;
    } case BUTTON_C: {
      Serial.print("C ");
      break;
    } default: {
      Serial.print(pinIn);
      break;
    }
  }
  Serial.println("pressed----");

  if (pinIn == 18) {
    Serial.println("Closing iris");
    if (!pd->isLensMapped()) {
      if (pd->getIris() < 64535) pd->setIris(pd->getIris() + 1000);
    } else {
      if (pd->getIris() <= 890) pd->setIris(pd->getIris() + 10);
    }
  } else if (pinIn == 17) {
    Serial.println("Opening iris");
    if (!pd->isLensMapped()) {
      if (pd->getIris() > 1000) pd->setIris(pd->getIris() - 1000);
    } else {
      if (pd->getIris() >= 10) pd->setIris(pd->getIris() - 10);
    }
  } else {
  
    switch(dialog) {
      case 0: { // "Normal" view
        switch(pinIn) {
          case BUTTON_A: {
            pd->setIris(pd->getIris() + 1000);
            break;
          }
          case BUTTON_C: {
            pd->setIris(pd->getIris() - 1000);
            break;
          }
          case BUTTON_B: {
            changeDialog(1); // open main menu
            break;
          }
        }
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
              case 0: { // Change Channel
                if (submenu) {
                  // We are currently in the "change channel" submenu, so save the channel selection and exit
                  //changeChannel(choffset);
                  choffset = 0;
                  submenu = false;
                } else {
                  // enter the channel select submenu
                  submenu = true;
                }
                break;
              }
              
              case 1: { // Map now
                Serial.println("map lens now");
                pd->startMap();
                changeDialog(3);
                break;
              }
              
              case 2: { // Empty
                // do nothing
                break;
              }
              
              case 3: { // Back
                menuselected = 0; // reset menu selection for next time
                Serial.println("closing menu");
                changeDialog(0, false); // return to normal view
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
            changeDialog(prevdialog, false);
            break;
          }
          
          case BUTTON_C: {
            Serial.println("map now");
            pd->startMap();
            changeDialog(3, false);
            break;
          }
        }
        
        break;
      }

      case 3: { // Select fully closed
        switch(pinIn) {
          case BUTTON_A: {
            curmappingav--;
            if (curmappingav < 0) {
              curmappingav = 0;
            }
            break;
          }

          case BUTTON_B: {
            Serial.println("Fully closed selected");
            changeDialog(5, false);
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

      case 4: { // Select WFO
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
            changeDialog(4, false);
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
        
      case 5: { // Map lens
        switch(pinIn) {
          case BUTTON_A: { // Finish
            pd->finishMap();
            curmappingav = 9;
            maxstop = 0;
            changeDialog(0, false);
            break;
          }

          case BUTTON_B: { // ok
            Serial.println("Saving AV position");
            pd->mapLens(curmappingav++);
            if (maxstop == 0) { 
              // loop back to the start
              maxstop = curmappingav;
              curmappingav = 0;
              changeDialog(4, false);
            } else if (curmappingav == maxstop) {
              pd->finishMap();
              curmappingav = 9;
              maxstop = 0;
              changeDialog(0, false);
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

      case 6: { // Lens changed
        switch(pinIn) {
          case BUTTON_B: {
            lenschangenotice = 0;
            break;
          }
        }
        break;
      }
    }
  }
}



void doNav(int dir) {
  if (submenu) { // Currently navigating a sub-menu
    switch (menuselected) {
      case 0: { // Channel select submenu
        choffset += dir;
        if (channel + choffset < 0) {
          choffset = 0xF - channel;
        } else if (channel + choffset > 0xF) {
          choffset = channel * -1;
        }
        break;
      }
      case 1: { // Next item down from channel select (currently empty)
        // Do nothing
        break;
      }
    }
  } else { // Currently navigating top level menu items
    menuselected -= dir; // select the adjacent menu item
    if (menuselected >= 4) { // loop if at the end of the menu
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

  Serial.print("Looking for seesaw...");
  
  if (! ss.begin(SEESAW_ADDR)) {
    Serial.println("Couldn't find seesaw on default address");
    while(1) delay(10);
  }
  Serial.println("seesaw started");

  encoder_position = ss.getEncoderPosition();
  Serial.println("Turning on interrupts");
  delay(10);
  ss.enableEncoderInterrupt();

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

  int encoder_delta = ss.getEncoderDelta();
  Serial.print("encoder delta is ");
  Serial.println(encoder_delta);

  if (pd->isLensMapped()) {
    pd->changeIris(10 * encoder_delta);
  } else {
    pd->changeIris(1000 * encoder_delta);
  }

  readButtons();
  pd->onLoop();

  if (!pd->isLensMapped() && !pd->isMapLater() && !pd->isLensMapping()) {
    // Lens isn't mapped, and we have not decided to delay mapping
    changeDialog(2);
  } else if (pd->didLensChange()) {
    // Lens changed, and is mapped already
    lenschangenotice = millis();
    changeDialog(5);
  }
  if (millis() > lenschangenotice + LENSCHANGEDELAY && dialog == 5) {
    // if the timer is up, and we're still displaying dialog 5, go back to normal view
    Serial.println("Time's up, removing lens change alert");
    changeDialog(0, false);
  }

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
    if (0 && er > 0) { //TODO
      drawError(er);
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
    case 1: { // Main menu
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

      oled.setCursor(7, 36);
      oled.print("Re-map lens >");

      oled.setCursor(7, 48);
      oled.print("Empty");

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


    case 3: { // Full close
      oled.fillRect(0, 10, 128, 54, 0); // black out background
      drawRect(3, 12, 82, 48); // draw main dialog box
      oled.setCursor(12, 22);
      oled.setFont(SMALL_FONT);
      oled.print("Select Closed:");
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

    case 4: { // WFO
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

    case 5: { // Mapping
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

    case 6: { // New Lens
      drawRect(3, 3, 105, 61); // draw main dialog box
      oled.setCursor(20, 12);
      oled.setFont(SMALL_FONT);
      oled.print("Lens changed:");
      oled.setCursor(10, 24);
      oled.print(pd->getLensBrand());
      oled.print(" ");
      oled.print(pd->getLensSeries());
      oled.setCursor(10, 36);
      oled.print(pd->getLensName());
      oled.setCursor(10, 48);
      oled.print(pd->getLensNote());

      int newlenscutoffsec = (lenschangenotice + LENSCHANGEDELAY + 1) / 1000;
      int millissec = (millis() + 1) / 1000;
  
      oled.setCursor(110, 60);
      oled.print(newlenscutoffsec - millissec);
    
      drawButton(1, "ok");
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
