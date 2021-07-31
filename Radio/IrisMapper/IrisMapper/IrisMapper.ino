#include <PrestonDuino.h>

PrestonDuino *mdr;

uint16_t lensmap[5] = {0, 0x44, 0x88, 0xAA, 0xFF};

void setup() {
  Serial.begin(115200);

  mdr = new PrestonDuino(Serial1);
  
  delay(100);
  mdr->setMDRTimeout(10);

  mdr->mode(0x10, 0x01); // We want control of AUX, we are only interested in commanded values
}

void loop() {

  command_reply irisdata = mdr->data(0x41); // get metadata position of iris channel
  uint16_t iris = 0;

  if (irisdata.replystatus > 0) {
    // data was received from mdr
    memcpy(iris, irisdata.data, 2);
  } else if (irisdata.replystatus == 0) {
    Serial.println("F/I knob not calibrated for this lens");
  }

  double irisdec = (double)iris/100.0;
  double avnumber = log(sq(irisdec))/log(2); // iris AV number

  uint8_t avnfloor = floor(avnumber); // next highest AV number
  uint8_t avnceil = ceil(avnumber); // next lowest AV number

  // map iris data to aux encoder setting
  uint16_t auxpos = map(avnumber, avnfloor, avnceil, lensmap[avnfloor], lensmap[avnceil]);

  // set aux motor to encoder setting
  uint8_t auxdata[3] = {0x18, auxpos & 0xFF, auxpos >> 8};
  mdr->data(auxdata, 3);
  
  // "map" iris in HU3 per lens, using ring rather than lens
  // IrisMap lens using IrisMapper (build Mapping LUT)
  
  // Read commanded iris metadata to determine desired stop
  // Use loaded Mapping LUT to determine correct aux encoder position from stop
  // Drive aux motor to position

}
