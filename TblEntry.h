#ifndef TblEntry_H
#define TblEntry_H

#include <stdint.h>

class RoutingProtocol;
class RoutingProtocolImpl;

class TblEntry {

 public:
  TblEntry();
  TblEntry(uint16_t pathCost, uint32_t timeStamp);
  ~TblEntry() {};
  uint16_t cost;
  uint32_t time;
};
#endif
