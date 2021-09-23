#include <PrestonDuino.h>
#include <SdFatConfig.h>
#include <sdios.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <SdFat.h>
#include <BlockDriver.h>
#include <SysCall.h>

#define DELAY 12

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
SdFat SD;

static char wholestops[10][4] = {"1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11\0", "16\0", "22\0"};
static uint16_t ringmap[10] = {0, 0x19C0, 0x371C, 0x529C, 0x7370, 0x8E40, 0xABC0, 0xCA70, 0xE203, 0xFEFC}; // map of actual encoder positions for linear iris, t/1 to t/22

uint16_t lensmap[10];

uint8_t lensnamelen;
char curlens[40];
char mdrlens[40];
char lenspath[25];
char filename[15];

unsigned long long timelastchecked = 0;

bool mapping = false;
bool saved = true;
uint8_t curmappingav = 0;

void setup() {
  Serial.begin(115200);
  while(!Serial) {};

  // disable radio for now
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);

  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(10, SPI_HALF_SPEED)) {
    Serial.println("failed.");
    while(1);
  } else {
    Serial.println("done.");
  }
  
  mdr = new PrestonDuino(Serial1);
  
  delay(100);
  mdr->setMDRTimeout(100);

}

void loop() {
  if (timelastchecked + DELAY < millis()) {
    timelastchecked = millis();
    
    mdr->mode(0x0, 0x40); // We want control of AUX, we are only interested in commanded values

    lensnamelen = mdr->getLensName()[0] - 1; // lens name includes length of lens name, which we disregard

    if (!mapping) {
      strncpy(mdrlens, &mdr->getLensName()[1], lensnamelen); // copy mdr data, sans length of lens name, to local buffer
      mdrlens[lensnamelen] = '\0'; // null character to terminate string
      
      if (strcmp(mdrlens, curlens) != 0) { // current mdr lens is different from our lens
        Serial.println("Switching lenses...");

        Serial.print("New lens name is ");
        Serial.println(mdrlens);
        
        for (int i = 0; i <= lensnamelen; i++) {
          curlens[i] = '\0'; // null-out previous lens name
        }

        strncpy(curlens, mdrlens, lensnamelen); // copy our new lens to current lens name
        curlens[lensnamelen] = '\0'; // make absolutely sure it ends with a null

        makePath();
        SD.chdir(); // Change to SD root and make directory structure for new lens
        SD.mkdir(lenspath, true);
        SD.chdir(lenspath);
        
        if (SD.exists(filename)) {
          Serial.println("Lens has already been mapped");
          lensfile = SD.open(filename, FILE_READ); // open lens file for current lens
        
          if (lensfile) { // lens file opened successfully
            for(int i = 0; i < 10; i++) {
              // read through file byte by byte to reconstruct lens table
              lensmap[i] = lensfile.read();  // 16-bit int from 2x 8-bit int
              lensmap[i] += lensfile.read() * 0xFF;
              Serial.print("[F/");
              Serial.print(wholestops[i]);
              Serial.print(": 0x");
              Serial.print(lensmap[i], HEX);
              Serial.print("] ");
              
            }
            Serial.println();
            Serial.println("Finished loading lens data");
            
            lensfile.close();
          } else {
            Serial.println("Lens file exists, but failed to open");
          }
          
        } else { // lens file for this lens does not exist (yet)
          Serial.println("This lens has not been mapped");
          mapLens();
        }
        
      } else if (!saved) {
        lensfile = SD.open(filename, FILE_WRITE);
        int written = lensfile.write((uint8_t*)lensmap, 20);
        lensfile.close();

        if (written == 20) {
          Serial.println("Wrote lens data to file");
        }
        saved = true;
      }
      
    } else {
      if (Serial.available() > 0) {
        Serial.read();
        mapLens();
      }
    }


    
    irisToAux();
  }
}


void makePath() {
  int pipecount = 0; // how many directory levels have we processed
  int pathlen = 0; // length of path so far
  
  for (int i = 0; i < lensnamelen; i++) {

    if (pipecount < 2) {
      // we are still processing the path, not the filename itself
      if (curlens[i] == '|') {
          lenspath[i] = '/'; // replace all pipes with slashes, to create directory levels later
          pipecount++;
      } else {
        lenspath[i] = curlens[i]; // otherwise, copy letters over directly
      }
      if (pipecount == 2) {
        lenspath[i+1] = '\0'; // end filepath with null
      }
      pathlen++;
    } else {
      // we are processing the filename, not the path
      filename[i-pathlen] = curlens[i];
    }

    if (curlens[i] == '\0') {
      // we have reached the end of the lens name, stop working
      return;
    }
  }

  // Remove trailing whitespace
  for (int i = 13; i >= 0; i--) {
    if (filename[i] == ' ') {
      filename[i] = '\0';
    } else {
      break;
    }
  }
}



void mapLens() {

  command_reply auxdata = mdr->data(0x58);
  uint16_t aux = auxdata.data[1] * 0xFF;
  aux += auxdata.data[2];
  
  if (!mapping) {
    Serial.println("Mapping new lens");
    
    mapping = true;
    saved = false;
    curmappingav = 0;
    
  } else {

    Serial.print("saved at encoder position 0x");
    Serial.println(aux, HEX);
    lensmap[curmappingav++] = aux;
  }

  if (curmappingav < 10) {
    Serial.print("Set aperture to F/");
    Serial.print(wholestops[curmappingav]);
    Serial.print(" and press Send to continue...");
  } else {
    Serial.println("Finished mapping");
    mapping = false;
  }
}




void irisToAux() {
  command_reply irisdata = mdr->data(0x41); // get encoder position of iris channel
  uint16_t iris = 0;

  if (irisdata.replystatus > 0) {
    // data was received from mdr
    iris = irisdata.data[1] * 0xFF;
    iris += irisdata.data[2];
  } else if (irisdata.replystatus == 0) {
    Serial.println("Communication error with MDR");
  }

  int ringmapindex = 0;
  for (ringmapindex; ringmapindex < 9; ringmapindex++) {
    if (iris >= ringmap[ringmapindex] && iris < ringmap[ringmapindex+1]) {
      break;
    }
  }

  uint8_t avnfrac = map(iris, ringmap[ringmapindex], ringmap[ringmapindex+1], 0, 100);
  double avnumber = ringmapindex + (avnfrac/100.0);
  uint8_t avnfloor = ringmapindex; // next highest AV number
  uint8_t avnceil = ringmapindex+1; // next lowest AV number

  // map iris data to aux encoder setting
  uint16_t auxpos;
  if (!mapping) {
    auxpos = map(avnumber*100, avnfloor*100, avnceil*100, lensmap[avnfloor], lensmap[avnceil]);
  } else {
    // if mapping is in progress, move aux linearly through entire range of encoder (no look up table)
    auxpos = map(avnumber*100, 0, 1000, 0, 0xFFFF);
  }
  
  // set aux motor to encoder setting
  uint8_t auxh = auxpos >> 8;
  uint8_t auxl = auxpos & 0xFF;
  uint8_t auxdata[3] = {0x48, auxh, auxl};
  mdr->data(auxdata, 3);
}
