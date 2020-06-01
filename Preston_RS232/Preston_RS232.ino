#include "SoftwareSerial.h"
#include "PrestonPacket.h"

SoftwareSerial prestonSerial(2, 3); //2 is RX, 3 is TX

void setup() {
  Serial.begin(9600); //open communication with computer
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial.println();
  Serial.println("Hello!");

  byte data[] = {0x00, 0x1c};
  int datalen = 2;
  byte mode = 0x01;
  PrestonPacket foo = PrestonPacket(mode, data, datalen);
  
  for (int i = 0; i < foo.getPacketLength(); i++) {
    Serial.println(foo.getPacket()[i], HEX);
  }
  Serial.println();

  /*prestonSerial.begin(115200);
  while (!prestonSerial) {
    ; // wait for Preston to connect
  }
  Serial.println("Connected to Preston");

  byte buf[] = {02,30,31,30,32,30,30,31,43,39,39,03};
  prestonSerial.write(buf, sizeof(buf)+1);*/

  
  Serial.println("----");
}

void loop() {


}

void string2hexString(char* input, char* output)
{
    int loop;
    int i; 
    
    i=0;
    loop=0;
    
    while(input[loop] != '\0')
    {
        sprintf((char*)(output+i),"%02X", input[loop]);
        loop+=1;
        i+=2;
    }
    //insert NULL at the end of the output string
    output[i++] = '\0';
}

String buildPrestonPacket(char decmode) {

  char hexmode[strlen(decmode)*2+1];

  string2hexString(decmode, hexmode);

  return hexmode;

}

bool sendCommandToPreston(String command) {
  Serial.print("Sending " + command + " to Preston");
  prestonSerial.print(command);
  while (!prestonSerial.available()) {
    ;
  }
  byte response = prestonSerial.read();
  if (response == char(06)) {
    Serial.println("ACK recieved from Preston");
    return true;
  } else if (response == char(15)) {
    Serial.println("NAK recieved from Preston");
    return false;
  } else {
    Serial.print(response);
    Serial.println(" recieved from Preston");
    return false;
  }
}
