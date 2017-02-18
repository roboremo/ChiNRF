/*******************************************************************
  This library is an attempt to make the
  NRF24L01+ and its clones understand each other.

  author: RoboRemo (www.roboremo.com) 
  
  BSD license, all text above must be included in any redistribution
 *******************************************************************/


#include "ChiNRF.h"
#include "Arduino.h"

static const uint8_t lcxPreamble[] = {
	0b01110001,
	0b00001111,
	0b01010101
};

static const uint8_t lcxScrambleSequence[] = {
	0xe3, 0xb1, 0x4b, 0xea, 0x85,
	0xbc, 0xe5, 0x66, 0x0d, 0xae, 0x8c, 0x88, 0x12,
  0x69, 0xee, 0x1f, 0xc7, 0x62, 0x97, 0xd5, 0x0b,
  0x79, 0xca, 0xcc, 0x1b, 0x5d, 0x19, 0x10, 0x24,
  0xd3, 0xdc, 0x3f, 0x8e, 0xc5, 0x2f, 0xaa, 0x16 };


ChiNRF::ChiNRF(radioChipModel model) {
	chipModel = model;
	setOnDebug(dummyOnDebug);
	scraMode = scramble_DISABLED;
}



void ChiNRF::usePins(int pIRQ, int pMISO, int pMOSI, int pSCK, int pCSN, int pCE) {
	irq = pIRQ;
	miso = pMISO;
	mosi = pMOSI;
	sck = pSCK;
	csn = pCSN;
	ce = pCE;
}


///////////////////////////////////////////////////////////////////////////////
// Software SPI:
///////////////////////////////////////////////////////////////////////////////

void ChiNRF::spiInit() { // assuming usePins(...) was called before

	pinMode(ce, OUTPUT);
  digitalWrite(ce, LOW);
  
  pinMode(csn, OUTPUT);
  digitalWrite(csn, HIGH);
  
  pinMode(sck, OUTPUT);
  digitalWrite(sck, LOW);
    
  pinMode(miso, INPUT);
  
  pinMode(mosi, OUTPUT);
  digitalWrite(mosi, LOW);
    
  pinMode(irq, INPUT);
}


uint8_t ChiNRF::spiTransfer(uint8_t data) { // software SPI
  uint8_t resData = 0;
  
  data = bitReverse(data);

  for(int i=0; i<8; i++) { 
    if(data%2==1) {
      digitalWrite(mosi, HIGH);      
    } else {
      digitalWrite(mosi, LOW);
    }
    
    data = data >> 1;
              
    delayMicroseconds(1);

    digitalWrite(sck, HIGH);

    resData = resData << 1;
    resData = resData + digitalRead(miso);

    delayMicroseconds(1);

    digitalWrite(sck, LOW);
    
    delayMicroseconds(1);       
  }
          
  return resData;
}


void ChiNRF::ceHigh() {
	digitalWrite(ce, HIGH);
}

void ChiNRF::ceLow() {
	digitalWrite(ce, LOW);
}


///////////////////////////////////////////////////////////////////////////////
// transceiver init:
///////////////////////////////////////////////////////////////////////////////



