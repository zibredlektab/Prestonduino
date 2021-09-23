#include <SdFatConfig.h>
#include <sdios.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <SdFat.h>
#include <BlockDriver.h>
#include <SysCall.h>

#include <PrestonDuino.h>

#define DELAY 12


PrestonDuino *mdr;

File lensfile;
SdFat SD;

unsigned long long timelastchecked = 0;

uint8_t lensnamelen;
char curlens[40];
char mdrlens[40];
char lenspath[25];
char filename[15];

void setup() {
  Serial.begin(115200);

  while(!Serial) {};
  
  Serial.println("Serial initialized");
  
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);

  Serial.print("Initializing SD card...");

  if (!SD.begin(10, SPI_HALF_SPEED)) {
    Serial.println("SD initialization failed.");
  } else {
    Serial.println("SD initialization done.");
  }
  
  mdr = new PrestonDuino(Serial1);
  
  delay(100);
  mdr->setMDRTimeout(100);

}

void loop() {
  if (timelastchecked + DELAY < millis()) {
    timelastchecked = millis();
    
    lensnamelen = mdr->getLensName()[0] - 2; // lens name also includes data type, which is 2 bytes
    
    strncpy(mdrlens, &mdr->getLensName()[1], lensnamelen); // copy mdr data, sans data type, to local buffer
    mdrlens[lensnamelen] = '\0'; // null character to terminate string
    
    if (strcmp(mdrlens, curlens) != 0) { // current mdr lens is different from our lens
      Serial.println("Switching lenses...");
      Serial.print("This lens (");
      Serial.print(mdrlens);
      Serial.println(") has not been mapped.");

      for (int i = 0; i <= lensnamelen; i++) {
        curlens[i] = '\0'; // null-out previous lens name
        Serial.print(i);
        Serial.print(": ");
        Serial.print(mdrlens[i]);
        Serial.print(" 0x");
        Serial.println(mdrlens[i], HEX);
      }
  
      strncpy(curlens, mdrlens, lensnamelen); // copy our new lens to current lens name
      curlens[lensnamelen] = '\0'; // make absolutely sure it ends with a null

      makePath();

      SD.chdir(); // change to SD root

      SD.mkdir(lenspath, true); // make the directory for the new lens file (this will fail if the directory already exists, which is fine
      SD.chdir(lenspath); // move to the new directory
      
      lensfile = SD.open(filename, FILE_WRITE); // open lens file for current lens
      if (lensfile) {
        // lens file was created/opened successfully
        Serial.print("Opened file ");
        Serial.print(filename);
        Serial.print(" at path ");
        Serial.println(lenspath);
        
        lensfile.print(curlens);
        lensfile.close();
      } else {
        // lens file was not opened for some reason
        Serial.println("File not opened.");
        Serial.print("File ");
        Serial.print(filename);
        Serial.print(" at path ");
        Serial.println(lenspath);
      }
      

      lensfile = SD.open(filename); // re-open the file to make sure everything worked
      if (lensfile) {
        while(lensfile.available()) {
          Serial.print((char)lensfile.read());
        }
        lensfile.close();
        Serial.println();
      } else {
        Serial.println("failed to reopen file");
      }
    }
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
