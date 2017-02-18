#include "ChiNRF.h"

// HARDWARE SETUP:

//  RFM73     Arduino  Wire
//  pin        pin     color

//  IRQ        7       orange
//  MISO       12      yellow
//  MOSI       11      green
//  SCK         9      blue
//  CSN        10      violet
//  CE          8      gray
//  VDD      3.3V      red
//  GND       GND      blue


//         |-- GND
// Button [|
//         |-- D3

// LED       D13


ChiNRF radio(chip_RFM73);

void onPacketReceived();


/*void addrDebug() {
  uint8_t addr[5];
  radio.readRegister(0x0A, addr, 5);
  Serial.print("RX_ADDR_P0: [(LSB) ");
  for(int i=0; i<5; i++) {
    Serial.print(addr[i], HEX);
    if(i<4) Serial.print(" ");
  }
  Serial.println(" (packet start)]");
  radio.readRegister(0x10, addr, 5);
  Serial.print("TX_ADDR: [(LSB) ");
  for(int i=0; i<5; i++) {
    Serial.print(addr[i], HEX);
    if(i<4) Serial.print(" ");
  }
  Serial.println(" (packet start)]");
}

void debug(const char* s) {
  Serial.println(s);
}*/


void setup() {

  //Serial.begin(115200);

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW); // LED OFF
  
  pinMode(3, INPUT_PULLUP); // BUTTON INIT

  radio.setOnReceive(onPacketReceived);
  //radio.setOnDebug(debug);
  
  radio.begin(2, 12, 11, 9, 10, 8);

  

  // begin defaults:
  //
  // RX on ch.2 (2402 MHz), 1Mbps, only pipe0 enabled
  // address 0xE7E7E7E7E7, addrW 5
  // fixed PL (32B), noCRC, noAA
  // ARC 0, EN_DYN_ACK
  // tx power 1 (not max)
  // LNA gain 1 (more sensitive RX, but also more noise)
  // CE HIGH

  radio.setRxPayloadWidth(0, 1); // pipe0, payloadLen 1

  radio.setRxAddressP0(0x1122332211LL);
  radio.setTxAddress(  0x1122332211LL);

  //addrDebug();

}

int buttonState, oldButtonState;


void loop() {
  
  radio.tick();

  buttonState = digitalRead(3);

  if(buttonState != oldButtonState) {

    uint8_t data;

    if(buttonState==LOW) { // button pressed
      data = 0x62; // symbol for LED ON   
    } else {
      data = 0x9D; // symbol for LED OFF
    }

    radio.writeTxPayload((uint8_t*)(&data), 1);

    oldButtonState = buttonState;
  }
  
}


uint8_t countOnes(uint8_t v) {
  uint8_t res = 0;
  for(int i=0; i<8; i++) {
    res += (v%2);
    v = v/2;
  }
  return res;
}


void onPacketReceived() {

  uint8_t data;

  radio.readRxPayload((uint8_t*)(&data), 1);

  Serial.println(data); 

  // symbols:
  // 1 -> 0x62
  // 0 -> 0x9D

  data = data ^ 0x9D;

  // 1 -> 0xFF
  // 0 -> 0x00

  uint8_t ones = countOnes(data);

  if(ones > 4) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }

  //Serial.println(ones); 
}

