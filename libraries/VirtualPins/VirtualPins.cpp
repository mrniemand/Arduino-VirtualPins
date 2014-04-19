#include <virtual_pins.h>
#include "VirtualPins.h"
#include <Arduino.h>
#include "../SPI/SPI.h"
#include "../Wire/Wire.h"

//give real pin for spi latch, virtual port number, and # of ports
SPIBranch::SPIBranch(SPIClass &spi,char latch_pin,char port,char sz):SPI(spi),latchPin(latch_pin),portBranch(port,sz),ioMode(VPSPI_COMPAT) {
	pinMode(latchPin,OUTPUT);
	on(latchPin);
	//SPI.begin();
	//user can define latch initial status (we will just toggle it)
	//this will select positive/negative polarity of the latch pin
	//for the hw spi SPI.setDataMode(...) can be used
	//also spi clock can be adjusted
}

void SPIBranch::mode() {}//this is internal control no meaning on the target shift registers
void SPIBranch::in() {io();}//call io because SPI bus is full-duplex
void SPIBranch::out() {io();}//call io because SPI bus is full-duplex

//do input and output (SPI is a bidirectional bus)
void SPIBranch::io() {
	pulse(latchPin);//read data (will also show output data)
	switch(ioMode) {
	case VPSPI_COMPAT:
		//pins can be input or output but not both at same time (still read all at once)
		//if the pin is in output mode, reading data will read the outputed data
		//TODO: put here some macros replacing constants
		//TODO: test this in compat mode
		for(int p=localPort+size-14;p>=0;p--)
			vpins_data[3*(size-p-1)+2]=
				(SPI.transfer(vpins_data[3*p+1]) & ~vpins_data[3*(size-p-1)])
				| (vpins_data[3*(size-p-1)+1] & vpins_data[3*(size-p-1)]);
		break;
	case VPSPI_DUPLEX:
		//separate inputs and outputs (still read all at once), only keeps data apart
		// even with separate data and working independent (in/out) pins will have the same number
		// digitalWrite(20,x) will affect 1st data pin of first shiftout register
		// digitalread(20) will get data from 1st input pin of first shiftin register
	default:
		for(int p=localPort+size-14;p>=0;p--) {//TODO: coul be size, but will be port #
			vpins_data[3*(size-p-1)+2]=SPI.transfer(vpins_data[3*p+1]);//TODO: use port macros
		}
		break;
	}
	pulse(latchPin);//write data
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
I2CShiftRegBranch::I2CShiftRegBranch(TwoWire & wire,char id,char local,char sz)
	:Wire(wire),serverId(id),portBranch(local,sz) {
}

void I2CShiftRegBranch::mode() {}//TODO: can we setup IO mode on i2c shift registers? google that!
void I2CShiftRegBranch::in() {}//TODO: test i2c input shift registers...

void I2CShiftRegBranch::out() {
  Wire.beginTransmission(serverId);
  for(int n=localPort;n<localPort+size;n++)
    while (Wire.write(*portOutputRegister(localPort))!=1);
  Wire.endTransmission(serverId);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
I2CBranch::I2CBranch(TwoWire & wire,char id,char local,char host,char sz):hostPort(host),I2CShiftRegBranch(wire,id,local,sz) {
	//TODO: wait for server to be ready
	//TODO: need a timeout and an error status somewhere
	/*do Wire.beginTransmission(id);
	while(!Wire.endTransmission());*/
}

void I2CShiftRegBranch::io() {in();out();}
void I2CBranch::mode() {dispatch(0b00);}
void I2CBranch::in() {
	char op=0b10;
  Wire.beginTransmission(serverId);
  Wire.write((hostPort<<2)|op);//codify operation on lower 2 bits
	int nbytes=Wire.requestFrom(serverId, 1);
  	*(portInputRegister(localPort))=Wire.read();
  Wire.endTransmission(serverId);
}
void I2CBranch::out() {dispatch(0b01);}

void I2CBranch::dispatch(char op) {
	Serial.println("Wire dispatch");
  Wire.beginTransmission(serverId);
  Wire.write((hostPort<<2)|op);//codify operation on lower 2 bits
  for(int n=0;n<size;n++) {
  	Wire.write(*(portModeRegister(localPort+n)+op));
  }
  Wire.endTransmission(serverId);
}


