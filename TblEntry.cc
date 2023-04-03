#include "TblEntry.h"
#include "RoutingProtocolImpl.h"

using namespace std;

TblEntry::TblEntry(){}

TblEntry::TblEntry(uint16_t pathCost, uint32_t timeStamp):cost(pathCost),time(timeStamp){
}
