#include <PrestonDuino.h>
#include <SD.h>

/*
 * "map" iris in HU3 per lens, using ring rather than lens
 * IrisMap lens using IrisMapper (build Mapping LUT)
 * 
 * Read commanded iris metadata to determine desired stop
 * Use loaded Mapping LUT to determine correct aux encoder position from stop
 * Drive aux motor to position
 */

PrestonDuino *mdr;

File lensfile;

uint8_t mappoints = 0;
uint16_t lensmap[5] = {0, 0x4444, 0x8888, 0xAAAA, 0xFFFF};
char curlens[28];
char lensfilename[25];

bool mapping = false;

void setup() {
  Serial.begin(115200);

  if (!SD.begin(4)) {
    Serial.println("SD initialization failed!");
    while (1);
  }
  Serial.println("SD initialization done.");

  mdr = new PrestonDuino(Serial1);
  
  delay(100);
  mdr->setMDRTimeout(10);

  mdr->mode(0x10, 0x01); // We want control of AUX, we are only interested in commanded values
}

void loop() {
  char* mdrlens = mdr->getLensName();
  if (!strcmp(mdr->getLensName(), curlens)) { // current mdr lens is different from our lens
    lensfile.close();
    memcpy(curlens, mdrlens, 25);
    
    lensfile = SD.open(curlens, FILE_WRITE);
    
    if (lensfile) {
      mappoints = lensfile.read(); // first byte in file indicates how many mapping points are saved
      for(int i = 0; i < mappoints; i++) {
        lensmap[i] = lensfile.read(); // read through file byte by byte to reconstruct lens table
      }
    } else {
      if (mapLens()) {
        lensfile.write(mappoints);
        lensfile.write((uint8_t*)lensmap, mappoints*2);
        lensfile.close();
      } else {
        Serial.println("Switching lenses failed, because mapping new lens failed");
      } 
    }
  }

  irisToAux();
}

bool mapLens() {
  mapping = true;
  
}

void irisToAux() {
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
  uint16_t auxpos;
  if (!mapping) {
    auxpos = map(avnumber, avnfloor, avnceil, lensmap[avnfloor], lensmap[avnceil]);
  } else {
    // if mapping is in progress, move aux linearly through entire range of encoder (no look up table)
    auxpos = map(avnumber, 0.0, 10.0, 0, 0xFFFF);
  }
  
  // set aux motor to encoder setting
  uint8_t auxdata[3] = {0x18, auxpos & 0xFF, auxpos >> 8};
  mdr->data(auxdata, 3);
}
