
void drawName(const char* br, const char* sr, const char* nm, const char* nt) {
    oled.setFont(SMALL_FONT);
    oled.setCursor(0, 8);

    if (strcmp(sr, "\0") == 0) {
      oled.print(br);
    }
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
      oled.setFont(SMALL_FONT);
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
      oled.setFont(SMALL_FONT);
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
  oled.drawRect(115, 0, 12, 12, SH110X_WHITE);
  oled.fillRect(116, 0, 11, 11, SH110X_WHITE);
  oled.setFont(SMALL_FONT);
  oled.setTextColor(SH110X_BLACK);
  oled.setCursor(119, 8);
  oled.print(channel, HEX);
  
  oled.setTextColor(SH110X_WHITE); // reset the text color to white for the next function
}

void drawBatt() {
  static int battcount = 9;
  static long int measuredvbatt = 0;
  static int output = 0;
  if (battcount < 10) {
    measuredvbatt += analogRead(VBATPIN);
    battcount++;
    
  } else {
    measuredvbatt /= 10;
    battcount = 0;

    output = map(measuredvbatt, 0, 653, 0, 100);
  }
  
  oled.setCursor(112, 17);
  if (output <= 100) {
    oled.print(output);
    oled.print("%");
  } else {
    oled.print("chg");
  }
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
  if (pd->setChannel(channel)) {
    //pd->subscribe(DATA);
  } else {
    Serial.println(F("failed to set new channel"));
  }
}