void ChiNRF::begin(uint8_t irq, uint8_t miso, uint8_t mosi, uint8_t sck, uint8_t csn, uint8_t ce) {
  usePins(irq, miso, mosi, sck, csn, ce);
  spiInit();
  //magicInit(); // will be called by setDataRate1M();
  
  // TODO: init registers
  
  setAddressWidth(5);
  setRxAddressP0(0xE7E7E7E7E7);
  setTxAddress(0xE7E7E7E7E7);
  
  setDataRate(data_rate_1M); // also calls magicInit()
  
  toggleActivateFeature(); // enables feature reg (it was disabled)
  
  disableDynamicPayloadLengthGlobal(); // aka DIS DPL
  
  // NRF24L01 datasheet says:
  // Disabling the Enhanced ShockBurstTM features
  // is done by setting register EN_AA=0x00 and the ARC = 0
  
  // DIS AA:
  disableAutoAck(0);
  disableAutoAck(1);
  disableAutoAck(2);
  disableAutoAck(3);
  disableAutoAck(4);
  disableAutoAck(5);
  
  setAutoRetxCount(0); // but actually this is required only by LCX24G/XN297
  
  setCRCSize(0); // Forced high if one of the bits in the EN_AA is high
  // so we call it here, after disabling the AA
  
  enableDynAck(); // Enables the W_TX_PAYLOAD_NOACK (0xB0) command
  // required by LCX24G/XN297
  // not mandatory for NRF24L01p (cmd 0xB0 also works before calling it)
  
  enableRxPipe(0);
  
  disableRxPipe(1);
  disableRxPipe(2);
  disableRxPipe(3);
  disableRxPipe(4);
  disableRxPipe(5);
  
  setRxPayloadWidth(0, 32); // fixed payload length 32 Bytes
  
  setTxPower(1); // not max
  setLNAGain(1); // high gain (more sensitive RX, but also more noise)
  
  setModeRX();
  setFreq(2402); // ch.2 (default for all?)
  powerUp();
  ceHigh();
  
}


///////////////////////////////////////////////////////////////////////////////
// registers control :
///////////////////////////////////////////////////////////////////////////////

void ChiNRF::readRegister(uint8_t reg, uint8_t *dest, uint8_t len) {
  digitalWrite(csn, LOW);
  spiTransfer(reg | regReadCmd);
  for(int i=0; i<len; i++) {
    dest[i] = spiTransfer(0x00);
  }
  digitalWrite(csn, HIGH);
}


uint8_t ChiNRF::readRegister(uint8_t reg) {
	uint8_t res;
	readRegister(reg, &res, 1);
	return res;
}



void ChiNRF::writeCommand(uint8_t cmd, uint8_t *data, uint8_t len) {
  digitalWrite(csn, LOW);
  spiTransfer(cmd);
  for(int i=0; i<len; i++) {
    spiTransfer(data[i]);
  }
  digitalWrite(csn, HIGH);
}

void ChiNRF::writeCommand(uint8_t cmd, uint8_t data) {
  digitalWrite(csn, LOW);
  spiTransfer(cmd);
  spiTransfer(data);
  digitalWrite(csn, HIGH);
}


void ChiNRF::writeRegister(uint8_t reg, uint8_t *data, uint8_t len) {

	char s[2+4+len*2+1];
	
	int i = 0;
	
	int val = (int)((reg)&0xFF);
  int a = (val/16)+'0';
  int b = (val%16)+'0';
  if(a>'9') a = ((a-'0')-10)+'A';
  if(b>'9') b = ((b-'0')-10)+'A';
	
	s[i] = a; i++;
	s[i] = b; i++;
	s[i] = ' '; i++;
	s[i] = '<'; i++;
	s[i] = '-'; i++;
	s[i] = ' '; i++;
	
	for(int j=0; j<len; j++) {
		val = (int)((data[j])&0xFF);
  	a = (val/16)+'0';
  	b = (val%16)+'0';
  	
  	if(a>'9') a = ((a-'0')-10)+'A';
  	if(b>'9') b = ((b-'0')-10)+'A';
		s[i] = a; i++;
		s[i] = b; i++;
	}
	
	s[i] = 0;
	debug(s);

  writeCommand(reg | regWriteCmd, data, len);
}

void ChiNRF::writeRegister(uint8_t reg, uint8_t data) {
  writeCommand(reg | regWriteCmd, data);
}


void ChiNRF::setBit(uint8_t reg, uint8_t b) {
  uint8_t val = readRegister(reg);
  val = val | (1 << b);
  writeRegister(reg, val);
}


void ChiNRF::clearBit(uint8_t reg, uint8_t b) {
  uint8_t val = readRegister(reg);
  val = val & (~(1 << b));
  writeRegister(reg, val);
}

uint8_t ChiNRF::readBit(uint8_t reg, uint8_t b) {
  uint8_t val = readRegister(reg);
  return (val >> b) % 2;
}


