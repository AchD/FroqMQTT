#pragma once
#include <cstdint>
class IPAddress { public: uint8_t a[4]={0}; IPAddress(){} IPAddress(uint8_t x,uint8_t y,uint8_t z,uint8_t w){a[0]=x;a[1]=y;a[2]=z;a[3]=w;} };
