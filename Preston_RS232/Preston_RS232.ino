#include "PrestonDuino.h"
#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function

Uart Serial2 (&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2); // RX 11, TX 10

void SERCOM1_Handler()
{
  Serial2.IrqHandler();
}

PrestonDuino *mdr;

bool running = false;
bool switchreleased = true;
uint32_t timeswitchreleased = 0;

bool runswitch = false;

#define DEBOUNCE 1000

void setup() {

  Serial.begin(115200);
  while(!Serial);


  Serial2.begin(115200, SERIAL_8N1);


  // Assign pins 10 & 11 SERCOM functionality
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  mdr = new PrestonDuino(Serial1, 1);

  delay(100);
}

uint16_t aux = 0;

void loop() {
  mdr->setAux(aux);

  aux ++;

  mdr->onLoop();
  uint8_t dataset[3] = {0x01, 0x00, 0x00};
  mdr->data(dataset, sizeof(dataset));
}