///////////////////////////////////////////////////////////////////////////////
// settings control :
///////////////////////////////////////////////////////////////////////////////

uint8_t ChiNRF::selectBank(uint8_t newBank) {

	if(chipModel == chip_RFM73 || chipModel == chip_RFM75) {
		// continue (these chips have 2 banks)
	} else {
		return 0; // chip does not have 2 banks
	}

  uint8_t bank = readRegister(0x07);
  bank = (bank & 0x80) >> 7;
  // now bank is the old bank
  bool sw = ((bank==0) && (newBank==1)) || ((bank==1) && (newBank==0));
  if(sw) {
    writeCommand(0x50, 0x53); // command to switch bank
    // now check if it has switched:
    bank = (readRegister(0x07) >> 7) & 0x01; // now is new bank
  }
  
  // debug:
  
  if(bank==1) {
  	debug("selected bank 1");
  } else if(bank==0) {
  	debug("selected bank 0");
  }
  
  return bank;
}


uint8_t ChiNRF::readRxPayloadWidth() { // of the last payload from RX FIFO
  uint8_t res;
  digitalWrite(csn, LOW);
  spiTransfer(0x60); // command R_RX_PL_WID
  res = spiTransfer(0x00);
  digitalWrite(csn, HIGH);
  return res;
}

void ChiNRF::readRxPayload(uint8_t *dest, uint8_t len) { // last from RX FIFO

  digitalWrite(csn, LOW);
  spiTransfer(0x61); // command R_RX_PAYLOAD
  
  if(scraMode==scramble_DISABLED) {
  	for(int i=0; i<len; i++) {
    	dest[i] = spiTransfer(0x00);
    }
  } else if(scraMode==scramble_NRF2LCX) {
  	// NRF pretending to be LCX
  	// the received data must be XORed, then each Byte reversed
  	
  	for(int i=0; i<len; i++) {
    	dest[i] = bitReverse( spiTransfer(0x00) ^ lcxScrambleSequence[i+addressWidth] );
    }
  } else if(scraMode==scramble_LCX2NRF) {
  	// LCX pretending to be NRF
  	// the received data must be each Byte reversed, then XORed
  	
  	for(int i=0; i<len; i++) {
    	dest[i] = bitReverse(spiTransfer(0x00)) ^ lcxScrambleSequence[i+addressWidth];
    }
  }

  digitalWrite(csn, HIGH);
}




void ChiNRF::writeTxPayload(uint8_t *src, uint8_t len, uint8_t forceAck, uint8_t forceNoAck) {

	
	setModeTX();
	//digitalWrite(ce, LOW);
  digitalWrite(csn, LOW);
  
  if(forceAck) {
  	spiTransfer(0xA8); // command W_TX_PAYLOAD_ACK
  	//debug("sending (A8)");
  } else if(forceNoAck) {
  	spiTransfer(0xB0); // command W_TX_PAYLOAD_NO_ACK
  	//debug("sending (B0)");
  } else {
	  spiTransfer(0xA0); // command W_TX_PAYLOAD
	  //debug("sending (A0)");
	}
	
  
  if(scraMode==scramble_DISABLED) {
  
  	// for compatibility with LCX24G / XN297,
  	// we must send the 3-Byte preamble, then address and data
  	
  	// LCX24G 3-byte preamble
  	// We are lucky it ends with 0x55,
  	// So any NRF24L01+ will also receive it correctly
  	for(int i=0; i<3; i++) {
  		spiTransfer(lcxPreamble[i]);
  	}
  	
  	// address
  	for(int i=0; i<addressWidth; i++) {
  		spiTransfer(txAddress[i]);
  	}
  	
  	// data
  	for(int i=0; i<len; i++) {
    	spiTransfer(src[i]);
    }
    
  } else if(scraMode==scramble_NRF2LCX) {
  	// NRF pretending to be LCX
  	// the data must be each Byte reversed, then XORed before sending
  	
  	// TODO: implement 3-Byte preamble here too
  	
  	for(int i=0; i<len; i++) {
  		spiTransfer( bitReverse(src[i]) ^ lcxScrambleSequence[i+addressWidth] );
    }
  } else if(scraMode==scramble_LCX2NRF) {
  	// LCX pretending to be NRF
  	// the data must be XORed, then each Byte reversed before sending
  	
  	for(int i=0; i<len; i++) {
  		spiTransfer( bitReverse(src[i] ^ lcxScrambleSequence[i+addressWidth]) );
    }
  }

  digitalWrite(csn, HIGH);
  
  //digitalWrite(ce, HIGH);
  
  delay(25);
  
  /*for(uint16_t i=0; i<1000; i++) {
  	if(readBit(0x07, 5)==1) { // TX_DATA_SENT
  		setBit(0x07, 5); // clear TX_DATA_SENT (write 1 to clear)
  		break;
  	}
  }*/
  // TX_DATA_SENT or timeout
  
  if(readBit(0x07, 5)==1) { // TX_DATA_SENT
  	//debug("data sent");
  	setBit(0x07, 5); // clear TX_DATA_SENT (write 1 to clear)
  } else {
  	//debug("data not sent");
  }
  
  setModeRX();
}

