#include "Impl.h"

void Impl :: NeighborSniff() {
  for(int i = 0; i < numOfPorts; i++){
    // this->log("Sending PING to port %d\n", i);
    PingPacket ping(routerID, sys->time());
    void* pingPkt = ping.serialize(); // pkt.serialize() will update ping's length
    sys->send(i, pingPkt, ping.size);
  }
}

void Impl :: handlePingPkt(Packet* pkt, unsigned short port) {
  // this->log("Received PING from %d\n", pkt->src);
  // make a pong packet back to the sender
  PongPacket pong(routerID, pkt->src, pkt->payload, pkt->payloadSize);
  void* pongPkt = pong.serialize(); // pkt.serialize() will update ping's length
  // this->log("Sending PONG to port %d, dst:%d\n", port, pong.dst);
  sys->send(port, pongPkt, pong.size);
  pkt->destory();
}

void Impl :: handlePongPkt(Packet* pkt, unsigned short port) {
  // this->log("Received PONG from %d\n", pkt->src);
  uint32_t* sentTime = (uint32_t*)pkt->payload;
  uint32_t RTT = sys->time() - *sentTime;
  // update the neighbor map
  if(neighbors.find(port) == neighbors.end()){
    // we have a new neighbor
    neighbors[port] = Neighbor{pkt->src, port, RTT, sys->time()};
    ports[pkt->src] = port;
    this->handleNewNeighbor(port);
  } else {
    neighbors[port].RTT = RTT;
    neighbors[port].lastPingTime = sys->time();
  }

  // this->displayNeighbors();
  // this->displayPorts();
  pkt->destory();
}

void Impl :: handleDataPkt(Packet* pkt, unsigned short port) {
  if (port == SPECIAL_PORT)
    this->log("Generated DATA\n");
  else 
    this->log("Received DATA from %d to %d\n", pkt->src, pkt->dst);

  // if the packet is not for me, forward it
  if (pkt->dst != routerID)
    this->route(pkt);
  else{
    this->log("DATA is for me\n");
    pkt->destory();
  }
}

void Impl :: linkCheck() {
  bool topologyChanged = false;
  vector<NodeID> neighborsGoesDown;
  for (auto it = neighbors.begin(); it != neighbors.end(); it++) {
    if (sys->time() - it->second.lastPingTime > LINK_TTL) {
      this->log("Neighbor %d on port %d is down\n", it->second.id, it->second.port);
      uint16_t oldID = it->second.id;
      neighbors.erase(it);
      ports.erase(oldID);
      topologyChanged = true;
      neighborsGoesDown.push_back(oldID);
    }
  }

  this->displayNeighbors();
  
  if (topologyChanged)
    this->handleTopologyChange(neighborsGoesDown);
  else
    this->log("We are good, Topology is stable\n");
}