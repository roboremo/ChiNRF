# ChiNRF
Arduino library for NRF24L01+ clones: RFM73, RFM75, LCX24G, XN297  
  
This library is an attempt to make the NRF24L01+ and its clones understand each other.  
## Simple example: buttons and LEDs  
3 different radio modules connected to 3 Arduinos. You press a button on any of them, and the pin13 LED of the other 2 will light up. You release the button, and the LEDs will turn off.  
https://github.com/roboremo/ChiNRF/tree/master/ChiNRF%20v1.0/Examples  
  
## Interesting facts  
LCX24G and XN297/KSL297 seem to be identical  
It seems that RFM73 is not produced anymore, and was replaced by RFM75  
I have a RFM73 that works if initialized with register values from the RFM75 datasheet, but not with those from RFM73 datasheet.  
  
## Limitations  
In order to be able to communicate with each other, the radio modules are set to fixed payload size, no CRC, no ACK.
Some chips use preamble [0x55], others use [0x71 0x0F 0x55]. In order to understand each other, the chips that use [0x55] have set the TX address to [0x00 0x00 0x00 0x00 0x00] and are emulating the 3-Byte preamble by writing it (and the actual TX address) at the beginning of the data message, so the maximum message length gets smaller.  
Tested only with 5-Byte address.  
There are still many things to test.  

## Credits:
Big thanks to the guys from www.deviationtx.com/forum for revealing the scrambling algorythm of the XN297.  
https://www.deviationtx.com/forum/protocol-development/3368-jd-395-cx-10?screenMode=none&start=180  
  
## Things used for developing this library:
https://github.com/roboremo/NRF24-total-control  
https://github.com/roboremo/NRF24-demodulator