void ChiNRF::writeTxPayload(uint8_t *src, uint8_t len) {
	writeTxPayload(src, len, 0, 0);
}

void ChiNRF::writeTxPayloadAck(uint8_t *src, uint8_t len) {
	writeTxPayload(src, len, 1, 0);
}

void ChiNRF::writeTxPayloadNoAck(uint8_t *src, uint8_t len) {
	writeTxPayload(src, len, 0, 1);
}



void ChiNRF::setCh(uint8_t ch) {
	writeRegister(0x05, ch);
}

void ChiNRF::setFreq(uint16_t freq) {
	freq -= 2400;
	writeRegister(0x05, (uint8_t)(freq & 0xFF));
}

void ChiNRF::setModeRX() {
	digitalWrite(ce, LOW);
	//delay(1);
	setBit(0x00, 0);
	digitalWrite(ce, HIGH);
	//delay(1);
}

void ChiNRF::setModeTX() {
	digitalWrite(ce, LOW);
	//delay(1);
	clearBit(0x00, 0);
	digitalWrite(ce, HIGH);
	//delay(1);
}

void ChiNRF::powerUp() {
	setBit(0x00, 1);
}

void ChiNRF::shutDown() {
	clearBit(0x00, 1);
}

void ChiNRF::setCRCSize(uint8_t crcSize) {
	if(crcSize==0) {
		clearBit(0x00, 3); // clear EN_CRC
	} else if(crcSize==1) {
		if(chipModel == chip_LCX24G || chipModel == chip_XN297) {
			// do nothing, these chips do not support 1-Byte CRC
		} else {
			clearBit(0x00, 2); // CRCO = 0 (1 Byte)
			setBit(0x00, 3);   // set EN_CRC
		}
	} else if(crcSize==2) {
		if(chipModel == chip_LCX24G || chipModel == chip_XN297) {
			// do not touch CRCO
		} else {
			setBit(0x00, 2);   // CRCO = 1 (2 Bytes)
		}	
		setBit(0x00, 3);   // set EN_CRC
	}
	
	// NOTE: LCX24G supports only 0 or 2-Byte CRC, CRCO must be always 0
}


