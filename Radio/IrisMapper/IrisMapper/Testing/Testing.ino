#include <PrestonDuino.h>

PrestonDuino *mdr;

uint16_t lensmap[10] = {0, 0x0100, 0x0200, 0x0300, 0x4444, 0x6666, 0x8888, 0xAAAA, 0xCCCC, 0xFFFF};
uint16_t ringmap[10] = {0, 0x19C0, 0x371C, 0x529C, 0x7370, 0x8E40, 0xABC0, 0xCA70, 0xE203, 0xFEFC};


void setup() {
  Serial.begin(115200);

  
  mdr = new PrestonDuino(Serial1);
  
  delay(100);
  mdr->setMDRTimeout(100);

  mdr->mode(0x00, 0x47); // We want control of AUX, we are only interested in commanded values
}

void loop() {
  mdr->mode(0x00, 0x40); // We want control of AUX, we are only interested in commanded values

  Serial.println();
  
  command_reply irisdata = mdr->data(0x1); // get metadata position of iris channel
  uint16_t iris = 0;

  if (irisdata.replystatus > 0) {
    // data was received from mdr
    iris = irisdata.data[1] * 0xFF;
    iris += irisdata.data[2];
    
  } else if (irisdata.replystatus == 0) {
    Serial.println("F/I knob not calibrated for this lens");
  }

  double irisdec = (double)iris/100.0;

  Serial.print("iris dec is ");
  Serial.println(irisdec);
  
  double avnumber = log(sq(irisdec))/log(2); // iris AV number

  uint8_t avnfloor = floor(avnumber+.01); // next highest AV number
  uint8_t avnceil = ceil(avnumber+.01); // next lowest AV number

  Serial.print("AV number is ");
  Serial.println(avnumber);

  Serial.print("Floor is ");
  Serial.println(avnfloor);
  Serial.print("Ceil is ");
  Serial.println(avnceil);

  // map iris data to aux encoder setting
  uint16_t auxpos;
  // if mapping is in progress, move aux linearly through entire range of encoder (no look up table)
  //auxpos = map(iris, 1, 2200, 0, 0xFFFF);
  auxpos = map(avnumber*100, avnfloor*100, avnceil*100, lensmap[avnfloor], lensmap[avnceil]);

  // set aux motor to encoder setting
  uint8_t auxh = auxpos >> 8;
  uint8_t auxl = auxpos & 0xFF;

  
  Serial.print("auxpos is 0x");
  Serial.println(auxpos, HEX);
  
  uint8_t auxdata[3] = {0x18, auxh, auxl};
  mdr->data(auxdata, 3);
  
}
