#include <PDClient.h>
#define SAMPLES 100

PDClient *pd;
int count;
unsigned long total = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("--------");
  Serial.println("Begin!");
  Serial.println("--------");
  pd = new PDClient();
  byte data[] = {0x10, 0x3};
  pd->sendCommand(1, data, 2);
}

void loop() {
  // put your main code here, to run repeatedly:

  pd->onLoop();

  long int focus = 0;
  if (count++ < SAMPLES) {
    total += analogRead(A1);
  } else {
    Serial.print("total is ");
    Serial.println(total);
    focus = total/SAMPLES;
    Serial.print("focus is ");
    Serial.println(focus);
    total = 0;
    focus = map(focus, 0, 1023, 0, 0xFFFF);
    count = 0;
    Serial.print("mapped focus is 0x");
    Serial.println(focus, HEX);
    uint8_t focusmin = focus % 0xFF00;
    uint8_t focusmaj = (focus - focusmin) / 0x100;
    byte lensdata[] = {2, focusmaj, focusmin};
    pd->sendCommand(4, lensdata, 3);
  }
}