void ChiNRF::disguiseAs(radioChipModel disguised) {
	disguisedChipModel = disguised;
	
	if(chipModel==chip_NRF24L01p ||
	   chipModel==chip_NRF24L01 ||
	   chipModel==chip_RFM73 ||
	   chipModel==chip_RFM75) {   // if chip is not scrambling
	   
	  if(disguisedChipModel==chip_NRF24L01p ||
	     disguisedChipModel==chip_NRF24L01 ||
	     disguisedChipModel==chip_RFM73 ||
	     disguisedChipModel==chip_RFM75) { // peer is not scrambling
	     
			scraMode = scramble_DISABLED;
			
		}	else if(disguisedChipModel==chip_XN297 ||
		          disguisedChipModel==chip_LCX24G) { // peer is scrambling
		          
			scraMode = scramble_NRF2LCX; // NRF pretend to be LCX
		}
		
	} else if(chipModel==chip_LCX24G ||
	          chipModel==chip_XN297) {      // if chip is scrambling
	          
	  if(disguisedChipModel==chip_NRF24L01p ||
	     disguisedChipModel==chip_NRF24L01 ||
	     disguisedChipModel==chip_RFM73 ||
	     disguisedChipModel==chip_RFM75) { // if peer is not scrambling
	          
	     scraMode = scramble_LCX2NRF; // LCX pretend to be NRF
	     
		}	else if(disguisedChipModel==chip_XN297 ||
		          disguisedChipModel==chip_LCX24G) { // peer is scrambling
		          
			scraMode = scramble_DISABLED; // they understand each other if both are scrambling
		}
	}
		
}

void ChiNRF::undisguise() {
	disguiseAs(chipModel);
}

void ChiNRF::setAddressWidth(uint8_t aw) {
	 addressWidth = aw; // store
	 aw = (aw - 2) & 0x03;
	 // now:
	 // 5 -> 0b11,
	 // 4 -> 0b10,
	 // 3 -> 0b01,
	 // 2 (if works) -> 0b00
	 
	 writeRegister(0x03, aw);
}

	 

void ChiNRF::setAddress(uint8_t reg, uint64_t address, uint8_t addrW) {
	// must write the number of bytes specified by address width
	// LSB is written first
	
	uint8_t addr[addrW];
	
	for(int i=0; i<addrW; i++) {
		addr[i] = address & 0xFF;
		address = address >> 8;
	}
	
	if(scraMode == scramble_LCX2NRF || scraMode == scramble_NRF2LCX) {
		for(int i=0; i<addressWidth; i++) {
			addr[i] = addr[i] ^ lcxScrambleSequence[(addressWidth-1)-i];
		}
	}
	
	writeRegister(reg, addr, addrW);
}

void ChiNRF::setRxAddressP0(uint64_t addr) { // assuming setAddressWidth was called before
	setAddress(0x0A, addr, addressWidth);
}

void ChiNRF::setRxAddressP1(uint64_t addr) { // assuming setAddressWidth was called before
	setAddress(0x0B, addr, addressWidth);
}

void ChiNRF::setRxAddressP2(uint8_t addr) { // for P2 .. P5, only LSB
	setAddress(0x0C, addr, 1);
}

void ChiNRF::setRxAddressP3(uint8_t addr) { // for P2 .. P5, only LSB
	setAddress(0x0D, addr, 1);
}

void ChiNRF::setRxAddressP4(uint8_t addr) { // for P2 .. P5, only LSB
	setAddress(0x0E, addr, 1);
}

void ChiNRF::setRxAddressP5(uint8_t addr) { // for P2 .. P5, only LSB
	setAddress(0x0F, addr, 1);
}



void ChiNRF::setTxAddress(uint64_t addr) { // assuming setAddressWidth was called before
	// NRF24L01, NRF24L01p and RFM73/75 are sending the LCX24G preamble (3 Bytes)
	// followed by address inside data payload, so here we put address 0:
	if(chipModel==chip_NRF24L01p ||
	   chipModel==chip_NRF24L01 ||
	   chipModel==chip_RFM73 ||
	   chipModel==chip_RFM75) {
	   
   	setAddress(0x10, 0, addressWidth); // we write 0 here
   	
   	// instead, store address to use it when writing payload:
	
		for(int i=0; i<addressWidth; i++) {
			txAddress[(addressWidth-1)-i] = addr & 0xFF;
			addr = addr >> 8;
			// byte order in txAddress is as in RF signal
		}
		
	} else {
		setAddress(0x10, addr, addressWidth);
	}
}

void ChiNRF::setRxPayloadWidth(uint8_t pipeNumber, uint8_t payloadW) {
	pipeNumber = pipeNumber % 6;
	writeRegister(0x11 + pipeNumber, payloadW);
}


