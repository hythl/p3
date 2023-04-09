#ifndef IMPL_H
#define IMPL_H

#include <unordered_map>
#include <map>
#include <iostream>
#include <arpa/inet.h>
#include <set>
#include <cstdarg>

#include "global.h"
#include "Node.h"
#include "RoutingProtocol.h"
#include "Packet.h"
#include "lsdb.h"
#include "TblEntry.h"
#include "ForwardTable.h"

typedef uint16_t NodeID;
typedef uint16_t PortID;

#define DATAPORT 0xffff
#define LINK_TTL 15000
#define DEBUG false

using namespace std;

enum AlarmType{
  UNKNOWN,
  PING_ALARM,
  NGBR_SNIFF,
  LINK_CHECK,
  ENTRY_CHECK,
  UPDATE
};

typedef struct Neighbor{
  uint16_t id;
  uint16_t port;
  uint32_t RTT;
  uint32_t lastPingTime;
}Neighbor;


class Impl {
  public:
    Impl(Node* sys, RoutingProtocol* proxy) {
      this->sys = sys;
      this->proxy = proxy;
      this->isDebug = DEBUG;
    }

    virtual void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) = 0;
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    virtual void handle_alarm(void *data) = 0;
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    virtual void recv(unsigned short port, void *packet, unsigned short size) = 0;
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.

    // events related to neighbors that need to be handled by the routing protocol
    virtual void handleNewNeighbor(PortID port) = 0;
    virtual void handleTopologyChange(vector<NodeID> oldIDs) = 0;

    Node* sys;
    RoutingProtocol* proxy;
    NodeID routerID;
    uint16_t numOfPorts;
    bool isDebug;

    map<PortID, Neighbor> neighbors;
    map<NodeID, PortID> ports;
    ForwardTable forwardTable;
    void NeighborSniff();

    void handlePingPkt(Packet* pkt, unsigned short port);
    void handlePongPkt(Packet* pkt, unsigned short port);
    void handleDataPkt(Packet* pkt, unsigned short port);
    void linkCheck();

    void log(const char* format, ...) {
      if (!DEBUG) return;
      cout << ">>>>>>Router " << routerID << " [" + to_string(sys->time()) + "]: \t";
      va_list args;
      va_start(args, format);
      vprintf(format, args);
    }

    void displayNeighbors() {
      if(!DEBUG) return;
      printf("\t\t\t********* Router %d Neighbors *********\n", routerID);
      for (auto it = neighbors.begin(); it != neighbors.end(); it++)
        printf("\t\t\tRouter %d, port %d, RTT %d, lastPingTime:%d\n", it->second.id, it->second.port, it->second.RTT, it->second.lastPingTime);
      printf("\t\t\t********* Router %d Neighbors *********\n", routerID);
    }

    void displayPorts() {
      if(!DEBUG) return;
      printf("\t\t\t********* Router %d Ports *********\n", routerID);
      for (auto it = ports.begin(); it != ports.end(); it++)
        printf("\t\t\tRouter %d on port %d\n", it->first, it->second);
      printf("\t\t\t********* Router %d Ports *********\n", routerID);
    }

    void displayForwardTable() {
      if(!DEBUG) return;
      printf("\t\t\t********* Router %d Forward Table *********\n", routerID);
      forwardTable.print();
      printf("\t\t\t********* Router %d Forward Table *********\n", routerID);
    }
};

class DistanceVector : public Impl {
  public:
    DistanceVector(Node *sys, RoutingProtocol *proxy);
    ~DistanceVector();

    // define these functions from Impl
    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    void handle_alarm(void *data);
    void recv(unsigned short port, void *packet, unsigned short size);

  private:
    AlarmType pingEvent = PING_ALARM;
    AlarmType entryCheck = ENTRY_CHECK;
    AlarmType update = UPDATE;

    void NeighborSniff();
    void sendPong(uint16_t src, uint32_t timeStamp, unsigned short port);
    void handlePongPkg(void* pkg, unsigned short port);
    void handlePingPkg(void* pkg, unsigned short port);
    void printRoutingTbl();
    void printDVTbl();
    void handleLinkCheck();
    pair<uint16_t, uint16_t> getDistance(uint16_t dest);
    std::pair<std::uint16_t, TblEntry> findMinPath(unordered_map<std::uint16_t, TblEntry> pathToDest);
    vector<pair<uint16_t, uint16_t>> updateNgbr(uint16_t nextHop, uint16_t delay);
    void handleDataPkg(void* pkg);
    void handleUpdateEvent();
    std::unordered_map<uint16_t, unsigned short> linkInfo;
    std::unordered_map<uint16_t,std::pair<uint16_t, uint16_t>> routingTbl;
    std::unordered_map<uint16_t, std::unordered_map<uint16_t, TblEntry>> dvTbl;
    void handleDVPkg(void* pkg, unsigned short port);
    void printPortStatus();
    void handleEntryCheck();
    void sendUpdate(vector<pair<uint16_t, uint16_t>> changes, uint16_t dest, uint16_t port, bool isNew);
    void sendUpdateToAll(vector<pair<uint16_t, uint16_t>> changes, bool isNew, uint16_t src);
    vector<pair<uint16_t, uint16_t>> updateNonNgbr(uint16_t src, uint16_t dest, uint16_t delay);

    void handleNewNeighbor(PortID port);
    void handleTopologyChange(vector<NodeID> oldIDs);
};

typedef struct LinkStateAnnouncementEntry {
  uint16_t dst;
  uint32_t cost;
} LinkStateAnnouncementEntry;

typedef struct LinkStateAnnouncement {
  NodeID routerID;
  uint32_t seqNum;
  uint16_t numLinks;
  LinkStateAnnouncementEntry* links;
} LinkStateAnnouncement;

class LinkState : public Impl {
  public:
    LinkState(Node *sys, RoutingProtocol *proxy);
    ~LinkState();

    // define these functions from Impl
    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    void handle_alarm(void *data);
    void recv(unsigned short port, void *packet, unsigned short size);

    void handleNewNeighbor(PortID port);
    void handleTopologyChange(vector<NodeID> oldIDs);

  private:
    void announce();
    void handleLSPkt(Packet* pkt, unsigned short port);
    void entryCheck();

    void displayLSDB();

    // constants for alarms
    AlarmType ngbrSniffEvent = NGBR_SNIFF;
    AlarmType linkCheckEvent = LINK_CHECK;
    AlarmType entryCheckEvent = ENTRY_CHECK;
    AlarmType updateEvent = UPDATE;

    uint32_t seqNum = 0;
    LinkStateDatabase db;
};

#endif