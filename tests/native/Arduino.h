#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <type_traits>
using byte = uint8_t;
#define F(x) x
extern unsigned long mockMillis;
inline unsigned long millis() { return mockMillis; }
inline void delay(unsigned long n) { mockMillis += n; }
class String {
  std::string value;
 public:
  String() = default;
  String(const char *s):value(s) {}
  template<class T, typename std::enable_if<std::is_integral<T>::value,int>::type = 0>
  String(T n):value(std::to_string(n)) {}
  String(float n,int digits) { std::ostringstream s;s<<std::fixed<<std::setprecision(digits)<<n;value=s.str(); }
  bool reserve(size_t n) { value.reserve(n);return true; }
  String &operator=(const char *s) {value=s;return *this;}
  String &operator+=(const String &s) {value+=s.value;return *this;}
  String &operator+=(const char *s) {value+=s;return *this;}
  String &operator+=(char c) {value+=c;return *this;}
  const char *c_str() const {return value.c_str();}
  size_t length() const {return value.length();}
};
struct SerialMock {
 std::ostringstream output;
 void begin(unsigned long) {}
 explicit operator bool() const {return true;}
 template<class T>void print(T v){output<<v;}
 void print(float n,int digits){output<<std::fixed<<std::setprecision(digits)<<n;}
 template<class T>void println(T v){print(v);println();}
 void println(){output<<'\n';}
 void clear(){output.str("");output.clear();}
};
extern SerialMock Serial;
