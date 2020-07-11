#include "PrestonPacket.h"
#include "PrestonDuino.h"

command_reply lensdata;

PrestonDuino *mdr;

unsigned long long time_now = 0;
int period = 2000;

long int ringtable[7][2] = {
  {0x410000, 140},
  {0x4123DD, 200},
  {0x414DB5, 280},
  {0x416EE2, 400},
  {0x4196B8, 560},
  {0x41C23A, 800},
  {0x41FFFF, 2200}
};

void setup() {

  Serial.begin(115200); //open communication with computer

  mdr = new PrestonDuino(Serial1);

  delay(100);
}


void loop() {
  if (millis() >= time_now + period) {
    
    time_now = millis();
    lensdata = mdr->data(0x41);
    byte posarray[4];
    posarray[3] = 0;
    for (int i = 2; i >= 0; i--) {
      posarray[i] = lensdata.data[2-i];
    }
    long int pos = ((long int*)posarray)[0];
    Serial.println("Input position: ");
    Serial.println(pos, 16);
    int iris = lookUp(pos, ringtable);
    Serial.print("Iris: ");
    Serial.println(iris);
    Serial.println("----");
  }
}

int lookUp(long int search, long int table[][2]) {
  int sizeoftable = 7;
  long int lowerpos, upperpos, loweriris, upperiris = 0;
  for (int i = 0; i < sizeoftable; i++) {
    if (table[i][0] < search && table[i+1][0] > search) {
      lowerpos = table[i][0];
      loweriris = table[i][1];
      upperpos = table[i+1][0];
      upperiris = table[i+1][1];
      break;
    }
  }

  Serial.print("Lower position is ");
  Serial.println(lowerpos, HEX);
  Serial.print("Upper position is ");
  Serial.println(upperpos, HEX);
  Serial.print("So lower iris is ");
  Serial.println(loweriris);
  Serial.print("And upper iris is ");
  Serial.println(upperiris);

  return map(search, lowerpos, upperpos, loweriris, upperiris);
}