void ChiNRF::enableDynamicPayloadLengthGlobal() {
	setBit(0x1D, 2);
}

void ChiNRF::enableDynamicPayloadLength(uint8_t pipe) {
	pipe = pipe % 6;
	setBit(0x1C, pipe);
}

void ChiNRF::disableDynamicPayloadLengthGlobal() {
	clearBit(0x1D, 2);
}

void ChiNRF::disableDynamicPayloadLength(uint8_t pipe) {
	pipe = pipe % 6;
	clearBit(0x1C, pipe);
}

void ChiNRF::enableDynAck() {
	setBit(0x1D, 0);
	// Enables the W_TX_PAYLOAD_NOACK (0xB0) command
}

void ChiNRF::disableDynAck() {
	clearBit(0x1D, 0);
}


void ChiNRF::enableRxPipe(uint8_t pipe) {
	pipe = pipe % 6;
	setBit(0x02, pipe);
}

void ChiNRF::disableRxPipe(uint8_t pipe) {
	pipe = pipe % 6;
	clearBit(0x02, pipe);
}

void ChiNRF::enableAutoAck(uint8_t pipe) {
	pipe = pipe % 6;
	setBit(0x01, pipe);
}

void ChiNRF::disableAutoAck(uint8_t pipe) {
	pipe = pipe % 6;
	clearBit(0x01, pipe);
}

void ChiNRF::setDataRate(dataRate rate) {

	if(rate == data_rate_2M) { // [b5 b3] = '01'
		if(chipModel==chip_NRF24L01p ||
		   chipModel==chip_RFM73 ||
		   chipModel==chip_RFM75) { // only these chips have the RF_DR_LOW (b5)
			clearBit(0x06, 5);
		}
		setBit(0x06, 3);
	} else if(rate == data_rate_1M) { // [b5 b3] = '00'
		if(chipModel==chip_NRF24L01p ||
		   chipModel==chip_RFM73 ||
		   chipModel==chip_RFM75) { // only these chips have the RF_DR_LOW (b5)
			clearBit(0x06, 5);
		}
		clearBit(0x06, 3);
	} else if(rate == data_rate_250k) { // [b5 b3] = '10'
		if(chipModel==chip_NRF24L01p ||
		   chipModel==chip_RFM73 ||
		   chipModel==chip_RFM75) { // only these chips have the RF_DR_LOW (b5)
			setBit(0x06, 5);
			clearBit(0x06, 3);
		} else {
			// do nothing (other chips do not support data_rate_250k
		}
	}

	// magic registers have different values
	// for different data rates,
	// so we call this again:
	magicInit();
}


dataRate ChiNRF::getDataRate() {
	uint8_t b3 = 0, b5 = 0;
	
	if(chipModel==chip_NRF24L01p ||
		   chipModel==chip_RFM73 ||
		   chipModel==chip_RFM75) { // only these chips have the RF_DR_LOW (b5)
		b5 = readBit(0x06, 5);
	}
	
	b3 = readBit(0x06, 3);
	
	if(b5==0 && b3==1) {
		return data_rate_2M;
	} else if(b5==0 && b3==0) {
		return data_rate_1M;
	} else if(b5==1 && b3==0) {
		return data_rate_250k;
	} else { // '11' (should not occur)
		return data_rate_2M; // reserved / 2M
	}
	
}


void ChiNRF::setAutoRetxDelay(uint16_t us) {
	us = (us/250)-1;
	// 250 -> 0b0000
	// 500 -> 0b0001
	// ...
	// 4000 -> 0b1111
	uint8_t v = readRegister(0x04);
	v = (v & 0x0F) | (us << 4);
	writeRegister(0x04, v);
}

void ChiNRF::setAutoRetxCount(uint8_t count) {
	uint8_t v = readRegister(0x04);
	v = (v & 0xF0) | (count & 0x0F);
	writeRegister(0x04, v);
}

