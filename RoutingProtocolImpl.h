#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include <unordered_map>

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
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
   
    
 private:
    Node *sys; // To store Node object; used to access GSR9999 interface
    unsigned short numOfPorts;
    unsigned short routerId;
    eProtocolType protocolType;
    void handleDataPkg();
    void sendPing();
    void sendPong(uint16_t src, uint32_t timeStamp, unsigned short port);
    void handlePongPkg(void* pkg, unsigned short port);
    void handlePingPkg(void* pkg, unsigned short port);
    void printRoutingTbl();
    void printDVTbl();
    void linkCheck();
    pair<uint16_t, uint16_t> getDistance(uint16_t dest);
    pair<uint16_t, uint16_t> findMinPath(unordered_map<uint16_t, uint16_t> pathToDest);
  //  vector<pair<uint16_t, uint16_t>> updateNgbr(uint16_t dest, uint16_t delay, uint16_t nextHop);
    vector<pair<uint16_t, uint16_t>> updateNgbr(uint16_t nextHop, uint16_t delay, unsigned short port);
    int pingEvent = PING;
    int linkCheckEvent = 10;
    int dvCheck = 11;
    std::unordered_map<unsigned short, pair<uint16_t, uint32_t>> portStatus;
    std::unordered_map<uint16_t,std::pair<unsigned short, uint16_t>> linkCosts;
    std::unordered_map<uint16_t,std::pair<uint16_t, uint16_t>> routingTbl;
    std::unordered_map<uint16_t, std::unordered_map<uint16_t, uint16_t>> dvTbl;
    void handleDVPkg(void* pkg, unsigned short port);
    void printPortStatus();
    void sendUpdate(vector<pair<uint16_t, uint16_t>> changes, uint16_t dest, uint16_t port, bool isNew);
    void sendUpdateToAll(vector<pair<uint16_t, uint16_t>> changes, bool isNew, uint16_t src);
    vector<pair<uint16_t, uint16_t>> updateNonNgbr(uint16_t src, uint16_t dest, uint16_t delay);
//     * Min Distance: Map(nodeId, Pair(nextNodeId, distance))
//     *  * Port Status: Map(nodeId, Pair(portNumber, time))
//     *   * Routing Table: Map(nodeId, Map(nextNodeId, DIstance))
//    unordered_map*<uint16_t, pair*<uint16_t, uint32_t>> minDistances = new unordered_map<uint16_t, pair*<uint16_t, uint32_t>>();
//    unordered_map<uint16_t, pair*<unsigned short, uint32_t>> portStatus = new unordered_map<uint16_t, pair*<unsigned short, uint32_t>>();
//    unordered_map<uint16_t, unordered_map*<uint16_t, uint32_t>> routingTable = new unordered_map<uint16_t, unordered_map*<uint16_t, uint32_t>>();
};

#endif

