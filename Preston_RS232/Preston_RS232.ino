#include "PrestonDuino.h"

PrestonDuino *mdr;

long unsigned int time = 0;
long unsigned int time1 = 0;
uint8_t i = 0;
bool continuelooping = true;

bool buttondown = false;
unsigned long long timebuttonreleased = 0;

const uint16_t stops[10] = {100, 140, 200, 280, 400, 560, 800, 1100, 1600, 2200}; // standard T stops
const uint16_t lensmap[10] = {0x0000, 0x19C0, 0x371C, 0x529C, 0x7370, 0x8E40, 0xABC0, 0xCA70, 0xE203, 0xFEFC};

uint16_t newiris = 0;

void setup() {

  Serial.begin(9600);
  while(!Serial);

  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);

  Serial.println("Starting test");

  mdr = new PrestonDuino(Serial1);
  delay(100);
  Serial.println("Done with mdr setup");
  mdr->shutUp();
  mdr->mode(0x11, 0x1); // take over iris, still streaming
  mdr->data(0x47); // request encoder values for FIZ
  byte fivesix[3] = {0x1, 0x8E, 0x40}; // set iris to t5.6
  mdr->data(fivesix, 3); 
}

void loop() {
  if (continuelooping) {
    mdr->onLoop();


    if (!buttondown && millis() >= timebuttonreleased + 100) {
      double offset = 0.0;
      if (!digitalRead(A0) || !digitalRead(A1)) {
        buttondown = true;
        if (!digitalRead(A0)) {
          offset = -.3;
        } else if (!digitalRead(A1)) {
          offset = .3;
        }

        uint16_t iris = mdr->getIris();

        Serial.print("Iris position is 0x");
        Serial.print(iris, HEX);

        double avnumber = positionToAV(iris);

        Serial.print(", AV is ");
        Serial.print(avnumber);

        avnumber *= 10;
        avnumber = round(avnumber);
        avnumber /= 10;

        Serial.print("(rounding to ");
        Serial.print(avnumber);

        Serial.print("), stop therefore is ");
        Serial.println(AVToStop(avnumber));

        double newav = avnumber - offset;

        Serial.print("New AV is ");
        Serial.print(newav);

        newiris = AVToPosition(newav);

        Serial.print(", new stop should be ");
        Serial.print(AVToStop(newav));
        Serial.print(", and new iris should be 0x");
        Serial.println(newiris, HEX);
        Serial.println();

        byte dataset[3] = {0x1, highByte(newiris), lowByte(newiris)};
        mdr->data(dataset, 3);

      }
    } else {
      if (digitalRead(A0) && digitalRead(A1)) {
        buttondown = false;
        timebuttonreleased = millis();
      }
    }

  
    if (0&& time + 10000 < millis()) {
      mdr->mode(0,0);
      delay(500);
      continuelooping = false;
      Serial.println("Debug time's up, shutting down.");
    }
  }
}

double stopToAV (uint16_t tstop) {

  uint8_t avnfloor, avnceil, avnfrac;
  double avnumber;
  // determine AV number
  int ringmapindex = 0;
  for (ringmapindex; ringmapindex < 9; ringmapindex++) { // Find our current position within the ringmap to find our whole AV number
    if (tstop >= stops[ringmapindex] && tstop < stops[ringmapindex+1]) {
      // iris position is greater than ringmapindex and less than the next index...so we have found our place
      break;
    }
  }

  avnfloor = ringmapindex; // whole portion of our AV number
  avnceil = ringmapindex+1; // next highest AV number

  // Calculate precise position between avnfloor and avnceil to find our fractional AV number
  avnfrac = map(tstop, stops[avnfloor], stops[avnceil], 0, 100);
  
  avnumber = avnfloor + (avnfrac/100.0); // complete AV number, with whole and fraction

  return avnumber;
}

uint16_t AVToStop (double avnumber) {
  double newstop = pow(2, avnumber);
  newstop = sqrt(newstop);
  newstop *= 100;
  uint16_t newstopasint = floor(newstop);

  return newstopasint;
}

double positionToAV (uint16_t position) {

  uint8_t avnfloor, avnceil, avnfrac;
  double avnumber;
  // determine AV number
  int lensmapindex = 0;
  for (lensmapindex; lensmapindex < 9; lensmapindex++) { // Find our current position within the ringmap to find our whole AV number
    if (position >= lensmap[lensmapindex] && position < lensmap[lensmapindex+1]) {
      // iris position is greater than ringmapindex and less than the next index...so we have found our place
      break;
    }
  }

  avnfloor = lensmapindex; // whole portion of our AV number
  avnceil = lensmapindex+1; // next highest AV number

  // Calculate precise position between avnfloor and avnceil to find our fractional AV number
  avnfrac = map(position, lensmap[avnfloor], lensmap[avnceil], 0, 100);
  
  avnumber = avnfloor + (avnfrac/100.0); // complete AV number, with whole and fraction

  return avnumber;
}

uint16_t positionToStop (uint16_t position) {
  return AVToStop(positionToAV(position));
}

uint16_t AVToPosition (double avnumber) {

  if (avnumber >= 9) {
    return lensmap[9];
  } else if (avnumber <= 0) {
    return lensmap[0];
  }

  uint8_t avnfloor, avnceil;
  avnfloor = floor(avnumber);
  avnceil = avnfloor+1;
  uint16_t newposition = map(avnumber*100, avnfloor*100, avnceil*100, lensmap[avnfloor], lensmap[avnceil]);

  return newposition;
}