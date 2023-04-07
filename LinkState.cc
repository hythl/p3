#include "Impl.h"

LinkState::LinkState(Node *sysIn, RoutingProtocol *proxyIn) : Impl(sysIn, proxyIn){}

LinkState::~LinkState() {
  // add your own code (if needed)
}

void LinkState::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  this->numOfPorts = num_ports;
	this->routerID = router_id;

  this->NeighborSniff();

  sys->set_alarm(this->proxy, 1000, &linkCheckEvent);  // check for new neighbors every 1 second
  sys->set_alarm(this->proxy, 1000, &entryCheckEvent);  // check for expired entries every 1 second
  sys->set_alarm(this->proxy, 10000, &ngbrSniffEvent);  // check for new neighbors every 10 seconds
  sys->set_alarm(this->proxy, 30000, &updateEvent);     // send update every 30 seconds

  this->log("Initialized, %d ports\n", num_ports);
}

void LinkState::handle_alarm(void *data) {
  AlarmType* alarm = (AlarmType*) data;
  switch (*alarm) {
    case LINK_CHECK:
      this->log("Link Check triggered\n");
      this->linkCheck();
      sys->set_alarm(this->proxy, 1000, &linkCheckEvent);
      break;
    case ENTRY_CHECK:
      this->log("Entry Check triggered\n");
      this->entryCheck();
      sys->set_alarm(this->proxy, 1000, &entryCheckEvent);
      break;
    case NGBR_SNIFF:
      this->log("Neighbor Sniff triggered\n");
      this->NeighborSniff();
      sys->set_alarm(this->proxy, 10000, &ngbrSniffEvent);
      break;
    case UPDATE:
      this->log("Update triggered\n");
      seqNum++;
      db.updateNode(routerID, seqNum, sys->time());
      this->announce();
      sys->set_alarm(this->proxy, 30000, &updateEvent);
      break;
    default:
      break;
  }
}

void LinkState::recv(unsigned short port, void *packetIn, unsigned short size) {
  Packet* packet = new Packet();
  packet->deserialize(packetIn);
  this->log("Received packet from port %d, type: %d, src:%d, dst:%d, size:%d\n", port, packet->type, packet->src, packet->dst, packet->size);
  switch (packet->type) {
    case PING:
      this->handlePingPkt(packet, port);
      break;
    case PONG:
      this->handlePongPkt(packet, port);
      break;
    case LS:
      this->handleLSPkt(packet, port);
      break;
    case DATA:
      this->handleDataPkt(packet, port);
      break;
    default:
      break;
  }
}

void LinkState::handleNewNeighbor(PortID port) {
  this->log("New neighbor on port %d\n", port);
  
  // we have a new neighbor and it has been added to the neighbors map
  // change is happening! so update our sequence number
  this->seqNum++;

  // And we need to:
  // 1. update our lsdb
  Neighbor n = this->neighbors[port];
  db.addNode(routerID, seqNum, sys->time());
  db.addEntry(routerID, n.id, n.RTT);
  db.addEntry(n.id, routerID, n.RTT);
  this->displayLSDB();
  db.updateRoutingTable(routerID);
  this->displayRoutingTable();
  // 2. make an announcement to the network that topology around us has changed
  this->announce();
}

void LinkState::handleTopologyChange(vector<NodeID> oldIDs) {
  this->log("According to link check, we have some new some topology changes\n");
  
  // a neigbor has not responded to a ping in a while, so we assume it is down
  // change is happening! so update our sequence number
  this->seqNum++;

  // And we need to:
  // 1. update our lsdb
  for (auto oldID : oldIDs) {
    db.removeEntry(routerID, oldID);
    db.removeEntry(oldID, routerID);
    db.updateNode(routerID, seqNum, sys->time());
  }

  db.updateRoutingTable(routerID);
  this->displayLSDB();
  this->displayRoutingTable();

  // 2. make an announcement to the network that topology around us has changed
  this->announce();
}

void LinkState::route(Packet* pkt) {
  int nextHop = db.route(routerID, pkt->dst);
  if (nextHop == -1) {
    this->log("No route found to %d, packet dropped\n", pkt->dst);
    pkt->destory();
    this->displayRoutingTable();
    return;
  }

  if(ports.find(nextHop) == ports.end()) {
    this->log("No port found for next hop %d, packet dropped [WEIRD STUFF]\n", nextHop);
    pkt->destory();
    this->displayPorts();
    this->displayLSDB();
    this->displayRoutingTable();
    return;
  }

  this->log("Routing packet[%d to %d] to next hop %d\n", pkt->src, pkt->dst, nextHop);
  sys->send(this->ports[nextHop], pkt->buffer, pkt->size);
}

