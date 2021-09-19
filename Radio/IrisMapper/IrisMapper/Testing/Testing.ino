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


char curlens[29];
char mdrlens[29];
char lensfilename[40];

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

    strncpy(mdrlens, &mdr->getLensName()[1], 28);
    mdrlens[28] = '\0';
    
    if (strcmp(mdrlens, curlens) != 0) { // current mdr lens is different from our lens
      Serial.println("Switching lenses...");
      Serial.println("This lens has not been mapped.");
  
      strncpy(curlens, mdrlens, 28);
      curlens[28] = '\0';
      
      lensfile = SD.open("f00.txt", FILE_WRITE); // open lens file for current lens
      if (lensfile) {
        lensfile.print(curlens);
        lensfile.close();
      } else {
        Serial.println("File not opened");
      }
      

      lensfile = SD.open("f00.txt");
      while(lensfile.available()) {
        Serial.print((char)lensfile.read());
      }
      lensfile.close();
      Serial.println();
    }
  }
}
