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
  db.addOrUpdateEntry(routerID, n.id, n.RTT);
  db.addOrUpdateEntry(n.id, routerID, n.RTT);
  this->displayLSDB();
  db.updateForwardingTable(routerID, this->forwardTable);
  this->displayForwardTable();
  // 2. make an announcement to the network that topology around us has changed
  this->announce();
}

void LinkState::handleTopologyChange(vector<NodeID> changedIDs) {
  this->log("According to link check, we have some new some topology changes\n");
  
  // a neigbor has not responded to a ping in a while, so we assume it is down
  // change is happening! so update our sequence number
  this->seqNum++;

  // And we need to:
  // 1. update our lsdb
  // first delete all the entries that are no longer valid
  for (auto changedID : changedIDs) {
    if (ports.find(changedID) != ports.end()){
      // this neighbor is still in our neighbor list
      // update the RTT
      Neighbor n = this->neighbors[ports[changedID]];
      db.addOrUpdateEntry(routerID, changedID, n.RTT);
      db.addOrUpdateEntry(changedID, routerID, n.RTT);
    } else {
      // this neighbor is no longer in our neighbor list
      db.removeEntry(routerID, changedID);
      db.removeEntry(changedID, routerID);
    }
  }

  db.updateNode(routerID, seqNum, sys->time());
  db.updateForwardingTable(routerID, this->forwardTable);
  this->displayLSDB();
  this->displayForwardTable();

  // 2. make an announcement to the network that topology around us has changed
  this->announce();
}

void LinkState::announce() {
  this->log("Announcing topology change\n");

  // 1. create a new packet
  Packet* pkt = new Packet();
  pkt->type = LS;
  pkt->src = routerID;
  pkt->dst = 0; // broadcast, dst doesn't matter, ignore it
  
  // seqNum (4 bytes)
  // links (6 bytes each)
  pkt->payloadSize = 4 + neighbors.size() * 4;
  pkt->payload = malloc(pkt->payloadSize);

  // 2. serialize the payload (within the payload, we have to adjust the byte order ourselves)
  char* payload = (char*) pkt->payload;
  uint32_t* seqNum = (uint32_t*) payload;
  *seqNum = htonl(this->seqNum);
  uint8_t* links = (uint8_t*) (payload + 4);
  for (auto it = neighbors.begin(); it != neighbors.end(); it++) {
    uint16_t* neighborID = (uint16_t*) links;
    *neighborID = htons(it->second.id);
    uint16_t* neighborRTT = (uint16_t*) (links + 2);
    *neighborRTT = htons((uint16_t)it->second.RTT);
    links += 4;
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
  uint16_t src = pkt->src;
  char* payload = (char*) pkt->payload;
  uint32_t* seqNum = (uint32_t*) payload;
  uint32_t srcSeqNum = ntohl(*seqNum);
  uint16_t srcNumOfLinks = (pkt->size - 8 - 4) / 4;
  uint8_t* links = (uint8_t*) (payload + 4);
  this->displayLSDB();
  if (db.needUpdate(src, srcSeqNum)) {
    this->log("Received new packet from %d with seq %d\n", src, srcSeqNum);
    db.removeNodeLinks(src);
    db.updateNode(src, srcSeqNum, sys->time());
    for (int i = 0; i < srcNumOfLinks; i++) {
      uint16_t* neighborID = (uint16_t*) links;
      uint16_t dst = ntohs(*neighborID);
      uint32_t* neighborRTT = (uint32_t*) (links + 2);
      uint32_t cost = ntohs(*neighborRTT);
      links += 4;

      this->log("Received link from %d to %d with cost %d, seq %d\n", src, dst, cost, srcSeqNum);

      // 2. update our lsdb
      db.addOrUpdateEntry(src, dst, cost);
      db.addOrUpdateEntry(dst, src, cost);
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
    db.updateForwardingTable(routerID, this->forwardTable);
    this->displayForwardTable();
  } else 
    this->log("No new information learned, packet dropped\n");

  pkt->destory();
}

void LinkState::entryCheck() {
  this->log("Checking for expired entries\n");
  this->displayLSDB();
  db.checkExpiredEntries(sys->time());
  db.updateForwardingTable(routerID, this->forwardTable);
  this->displayForwardTable();
}

void LinkState::displayLSDB() {
  if (!this->isDebug) return;
  this->log("LSDB:\n");
  this->db.displayLSDB();
}
