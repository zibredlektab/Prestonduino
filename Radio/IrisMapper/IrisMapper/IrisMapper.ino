#include <PrestonDuino.h>
#include <SD.h>

#define DELAY 6

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

static uint16_t ringmap[10] = {0, 0x19C0, 0x371C, 0x529C, 0x7370, 0x8E40, 0xABC0, 0xCA70, 0xE203, 0xFEFC}; // map of actual encoder positions for linear iris, t/1 to t/22
uint16_t testmap[10] = {0, 0x172A, 0x2CEF, 0x5466, 0x6724, 0x9C76, 0xBD72, 0xD545, 0xDC41, 0xEA39}; // test mapping for hand-drawn apertures

uint16_t lensmap[10];
char curlens[40];
char lensfilename[40];
unsigned long long timelastchecked = 0;

bool mapping = false;

void setup() {
  Serial.begin(115200);

  if (!SD.begin(4)) {
    Serial.println("SD initialization failed!");
    //while (1);
  } else {
    Serial.println("SD initialization done.");
  }
  
  mdr = new PrestonDuino(Serial1);
  
  delay(100);
  mdr->setMDRTimeout(100);

  mdr->mode(0x00, 0x47); // We want control of AUX, we are only interested in commanded values
}

void loop() {
  if (timelastchecked + DELAY < millis()) {
    timelastchecked = millis();
    
    mdr->mode(0x00, 0x40); // We want control of AUX, we are only interested in commanded values
  
    char* mdrlens = mdr->getLensName();
    if (!strcmp(mdr->getLensName(), curlens)) { // current mdr lens is different from our lens
      /*lensfile.close();
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
      }*/
    }
  
    irisToAux();
  }
}

bool mapLens() {
  mapping = true;
  return true;
}

void irisToAux() {
  command_reply irisdata = mdr->data(0x41); // get metadata position of iris channel
  uint16_t iris = 0;

  if (irisdata.replystatus > 0) {
    // data was received from mdr
    iris = irisdata.data[1] * 0xFF;
    iris += irisdata.data[2];
  } else if (irisdata.replystatus == 0) {
    Serial.println("F/I knob not calibrated for this lens");
  }

  int ringmapindex = 0;
  for (ringmapindex; ringmapindex < 9; ringmapindex++) {
    if (ringmapindex < 9) {
      if (iris >= ringmap[ringmapindex] && iris < ringmap[ringmapindex+1]) {
        break;
      }
    }
  }

  uint8_t avnfrac = map(iris, ringmap[ringmapindex], ringmap[ringmapindex+1], 0, 100);
  double avnumber = ringmapindex + (avnfrac/100.0);
  uint8_t avnfloor = ringmapindex; // next highest AV number
  uint8_t avnceil = ringmapindex+1; // next lowest AV number

  // map iris data to aux encoder setting
  uint16_t auxpos;
  if (!mapping) {
    auxpos = map(avnumber*100, avnfloor*100, avnceil*100, testmap[avnfloor], testmap[avnceil]);
  } else {
    // if mapping is in progress, move aux linearly through entire range of encoder (no look up table)
    auxpos = map(avnumber, 0.0, 10.0, 0, 0xFFFF);
  }
  
  // set aux motor to encoder setting
  uint8_t auxh = auxpos >> 8;
  uint8_t auxl = auxpos & 0xFF;
  uint8_t auxdata[3] = {0x08, auxh, auxl};
  mdr->data(auxdata, 3);
}
