#pragma once
#include "Wire.h"
#include <array>
#include <deque>
#include <utility>
#define I2C_SPEED_FAST 400000
#define MAX30105_ADDRESS 0x57
struct MAX30105 {
 std::array<uint8_t,256> registers{};
 std::deque<std::pair<uint32_t,uint32_t>> samples;
 bool beginOK=true;
 bool begin(TwoWire &,uint32_t){return beginOK;}
 void setup(byte,byte,byte,int,int,int){}
 void enableDIETEMPRDY(){}
 uint8_t getINT2(){auto x=registers[1];registers[1]=0;return x;}
 uint8_t readRegister8(uint8_t,uint8_t reg){return registers[reg];}
 void writeRegister8(uint8_t,uint8_t reg,uint8_t n){registers[reg]=n;}
 uint16_t check(){return samples.size();}
 uint8_t available(){return samples.size();}
 uint32_t getFIFORed(){return samples.front().first;}
 uint32_t getFIFOIR(){return samples.front().second;}
 void nextSample(){samples.pop_front();}
 void clearFIFO(){samples.clear();}
};
