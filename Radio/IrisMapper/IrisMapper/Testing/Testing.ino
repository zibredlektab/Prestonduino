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

    
    
    lensnamelen = mdr->getLensName()[0] - 2;
    strncpy(mdrlens, &mdr->getLensName()[1], lensnamelen);
    mdrlens[lensnamelen] = '\0';
    
    if (strcmp(mdrlens, curlens) != 0) { // current mdr lens is different from our lens
      Serial.println("Switching lenses...");
      Serial.print("This lens (");
      Serial.print(mdrlens);
      Serial.println(") has not been mapped.");

      for (int i = 0; i < lensnamelen; i++) {
        curlens[i] = '\0';
        Serial.print(i);
        Serial.print(": ");
        Serial.print(mdrlens[i]);
        Serial.print(" 0x");
        Serial.println(mdrlens[i], HEX);
      }
  
      strncpy(curlens, mdrlens, lensnamelen);
      curlens[lensnamelen] = '\0';

      makePath();

      SD.chdir();

      if(!SD.mkdir(lenspath, true)) {
        Serial.println("Failed to make directory");
      }
      SD.chdir(lenspath);
      
      lensfile = SD.open(filename, FILE_WRITE); // open lens file for current lens
      if (lensfile) {
        Serial.print("Opened file ");
        Serial.print(filename);
        Serial.print(" at path ");
        Serial.println(lenspath);
        
        lensfile.print(curlens);
        lensfile.close();
      } else {
        Serial.println("File not opened.");
        Serial.print("File ");
        Serial.print(filename);
        Serial.print(" at path ");
        Serial.println(lenspath);
      }
      

      lensfile = SD.open(filename);
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
  int pipecount = 0;
  int pathlen = 0;
  
  for (int i = 0; i < lensnamelen; i++) {
    if (curlens[i] == '\0') {
      return;
    }
    if (pipecount < 2) {
      if (curlens[i] == '|') {
          lenspath[i] = '/';
          pipecount++;
      } else {
        lenspath[i] = curlens[i];
      }
      lenspath[i+1] = '\0';
      pathlen++;
    } else {
      filename[i-pathlen] = curlens[i];
      filename[i-pathlen+1] = '\0';
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
