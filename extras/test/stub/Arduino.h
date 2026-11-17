#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <cstdio>
using std::isnan; using std::isinf;
typedef uint8_t byte; typedef bool boolean;
extern uint32_t g_millis;
#ifdef AUTO_TICK
inline uint32_t millis(){ return g_millis++; }   // Uhr laeuft bei jedem Aufruf weiter (wie echte Hardware)
#else
inline uint32_t millis(){ return g_millis; }
#endif
#define F(x) x
#define HEX 16
class String { public: std::string s; String(){} String(const char*c):s(c?c:""){}
  unsigned length() const {return s.size();} const char* c_str() const {return s.c_str();} };
struct SerialStub { template<class T> void print(T){} template<class T> void print(T,int){} template<class T> void println(T){} };
extern SerialStub Serial;
