# ChiNRF
Arduino library for NRF24L01+ clones: RFM73, RFM75, LCX24G, XN297  
  
This library is an attempt to make the NRF24L01+ and its clones understand each other.  
##Simple example: buttons and LEDs  
3 different radio modules connected to 3 Arduinos. You press a button on any of them, and the pin13 LED of the other 2 will light up. You release the button, and the LEDs will turn off.  
https://github.com/roboremo/ChiNRF/tree/master/ChiNRF%20v1.0/Examples

##Credits:
Big thanks to the guys from www.deviationtx.com/forum for revealing the scrambling algorythm of the XN297.  
https://www.deviationtx.com/forum/protocol-development/3368-jd-395-cx-10?screenMode=none&start=180  
  
##Things used for developing this library:
https://github.com/roboremo/NRF24-total-control  
https://github.com/roboremo/NRF24-demodulator
