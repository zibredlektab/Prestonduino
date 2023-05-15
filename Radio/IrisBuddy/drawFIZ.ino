
void drawName(const char* br, const char* sr, const char* nm, const char* nt) {
  Serial.print("Full name to display is [");
  Serial.print(br);
  Serial.print("][");
  Serial.print(sr);
  Serial.print("][");
  Serial.print(nm);
  Serial.print("][");
  Serial.print(nt);
  Serial.println("]");
  
  oled.setFont(SMALL_FONT);
  oled.setCursor(0, 8);

  if (strcmp(sr, "\0") == 0) {
    // if there's no series name, then print the brand instead
    oled.print(br);
  }
  oled.print(sr);
  oled.print(" ");
  oled.print(nm);
  oled.print("\n");
  oled.print(nt);
}

void drawIris(uint16_t ap) {
  uint8_t x, y;
  /*
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
*/
  x = X_OFFSET;
  y = Y_OFFSET;
  uint8_t irisx = x;
  
  oled.setCursor(irisx, y);
  oled.setFont(SMALL_FONT);
  oled.print("0x"); //5pix
  oled.setCursor(irisx + 8, y);
  oled.setFont(XLARGE_FONT);
  oled.print(ap, HEX);//irislabel);
  /*
  uint8_t fractionx = oled.getCursorX() + 1;
  
  oled.setCursor(fractionx + 5, y - 10);
  oled.setFont(SMALL_FONT);
  oled.print((int)(irisfraction*10));
  oled.drawFastHLine(fractionx, y - 8, 16, SH110X_WHITE); 
  oled.setCursor(fractionx + 3, y);
  oled.print(F("10"));*/
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
