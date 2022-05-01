#include <SCMD_config.h>
#include <SCMD.h>

#include <PrestonPacket.h>
#include <PrestonDuino.h>

PrestonDuino *PD;
SCMD motor;

void setup() {
  Serial.begin(115200);
  while(!Serial) {};
  Serial.println("Hello!");
  
  PD = new PrestonDuino(Serial1);
  PD->setMDRTimeout(1000);
  delay(1000);

  /*motor.settings.commInterface = I2C_MODE;
  motor.settings.I2CAddress = 0x5D;
  motor.settings.chipSelectPin = 10;

  while (motor.begin() != 0xA9 ) //Wait until a valid ID word is returned
  {
    Serial.println( "ID mismatch, trying again" );
    delay(500);
  }
  Serial.println( "ID matches 0xA9" );
  Serial.print("Waiting for enumeration...");
  while (motor.ready() == false );
  Serial.println("Done.");
  Serial.println();

  while (motor.busy() );
  motor.enable();*/
}

uint16_t bytesToUInt16 (const byte* buf, int i) {
  uint16_t out = 0;
  out = buf[i++] * 0x100;
  out += buf[i++];
  return out;
}

void loop() {
  PD->mode(0b00000000, 0);
  command_reply curdata = PD->data(0b01000001);
  int16_t zoomvelocity = 0;
  if (curdata.replystatus != 0) {
    zoomvelocity = bytesToUInt16(curdata.data, 1);
    Serial.print("velocity is ");
    Serial.println(zoomvelocity);
  }

  bool forward = (zoomvelocity > 0);
  uint8_t motorspeed = map(abs(zoomvelocity), 0, 0x7FFF, 0, 0xFF);

  Serial.print("Motor is moving ");
  if (forward) {
    Serial.print("forwards");
  } else {
    Serial.print("backwards");
  }

  Serial.print(" at a speed of ");
  Serial.println(motorspeed);

  //motor.setDrive(0, forward, motorspeed);
}