void LinkState::displayLSDB() {
  if (!this->isDebug) return;
  this->log("LSDB:\n");
  this->db.displayLSDB();
}

void LinkState::displayRoutingTable() {
  if (!this->isDebug) return;
  this->log("Routing Table:\n");
  this->db.displayRoutingTable();
}

void LinkState::announce() {
  this->log("Announcing topology change\n");

  // 1. create a new packet
  Packet* pkt = new Packet();
  pkt->type = LS;
  pkt->src = routerID;
  pkt->dst = 0; // broadcast, dst doesn't matter
  // routerID (2 bytes)
  // seqNum (4 bytes)
  // numOfLinks (2 bytes)
  // links (6 bytes each)
  pkt->payloadSize = 2 + 4 + 2 + neighbors.size() * 6;
  pkt->payload = malloc(pkt->payloadSize);


  // 2. serialize the payload
  char* payload = (char*) pkt->payload;
  uint16_t* routerID = (uint16_t*) payload;
  *routerID = this->routerID;
  uint32_t* seqNum = (uint32_t*) (payload + 2);
  *seqNum = this->seqNum;
  uint16_t* numOfLinks = (uint16_t*) (payload + 6);
  *numOfLinks = neighbors.size();
  uint8_t* links = (uint8_t*) (payload + 8);
  for (auto it = neighbors.begin(); it != neighbors.end(); it++) {
    uint16_t* neighborID = (uint16_t*) links;
    *neighborID = it->second.id;
    uint32_t* neighborRTT = (uint32_t*) (links + 2);
    *neighborRTT = it->second.RTT;
    links += 6;
  }

  // 3. flood the packet to all ports
  for(int i = 0; i < numOfPorts; i++){
    this->log("Sending announcement to port %d\n", i);
    Packet announcement = pkt->clone();
    announcement.serialize();
    sys->send(i, announcement.buffer, announcement.size);
  }
}

void LinkState::handleLSPkt(Packet* pkt, uint16_t port) {
  this->log("Received LS packet from port %d\n", port);
  
  // 1. deserialize the payload
  char* payload = (char*) pkt->payload;
  uint16_t* srcID = (uint16_t*) payload;
  uint16_t src = *srcID;
  uint32_t* seqNum = (uint32_t*) (payload + 2);
  uint32_t srcSeqNum = *seqNum;
  uint16_t* numOfLinks = (uint16_t*) (payload + 6);
  uint16_t srcNumOfLinks = *numOfLinks;
  uint8_t* links = (uint8_t*) (payload + 8);

  this->displayLSDB();
  if (db.needUpdate(src, srcSeqNum)) {
    this->log("Received new packet from %d with seq %d\n", src, srcSeqNum);
    db.removeNodeLinks(src);
    db.updateNode(src, srcSeqNum, sys->time());
    for (int i = 0; i < srcNumOfLinks; i++) {
      uint16_t* neighborID = (uint16_t*) links;
      uint16_t dst = *neighborID;
      uint32_t* neighborRTT = (uint32_t*) (links + 2);
      uint32_t cost = *neighborRTT;
      links += 6;

      this->log("Received link from %d to %d with cost %d, seq %d\n", src, dst, cost, srcSeqNum);

      // 2. update our lsdb
      db.addEntry(src, dst, cost);
      db.addEntry(dst, src, cost);
    }

    this->displayLSDB();
    this->log("New information learned, flooding packet\n");
    for(int i = 0; i < numOfPorts; i++){
      if (i == port) continue;
      this->log("Sending announcement to port %d\n", i);
      Packet announcement = pkt->clone();
      announcement.serialize();
      sys->send(i, announcement.buffer, announcement.size);
    } 

    // 4. update our routing table
    db.updateRoutingTable(routerID);
    this->displayRoutingTable();
  } else 
    this->log("No new information learned, packet dropped\n");

  pkt->destory();
}

void LinkState::entryCheck() {
  this->log("Checking for expired entries\n");
  this->displayLSDB();
  db.checkExpiredEntries(sys->time());
  db.updateRoutingTable(routerID);
  this->displayRoutingTable();
}