void ChiNRF::contTx() {
	if(chipModel==chip_NRF24L01 || // not tested
		 chipModel==chip_NRF24L01p ||
		 chipModel==chip_RFM73) { // not tested
		setBit(0x06, 7);
	}
}

void ChiNRF::contTxStop() {
	if(chipModel==chip_NRF24L01 || // not tested
		 chipModel==chip_NRF24L01p ||
		 chipModel==chip_RFM73) { // not tested
		clearBit(0x06, 7);
	}
}

void ChiNRF::setTxPower(uint8_t power) {  // 0, 1, 2, 3
	uint8_t v = readRegister(0x06);
	v = (v & 0b11111001) | ((power << 1) & 0b110);
	writeRegister(0x06, v);
}

void ChiNRF::setLNAGain(uint8_t gain) { // 1 = high, 0 = low (-20dB)
	if(gain) setBit(0x06, 0);
	else clearBit(0x06, 0);
}


uint8_t ChiNRF::getRxPipeNumber() {
	return (readRegister(0x07) >> 1) & 0b111;
}


void ChiNRF::magicInit() {

	dataRate rate = getDataRate();
	// some register values are different for different data rates

	if(chipModel==chip_LCX24G || chipModel==chip_XN297) {

		uint8_t dpl = readBit(0x1D, 2); // 1 = DPL enabled
		
		uint8_t BBCAL[] = {0xCD, 0x3F, 0x7F, 0x9C, 0x20}; // BBCAL for 8B
		// with this, the LCX transmits too many bytes when trying to send only one
		// so we added this:
		dpl = 1;
		// in order to use DYN_2M or DYN_1M
		
		if(!dpl) {
			// BCAL already for 8B
		} else if(rate==data_rate_2M) { 
			BBCAL[0] = 0xEA; // for DYN_2M
		} else { // DYN_1M
			BBCAL[0] = 0xD1; // for DYN_1M
		}
		
		writeRegister(0x1F, BBCAL, 5); // write BBCAL
		
		uint8_t DEMCAL[] = {0x0B, 0xDF, 0xC4, 0xA7, 0x03};
		
		writeRegister(0x19, DEMCAL, 5); // write DEMCAL
		
		uint8_t RFCAL[] = {0xCA, 0x9A, 0xB0, 0x61, 0xBB, 0xAB, 0x9C}; // RFCAL_297
		
		if(rate) { // DYN_2M
			RFCAL[0] = 0xC9;
			RFCAL[3] = 0x79;
		} else { // DYN_1M
			RFCAL[0] = 0xDA;
			RFCAL[3] = 0x79;
		}
		
		writeRegister(0x1E, RFCAL, 7); // write RFCAL
		
	} else if(chipModel==chip_RFM73 || chipModel==chip_RFM75) {
	
		uint8_t bank = selectBank(1);
		if(!bank) { // could not select bank 1
			return;
		}
		
		uint8_t reg00[] = {0x40, 0x4B, 0x01, 0xE2};
		uint8_t reg01[] = {0xC0, 0x4B, 0x00, 0x00};
		uint8_t reg02[] = {0xD0, 0xFC, 0x8C, 0x02};
		uint8_t reg03[] = {0x99, 0x00, 0x39, 0x41};
		
		// from RFM73 datasheet:
		//uint8_t reg04[] = {0xD9, 0x9E, 0x86, 0x0B}; // multiple carrier ???
		//uint8_t reg04[] = {0xD9, 0x9E, 0x86, 0x21}; // for single carrier mode
		
		//uint8_t reg05[] = {0x24, 0x06, 0x7F, 0xA6}; // DISABLE RSSI
		
		
		// from RFM75 datasheet:
		uint8_t reg04[] = {0xF9, 0x96, 0x82, 0x1B}; // 1Msps
		//uint8_t reg04[] = {0xF9, 0x96, 0x82, 0xDB}; // 2Msps
		//uint8_t reg04[] = {0xF9, 0x96, 0x8A, 0xDB}; // 250ksps
		//uint8_t reg04[] = {0xF9, 0x96, 0x82, 0x21}; // single carrier ? mode
		
		uint8_t reg05[] = {0x24, 0x06, 0x0F, 0xA6}; // DISABLE RSSI for 1M ?
		//uint8_t reg05[] = {0x24, 0x06, 0x0F, 0xB6}; // DISABLE RSSI for 2M ?
		//uint8_t reg05[] = {0x24, 0x06, 0x0F, 0xB6}; // DISABLE RSSI for 250k ?
		
		if(rate==data_rate_2M) {
			reg04[3] = 0xDB;
			
			reg05[3] = 0xB6;
		} else if(rate==data_rate_250k) {
			reg04[2] = 0x8A;
			reg04[3] = 0xDB;
			
			reg05[3] = 0xB6;
		} else { // 1M
			// do nothing (regs already set for 1M)
		}
		
		
		
		
		
		//uint8_t reg0C[] = {0x00, 0x12, 0x73, 0x05}; // PLL settling time 130 us
		uint8_t reg0C[] = {0x00, 0x12, 0x73, 0x00}; // PLL settling time 120 us
		
		// STATIC / DYNAMIC compatible ???
		
		uint8_t reg0D[] = {0x36, 0xB4, 0x80, 0x00}; // NEW_FEATURE ???

		// from RFM73 datasheet:
		//uint8_t reg0E[] = {0x41, 0x10, 0x04, 0x82, 0x20, 0x08,
		//                         0x08, 0xF2, 0x7D, 0xEF, 0xFF}; // Ramp curve ???
		
		// from RFM75 datasheet:
		                         
		uint8_t reg0E[] = {0x41, 0x20, 0x08, 0x04, 0x81, 0x20,
		                         0xCF, 0xF7, 0xFE, 0xFF, 0xFF};
		    
		    
		writeRegister(0x00, reg00, 4);
		writeRegister(0x01, reg01, 4);
		writeRegister(0x02, reg02, 4);
		writeRegister(0x03, reg03, 4);
		writeRegister(0x04, reg04, 4);
		writeRegister(0x05, reg05, 4); 	
		writeRegister(0x0C, reg0C, 4);	
		writeRegister(0x0D, reg0D, 4);
		writeRegister(0x0E, reg0E, 11);
		                                   
		selectBank(0); // back to Earth		
		
	} else if(chipModel==chip_NRF24L01p || chipModel==chip_NRF24L01) {
		// These ones don't have magic. (Or do they?)
	}
	
}


