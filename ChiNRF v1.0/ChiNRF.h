/*******************************************************************
  This library is an attempt to make the
  NRF24L01+ and its clones understand each other.

  author: RoboRemo (www.roboremo.com) 
  
  BSD license, all text above must be included in any redistribution
 *******************************************************************/


#ifndef LCX24G_H
#define LCX24G_H

#include <stdio.h>


// values to be OR-ed with the register address for read, write:
// these are same for all transceiver modules
#define regReadCmd 0x00
#define regWriteCmd 0x20

// chip model, for use with constructor
typedef enum { chip_NRF24L01p = 0, chip_NRF24L01,

               chip_RFM73, chip_RFM75,
               // RFM73 not produced anymore ???
               // My RFM73 is actually a RFM75 ???      
               
               chip_LCX24G, chip_XN297
               // these 2 seem to be identical
               
             } radioChipModel;

// disguise mode
// XN297 and LCX24G scramble the address and data of the packets
// if you want to send from LCX24G to NRF24,
// you must call
// lcx.disguiseAs(chip_NRF24L01p)
// or
// nrf.disguiseAs(chip_LCX24G)
// but NOT both!!!


typedef enum { scramble_DISABLED = 0, scramble_LCX2NRF, scramble_NRF2LCX } scrambleMode;

typedef enum { data_rate_2M = 0, data_rate_1M, data_rate_250k } dataRate;


class ChiNRF {

	private:
		radioChipModel chipModel, disguisedChipModel;
		scrambleMode scraMode;
	
		uint8_t irq, miso, mosi, sck, csn, ce; // pins for software SPI
		
		uint8_t addressWidth;
		
		uint8_t txAddress[5];
		
		void usePins(int pIRQ, int pMISO, int pMOSI, int pSCK, int pCSN, int pCE);
		void spiInit();
		uint8_t spiTransfer(uint8_t data);
		
		void magicInit();

		void (*onReceive)(void);
		void (*onDebug)(const char* s);
		
		void debug(const char* s);
		static void dummyOnDebug(const char* s);
		
		
		// util:
		static uint8_t bitReverse(uint8_t b_in);
		static void bitReverseEachElement(uint8_t *buf, uint8_t bufLen);
		
	public:

		ChiNRF(radioChipModel chipModel);
		
		void begin(uint8_t irq, uint8_t miso, uint8_t mosi, uint8_t sck, uint8_t csn, uint8_t ce);
		
		void ceHigh();
		void ceLow();
		
		void readRegister(uint8_t reg, uint8_t *dest, uint8_t len);
		uint8_t readRegister(uint8_t reg);
		
		void writeCommand(uint8_t cmd, uint8_t *data, uint8_t len);
		void writeCommand(uint8_t cmd, uint8_t data);
		
		void writeRegister(uint8_t reg, uint8_t *data, uint8_t len);
		void writeRegister(uint8_t reg, uint8_t data);
		
		void setBit(uint8_t reg, uint8_t b);
		void clearBit(uint8_t reg, uint8_t b);
		uint8_t readBit(uint8_t reg, uint8_t b);
		
		// settings:
		uint8_t selectBank(uint8_t newBank);
	
		uint8_t readRxPayloadWidth();
		void readRxPayload(uint8_t *dest, uint8_t len);
		void writeTxPayload(uint8_t *src, uint8_t len, uint8_t forceAck, uint8_t forceNoAck);
		void writeTxPayload(uint8_t *src, uint8_t len);
		void writeTxPayloadAck(uint8_t *src, uint8_t len);
		void writeTxPayloadNoAck(uint8_t *src, uint8_t len);
	
		void setCh(uint8_t ch);
		void setFreq(uint16_t freq);
		void setModeRX();
		void setModeTX();
		void powerUp();
		void shutDown();
		void setCRCSize(uint8_t crcSize);
		void disguiseAs(radioChipModel disguisedModel);
		void undisguise();
	
		void setAddressWidth(uint8_t aw);
		void setAddress(uint8_t reg, uint64_t address, uint8_t addrW);
	
		void setRxAddressP0(uint64_t addr);
		void setRxAddressP1(uint64_t addr);
		void setRxAddressP2(uint8_t addr);
		void setRxAddressP3(uint8_t addr);
		void setRxAddressP4(uint8_t addr);
		void setRxAddressP5(uint8_t addr);
	
		void setTxAddress(uint64_t addr);
	
		void setRxPayloadWidth(uint8_t pipeNumber, uint8_t payloadW);
	
		void enableDynamicPayloadLengthGlobal();
		void enableDynamicPayloadLength(uint8_t pipe);
		void disableDynamicPayloadLengthGlobal();
		void disableDynamicPayloadLength(uint8_t pipe);
	
		void enableDynAck();
		void disableDynAck();
	
		void toggleActivateFeature();
	
		void enableRxPipe(uint8_t pipe);
		void disableRxPipe(uint8_t pipe);
		
		void enableAutoAck(uint8_t pipe);
		void disableAutoAck(uint8_t pipe);
		
		void setDataRate(dataRate);
		dataRate getDataRate();
		
		void setAutoRetxDelay(uint16_t us);
		void setAutoRetxCount(uint8_t count);
		
		void contTx();
		void contTxStop();
		
		void setTxPower(uint8_t power);
		void setLNAGain(uint8_t gain);
		
		uint8_t getRxPipeNumber();
		
		void tick();
		void setOnReceive( void (*f)(void));
		
		void setOnDebug( void (*f)(const char* s));
};

#endif
