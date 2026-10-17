#pragma once
#include "Arduino.h"
#include <memory>
#include <deque>
#include <algorithm>
#include <cstring>
#define WL_NO_MODULE 255
#define WL_AP_LISTENING 7
#define WL_AP_CONNECTED 8
struct IPAddress {
 uint8_t p[4];
 IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):p{a,b,c,d}{}
 friend std::ostream&operator<<(std::ostream&s,const IPAddress&x){return s<<(int)x.p[0]<<'.'<<(int)x.p[1]<<'.'<<(int)x.p[2]<<'.'<<(int)x.p[3];}
};
struct Socket {std::string input,output;size_t offset=0,maxWrite=256;bool open=true;};
struct WiFiClient {
 std::shared_ptr<Socket> sock;
 WiFiClient()=default;
 WiFiClient(std::shared_ptr<Socket>s):sock(s){}
 operator bool(){return sock&&sock->open;}
 int available(){return sock&&sock->open?sock->input.size()-sock->offset:0;}
 int read(uint8_t*buf,size_t n){n=std::min(n,(size_t)available());memcpy(buf,sock->input.data()+sock->offset,n);sock->offset+=n;return n;}
 size_t write(const uint8_t*buf,size_t n){if(!sock||!sock->open)return 0;n=std::min(n,sock->maxWrite);sock->output.append((const char*)buf,n);return n;}
 void stop(){if(sock)sock->open=false;}
};
struct WiFiServer {
 std::deque<std::shared_ptr<Socket>> queue;
 WiFiServer(uint16_t){}
 void begin(){}
 WiFiClient available(){if(queue.empty())return {};auto s=queue.front();queue.pop_front();return WiFiClient(s);}
};
struct WiFiMock {
 int code=WL_AP_LISTENING;
 int status(){return code;}
 void config(IPAddress){}
 int beginAP(const char*,const char*){return code;}
 IPAddress localIP(){return {192,168,4,1};}
};
extern WiFiMock WiFi;