void ChiNRF::toggleActivateFeature() {
	writeCommand(0x50, 0x73);
}


	

void (ChiNRF::*onReceive)(void);

void ChiNRF::setOnReceive( void (*f)(void) ) {
  onReceive = f;
}


void ChiNRF::tick() {

  if ( readBit(0x07, 6) ) { // if DR triggered
  	if(onReceive) { // if function pointer was set
  		onReceive(); // call it
  	}
  	// clear RX FIFO: (discard theremaining data, if any)
  	/*uint8_t devnull;
  	while( readBit(0x17, 0)==0 ) {
  		readRxPayload((uint8_t*)(&devnull), 1);
  	}*/
  	setBit(0x07, 6); // clear DR (write 1 to clear)
  }
}

void (ChiNRF::*onDebug)(const char* s);

void ChiNRF::dummyOnDebug(const char* s) {
	// do nothing
}

void ChiNRF::setOnDebug( void (*f)(const char* s) ) {
  onDebug = f;
}

void ChiNRF::debug(const char* s) {
  onDebug(s);
}


///////////////////////////////////////////////////////////////////////////////
// util :
///////////////////////////////////////////////////////////////////////////////

uint8_t ChiNRF::bitReverse(uint8_t d) {
	uint8_t res = 0;
	
  for(int i=0; i<8; i++) {
  	res = res << 1;
  	res += (d & 0x01);
  	d = d >> 1;
  }
  return res;
}

void ChiNRF::bitReverseEachElement(uint8_t *buf, uint8_t bufLen) {
  for(int i=0; i<bufLen; i++) {
    buf[i] = bitReverse(buf[i]);
  }
}


