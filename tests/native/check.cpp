#include <cassert>
#include <fstream>
#include <iostream>
#include "Wire.h"
#include "WiFiS3.h"
unsigned long mockMillis=0;
SerialMock Serial;
TwoWire Wire;
WiFiMock WiFi;
int calculations=0;
std::string dashboardPath;
void maxim_heart_rate_and_oxygen_saturation(uint32_t*,int32_t n,uint32_t*,int32_t*s,int8_t*sv,int32_t*h,int8_t*hv){assert(n==100);*s=97;*h=75;*sv=1;*hv=1;++calculations;}
#include "../../arduino/MAX30102_AP_Web/MAX30102_AP_Web.ino"
void request(const std::string&text,const std::string&target,size_t maxWrite=256){
 auto s=std::make_shared<Socket>();s->input=text;s->maxWrite=maxWrite;webServer.queue.push_back(s);
 for(int i=0;i<2400&&s->open;++i){mockMillis+=5;lastSampleTime=mockMillis;serviceWeb();}
 assert(!s->open);assert(s->output.find(target)!=std::string::npos);
 auto pos=s->output.find("\r\n\r\n");assert(pos!=std::string::npos);
 auto lenpos=s->output.find("Content-Length: ");assert(lenpos!=std::string::npos);
 auto n=std::stoul(s->output.substr(lenpos+16));assert(s->output.size()-pos-4==n);
 if(target=="<!doctype html>")std::ofstream(dashboardPath)<<s->output.substr(pos+4);
}
int main(int argc, char **argv){
 assert(argc==2);dashboardPath=argv[1];
 setup();assert(apReady&&sensorReady);assert(Serial.output.str().find("http://192.168.4.1/")!=std::string::npos);
 // Weaker contact is now accepted; uncovered background stays absent.
 for(int i=0;i<10;++i)processSample(800,1200);
 assert(!fingerPresent&&samplesStored==0);
 processSample(8000,12000);assert(!fingerPresent);
 processSample(8000,12000);assert(fingerPresent&&samplesStored==0);
 processSample(4500,6000);assert(fingerPresent&&samplesStored==1);
 processSample(3000,4000);assert(fingerPresent&&!calculationReady);
 processSample(3000,4000);assert(!fingerPresent&&samplesStored==0);
 processSample(1000,1000);buildJson(mockMillis);assert(std::string(jsonBody.c_str()).find("\"spo2\":null")!=std::string::npos);
 assert(!strcmp(measurementStatus(mockMillis),"no_finger"));
 for(int i=0;i<PRESENCE_CONFIRM_SAMPLES;++i)processSample(40000,60000);
 assert(fingerPresent&&!calculationReady);
 for(int i=0;i<100;++i)processSample(40000+i,60000+i);
 assert(calculations==1&&samplesStored==75&&measurementSequence==1);
 buildJson(mockMillis);assert(std::string(jsonBody.c_str()).find("\"spo2\":97")!=std::string::npos);
 Serial.clear();printResults(mockMillis);assert(Serial.output.str().find("BPM: 75 | SpO2:97 %")!=std::string::npos);
 for(int i=0;i<25;++i)processSample(40000,60000);
 assert(calculations==2&&measurementSequence==2);
 request("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n","<!doctype html>");
 request("GET /api?x=1 HTTP/1.1\r\nHost: test\r\n\r\n","application/json",7); // Partial writes.
 validSPO2=0;buildJson(mockMillis);assert(!strcmp(measurementStatus(mockMillis),"invalid"));
 assert(std::string(jsonBody.c_str()).find("\"bpm\":75")!=std::string::npos);
 assert(std::string(jsonBody.c_str()).find("\"spo2\":null")!=std::string::npos);
 for(int i=0;i<PRESENCE_CONFIRM_SAMPLES;++i)processSample(1000,1000);
 assert(!fingerPresent&&!calculationReady);buildJson(mockMillis);
 assert(std::string(jsonBody.c_str()).find("\"bpm\":null")!=std::string::npos);
 for(int i=0;i<PRESENCE_CONFIRM_SAMPLES;++i)processSample(40000,60000);
 for(int i=0;i<99;++i)processSample(40000,60000);
 assert(!calculationReady);
 processSample(40000,60000);assert(calculationReady);
 processSample(262143,60000);assert(signalClipped&&!calculationReady);
 assert(!strcmp(measurementStatus(mockMillis),"clipped"));
 updateTemperature(mockMillis);particleSensor.registers[1]=2;particleSensor.registers[0x1f]=25;particleSensor.registers[0x20]=8;
 mockMillis+=30;lastSampleTime=mockMillis;updateTemperature(mockMillis);assert(temperatureValid&&temperatureC==25.5f);
 lastSensorServiceTime=mockMillis;mockMillis+=200;serviceSensor();assert(samplingInterrupted&&samplingResets==1&&!calculationReady);
 lastSensorServiceTime=mockMillis;for(int i=0;i<4;++i)particleSensor.samples.push_back({40000,60000});
 serviceSensor();assert(samplingResets==2&&!calculationReady&&particleSensor.samples.empty());
 lastSampleTime=mockMillis-1001;assert(!strcmp(measurementStatus(mockMillis),"data_missing"));
 buildJson(mockMillis);assert(std::string(jsonBody.c_str()).find("\"chip_temperature_c\":null")!=std::string::npos);
 request("GET /missing HTTP/1.1\r\n\r\n","404 Not Found");
 request("POST /api HTTP/1.1\r\n\r\n","405 Method Not Allowed");
 request("GET /favicon.ico HTTP/1.1\r\n\r\n","204 No Content");
 request("GET /"+std::string(180,'a')+" HTTP/1.1\r\n\r\n","414 URI Too Long");
 request("GET / HTTP/1.1\r\nX: "+std::string(2050,'a')+"\r\n\r\n","431 Request Header Fields Too Large");
 auto slow=std::make_shared<Socket>();slow->input="GET / HTTP/1.1\r\n";webServer.queue.push_back(slow);
 for(int i=0;i<500&&slow->open;++i){mockMillis+=5;serviceWeb();}assert(!slow->open&&httpPhase==0);
 sensorReady=false;request("GET /api HTTP/1.1\r\n\r\n","sensor_missing");
 apReady=false;WiFi.code=WL_NO_MODULE;startAccessPoint();assert(!apReady);
 std::cout<<"PASS: native C++ syntax and simulated acquisition, removal/reacquisition, validity, clipping, temperature, missing sensor/data, sample loss, partial HTTP writes, request limits, routing, timeout and AP failure.\n";
 std::cout<<"No Arduino board compilation or hardware test performed.\n";
}
