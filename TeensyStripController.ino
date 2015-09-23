/********************************************************************************************************
 ** TeensyStrip Controller
 ** ----------------------
 ** 
 ** This Sketch turns a Teensy 3.1, 3.2 or later into a controller for WS2811/WS2812 based led strips.
 ** This strip controller was originally designed supported by the Direct Output Framework, but since 
 ** the communication protocol is simple and communication uses the virtual com port of the Teensy
 ** it should be easy to controll the strips from other applications as well.
 ** 
 ** The most important part of the code is a slightly hacked version (a extra method to set the length of the 
 ** strips dynamiccaly has been added) of Paul Stoffregens excellent OctoWS2811 LED Library.  
 ** For more information on the lib check out: 
 ** https://github.com/PaulStoffregen/OctoWS2811
 ** http://www.pjrc.com/teensy/td_libs_OctoWS2811.html 
 ** 
 *********************************************************************************************************/  
 /*  
    License:
    --------   
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE. 
*/

#include "OctoWS2811Ext.h" //A slightly hacked version of the OctoWS2811 lib which allows for dynamic setting of the number of leds is used.

//Definiton of Major and Minor part of the firmware version. This value can be received using the V command.
//If something is changed in the code the number should be increased.
#define FirmwareVersionMajor 1
#define FirmwareVersionMinor 1

//Defines the max number of leds which is allowed per ledstrip.
//This number is fine for Teensy 3.2 & 3.1. For newer Teensy versions (they dont exists yet) it might be possible to increase this number.
#define MaxLedsPerStrip 1100

//Memory buffers for the OctoWS2811 lib
DMAMEM int displayMemory[MaxLedsPerStrip*6];
int drawingMemory[MaxLedsPerStrip*6];

//Config definition for the OctoWS2811 lib
const int config = WS2811_RGB | WS2811_800kHz; //Dont change the color order (even if your strip are GRB). DOF takes care of this issue (see config of ledstrip toy)

OctoWS2811Ext leds(MaxLedsPerStrip, displayMemory, drawingMemory, config);

word configuredStripLength=64;

//Setup of the system. Is called once on startup.
void setup() {
  Serial.begin(9600); // This has no effect. USB bitrate (12MB) will be used anyway.

  //Initialize the lib for the leds
  leds.setStripLength(configuredStripLength);
  leds.begin();
  leds.show();

}

//Main loop of the programm gets called again and again.
void loop() {
  // put your main code here, to run repeatedly:

  //Check if data is available
  if (Serial.available()) {
    
    byte receivedByte = Serial.read();  
    switch (receivedByte) {
      case 'L':
        //Set length of strips
        SetLedStripLength();
        break;
      case 'F':
        //Fill strip area with color
        Fill();
        break;  
      case 'R':
        //receive data for strips
        ReceiveData();
        break;
      case 'O':
        //output data on strip
        OutputData();
        break;   
      case 'C':
        //Clears all previously received led data
        ClearAllLedData();
        break;
      case 'V':
        //Send the firmware version
        SendVersion();  
        break;
      case 'M':
        //Get max number of leds per strip  
        SendMaxNumberOfLeds();
        break;
      default:
        // no unknown commands allowed. Send NACK (N)
        Nack();
      break;
    }
  }
}

//Outputs the data in the ram to the ledstrips
void OutputData() {
   leds.show();
   Ack();
}


//Fills the given area of a ledstrip with a color
void Fill() {
  word firstLed=ReceiveWord();

  word numberOfLeds=ReceiveWord();

  int ColorData=ReceiveColorData();

   if( firstLed<=configuredStripLength*8 && numberOfLeds>0 && firstLed+numberOfLeds-1<=configuredStripLength*8 ) {
       word endLedNr=firstLed+numberOfLeds;
       for(word ledNr=firstLed; ledNr<endLedNr;ledNr++) {
         leds.setPixel(ledNr,ColorData);
       }
       Ack();
   } else {
     //Number of the first led or the number of leds to receive is outside the allowed range
     Nack();    
   }
  
  
}


//Receives data for the ledstrips
void ReceiveData() {
  word firstLed=ReceiveWord();

  word numberOfLeds=ReceiveWord();

  if( firstLed<=configuredStripLength*8 && numberOfLeds>0 && firstLed+numberOfLeds-1<=configuredStripLength*8 ) {
    //FirstLedNr and numberOfLeds are valid. 
    //Receive and set color data
    
    word endLedNr=firstLed+numberOfLeds;
    for(word ledNr=firstLed; ledNr<endLedNr;ledNr++) {
      leds.setPixel(ledNr,ReceiveColorData());
    }

    Ack();

  } else {
    //Number of the first led or the number of leds to receive is outside the allowed range
    Nack();
  }
}

//Sets the length of the longest connected ledstrip. Length is restricted to the max number of allowed leds
void SetLedStripLength() {
   word stripLength=ReceiveWord();
   if(stripLength<1 || stripLength>MaxLedsPerStrip) {
     //stripLength is either to small or above the max number of leds allowed
     Nack();
   } else {
     //stripLength is in the valid range
     configuredStripLength=stripLength;
     leds.setStripLength(stripLength);
     leds.begin();  //Reinitialize the OctoWS2811 lib (not sure if this is needed)

     Ack();
   }
}

//Clears the data for all configured leds
void  ClearAllLedData() {
  for(word ledNr=0;ledNr<configuredStripLength*8;ledNr++) {
    leds.setPixel(ledNr,0);
  }
  Ack();
}


//Sends the firmware version
void SendVersion() {
  Serial.write(FirmwareVersionMajor);
  Serial.write(FirmwareVersionMinor);
  Ack();
}

//Sends the max number of leds per strip
void SendMaxNumberOfLeds() {
  byte B=MaxLedsPerStrip>>8;
  Serial.write(B);
  B=MaxLedsPerStrip&255;
  Serial.write(B);
  Ack();
}


//Sends a ack (A)
void Ack() {
  Serial.write('A');
}

//Sends a NACK (N)
void Nack() {
  Serial.write('N');
}

//Receives 3 bytes of color data.
int ReceiveColorData() {
  while(!Serial.available()) {};
  int colorValue=Serial.read(); 
  while(!Serial.available()) {};
  colorValue=(colorValue<<8)|Serial.read();
  while(!Serial.available()) {};
  colorValue=(colorValue<<8)|Serial.read();
  
  return colorValue;


}

//Receives a word value. High byte first, low byte second
word ReceiveWord() {
  while(!Serial.available()) {};
  word wordValue=Serial.read()<<8; 
  while(!Serial.available()) {};
  wordValue=wordValue|Serial.read();
  
  return wordValue;
}
   

