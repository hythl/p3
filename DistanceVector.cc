#include "Impl.h"
#include <iostream>

DistanceVector::DistanceVector(Node *sysIn, RoutingProtocol *proxyIn) : Impl(sysIn, proxyIn){}

DistanceVector::~DistanceVector() {
  // add your own code (if needed)
}

void DistanceVector::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
	this->numOfPorts = num_ports;
	this->routerID = router_id;

  this->NeighborSniff();

  sys->set_alarm(this->proxy, 1000, &linkCheckEvent);  // check for new neighbors every 1 second
  sys->set_alarm(this->proxy, 1000, &entryCheckEvent);  // check for expired entries every 1 second
  sys->set_alarm(this->proxy, 10000, &ngbrSniffEvent);  // check for new neighbors every 10 seconds
  sys->set_alarm(this->proxy, 30000, &updateEvent);     // send update every 30 seconds

  this->log("Initialized, %d ports\n", num_ports);
}

void DistanceVector::handle_alarm(void *data) {
  AlarmType* alarm = (AlarmType*) data;
  switch (*alarm) {
    case LINK_CHECK:
      this->log("Link Check triggered\n");
      this->linkCheck();
      sys->set_alarm(this->proxy, 1000, &linkCheckEvent);
      break;
    case ENTRY_CHECK:
      this->log("Entry Check triggered\n");
      // TODO
      // this->entryCheck();
      sys->set_alarm(this->proxy, 1000, &entryCheckEvent);
      break;
    case NGBR_SNIFF:
      this->log("Neighbor Sniff triggered\n");
      this->NeighborSniff();
      sys->set_alarm(this->proxy, 10000, &ngbrSniffEvent);
      break;
    case UPDATE:
      this->log("Update triggered\n");
      // TODO
      sys->set_alarm(this->proxy, 30000, &updateEvent);
      break;
    default:
      break;
  }
}

void DistanceVector::recv(unsigned short port, void *packetIn, unsigned short size) {
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
    case DV:
      // this->handleDVPkg(packet, port);
      break;
    case DATA:
      this->handleDataPkt(packet, port);
      break;
    default:
      break;
  }
}

void DistanceVector::handleUpdateEvent(){
	vector<pair<uint16_t, uint16_t>> datas;
	for(auto routingEntry: routingTbl){
		if(routingEntry.second.second == 0xffff){
			continue;
		}
		datas.push_back(pair<uint16_t, uint16_t>(routingEntry.first, routingEntry.second.second));
	}
	sendUpdateToAll(datas, false, routerID);
	sys->set_alarm((RoutingProtocol*)this, 30000, &(this->updateEvent));	
}

void DistanceVector::entryCheck(){
	uint32_t curTime = sys->time();
	for(auto tblPair: dvTbl){
		unordered_map<uint16_t, TblEntry> paths= tblPair.second;
		uint16_t dest = tblPair.first;
		for(auto path: paths){
			uint32_t lastSeen = path.second.time;
			uint16_t nextHop = path.first;
			uint16_t cost = path.second.cost;
			if(curTime - lastSeen > 45000 && cost != 0xffff){
				cout<<"path from " << routerID << " to " << dest << " through " << nextHop << " expires \n";
				if(nextHop != dest){
					sendUpdateToAll(updateNonNgbr(nextHop, dest, 0xffff), false, nextHop);
				}
			}
		}		
	}
	sys->set_alarm((RoutingProtocol*)this, 1000, &(this->entryCheckEvent));			
}


void DistanceVector::linkCheck(){
	uint32_t curTime = sys->time();
	vector<pair<uint16_t, uint16_t>> changes;
	cout<<"current time is " << curTime << "\n";
	unordered_map<uint16_t, pair<uint16_t, uint32_t>> portStatusCopy;
	for(auto portStatusPair: portStatus){
		portStatusCopy[portStatusPair.first] = portStatusPair.second;
	}
	for(auto portStatusPair: portStatusCopy){
		uint16_t src = portStatusPair.second.first;
		uint16_t port = portStatusPair.first;
		if(curTime - portStatusPair.second.second > 15000){
			cout << "link from " << routerID << " to " << src << "curshed " << "last seen "
			<< portStatusPair.second.second <<"\n";
			changes = updateNgbr(src, 0xffff, port);
			portStatus.erase(portStatusPair.first);
			linkCosts.erase(src);
			sendUpdateToAll(changes, false, src);						
		}
	}
//	printPortStatus();	
	sys->set_alarm((RoutingProtocol*)this, 1000, &(this->linkCheckEvent));			
}

void DistanceVector::printPortStatus(){
	cout << "print port staus of node " <<  routerID << "\n";
	for(auto portStatusEntry: portStatus){
		cout << "port: " << portStatusEntry.first << " to Node: " << portStatusEntry.second.first
		 << " timestap: "  << portStatusEntry.second.second << "\n";
	}
}

void DistanceVector::handleDataPkg(void *pkg){
	uint16_t* temp = (uint16_t*) pkg;
  uint16_t size = ntohs(*(temp + 1));
  uint16_t dest = ntohs(*(temp + 3));
	if(dest != routerID){
		if(routingTbl.count(dest) == 0 || routingTbl[dest].second == 0xffff){
			cout << "drop packet since no path to destination found " << "\n";
			free(pkg);
			return;
		}
		cout <<"dest for DATA pkt is " << dest << "\n";
		unsigned short nextPort = linkCosts[routingTbl[dest].first].first; 		
		sys->send(nextPort, pkg, size);
	}
	else{
		cout<<"data received at " << routerID << "\n";
		free(pkg);
		return;		
	}
}

void DistanceVector::handleDVPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
        uint16_t src = ntohs(*(temp + 2));
	uint16_t dest = ntohs(*(temp + 3));
	if(dest != routerID){
		if(routingTbl.count(dest) == 0){
			cout<<"can not find a path to " << dest << " on node " << routerID << "\n";
			return;
		}
		unsigned short nextPort = linkCosts[routingTbl[dest].first].first;
		sys->send(nextPort, pkg, size);
		return;
	}
	cout<<"Receive DV  on node " << this->routerID << " with type "
        << pkgType << " with size " << size <<" from " << src << "\n";
	temp = temp + 4;
	vector<vector<pair<uint16_t, uint16_t>>> updatesForAll;
	for(int i = 0; i < (size - 8) / 4; i++){
		uint16_t nodeId = ntohs(*temp);
		temp = temp + 1;
		uint16_t cost = ntohs(*temp);
		temp = temp + 1;
		cout<< "Update distance to node " << nodeId << " to cost " << cost << " from " << src << "\n";
		if(portStatus.count(port) == 0){
			continue;
		}
		uint16_t nextHop = portStatus[port].first;
		if(nodeId == routerID && nextHop == src){
			updatesForAll.push_back(updateNgbr(src, cost, port));
		}
		else if(nodeId == routerID){
			continue;
		}
		else{
			updatesForAll.push_back(updateNonNgbr(src, nodeId, cost));
		}
	}
	cout<< "The Routing table on node " << routerID << " is " << "\n";
	printRoutingTbl();
	cout<< "The DV table on node" << routerID << " is " << "\n";
	printDVTbl();
	cout<<"The size of updateForAll is "<< updatesForAll.size() << "\n";
	vector<pair<uint16_t, uint16_t>> realChanges;
	for(auto updatesEach: updatesForAll){
		for(auto update: updatesEach){
			cout<<"update is dest: " << update.first << " cost: " <<update.second <<"\n";
			cout<<"routing table is " << update.first << "cost: " <<routingTbl[update.first].second << "\n";
			if(routingTbl[update.first].second == update.second){
				realChanges.push_back(update);
			}
		}
	}
	cout<<"real change size is " << realChanges.size() << "\n";
	if(realChanges.size() != 0){
		sendUpdateToAll(realChanges, false, src);
	}
	free(pkg);
}

void DistanceVector::handlePingPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
	uint16_t pkgType = ntohs(*temp);
	uint16_t size = ntohs(*(temp + 1));
	uint16_t src = ntohs(*(temp + 2));
	uint32_t* temp32 = (uint32_t*) (temp + 4);
	uint32_t timeStamp = ntohl(*temp32);
	cout<<"Receive PING on node " << this->routerID << " with type PING"
	 << " with size " << size << " timeStamp " << timeStamp <<
	" from " << src << "\n";
	sendPong(src, timeStamp, port); 
	fflush(stdout);
	free(pkg);
}

void DistanceVector::NeighborSniff(){
	for(unsigned short i = 0; i < this->numOfPorts; i++){
		uint16_t* pkg = (uint16_t*) malloc(6 * sizeof(uint16_t));
		uint8_t* temp1 = (uint8_t*)pkg;
		*temp1 = PING;
		*(pkg + 1) = htons(12);
		*(pkg + 2) = htons(this->routerID);
		uint32_t* temp = (uint32_t*) (pkg + 4);
		*temp = htonl(sys->time());
		sys->send(i, pkg, 12);		
	}
  sys->set_alarm(this->proxy, 10000, &(this->updateEvent));		
}

void DistanceVector::sendPong(uint16_t src, uint32_t timeStamp, unsigned short port){
	uint16_t* pkg = (uint16_t*) malloc(6 * sizeof(uint16_t));
  *pkg = PONG;
  *(pkg + 1) = htons(12);
  *(pkg + 2) = htons(this->routerID);
  *(pkg + 3) = htons(src);
  uint32_t* temp = (uint32_t*) (pkg + 4);
  *temp = htonl(timeStamp);
  sys->send(port, pkg, 12);	
}

void DistanceVector::handlePongPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
        uint16_t src = ntohs(*(temp + 2));
	uint16_t dest = ntohs(*(temp + 3));
        uint32_t* temp32 = (uint32_t*) (temp + 4);
        uint32_t timeStamp = ntohl(*temp32);
	uint16_t delay = (uint16_t)sys->time() - timeStamp;
        cout<<"Receive PONG on node " << this->routerID << " with type "
        << pkgType << " with size " << size << " timeStamp " << timeStamp <<
        " from " << src << "\n";
	unsigned short nextPort = port;
	vector<pair<uint16_t, uint16_t>> changedRouting = updateNgbr(src, delay, nextPort);
	cout<< "DV table update to \n";
	printDVTbl();
	cout<<"Routing Table update to \n";
	printRoutingTbl();
	bool isNew = false;
	if(linkCosts.count(src) == 0){
		cout<< "Find a new link. Dump all info to " << src << "\n";
		isNew = true;
	}	
	portStatus[port] = std::pair<uint16_t, uint32_t>(src, sys->time());
	linkCosts[src] = std::pair<unsigned short, uint16_t>(port, delay);
	sendUpdateToAll(changedRouting, isNew, src);
	free(pkg);
}

void DistanceVector::sendUpdateToAll(vector<pair<uint16_t, uint16_t>> changes, bool isNew, uint16_t src){
	if(changes.size() == 0){
		return;
	}
	for(auto link: linkCosts){
		if(link.first == src && isNew){
			sendUpdate(changes, link.first, linkCosts[routingTbl[link.first].first].first, true);
		}
		else{		
			sendUpdate(changes, link.first, linkCosts[routingTbl[link.first].first].first, false);
		}
	} 
}

void DistanceVector::sendUpdate(vector<pair<uint16_t, uint16_t>> changes, uint16_t dest, uint16_t port, bool isNew){
	cout<< "send update to " << dest << "from node" << routerID << "\n";
	set<pair<uint16_t, uint16_t>> changeCopy;
	for(auto change: changes){
		changeCopy.insert(change);
	}
	vector<pair<uint16_t, uint16_t>> changesPoisonRv;
	if(isNew){
		for(auto routingEntry: routingTbl){
			if(routingEntry.second.second == 0xffff){
				continue;
			}
			changeCopy.insert(pair<uint16_t, uint16_t>(routingEntry.first, routingEntry.second.second));			
		}
	}
	for(auto change: changeCopy){
		cout<< "change is dest: " <<change.first <<" cost: " << change.second << "\n";
                uint16_t nextHop = routingTbl[change.first].first;
                if(nextHop == dest && change.first != dest){
			changesPoisonRv.push_back(pair<uint16_t, uint16_t>(change.first, 0xffff));
                        continue;
                }
		changesPoisonRv.push_back(pair<uint16_t, uint16_t>(change.first, change.second));
        } 
	if(changesPoisonRv.size() == 0){
		return;
	}
	uint16_t pkgSize = 8 + 4 * (changesPoisonRv.size());
	uint16_t* pkg = (uint16_t*) malloc(pkgSize);
        *pkg = DV;
        *(pkg + 1) = htons(pkgSize);
        *(pkg + 2) = htons(this->routerID);
        *(pkg + 3) = htons(dest);
	uint16_t* temp = pkg + 4;
	for(auto change: changesPoisonRv){
		*temp = htons(change.first);
		temp = temp + 1;
		*temp = htons(change.second);
		temp = temp + 1;
	}
        sys->send(port, pkg, pkgSize);
}

void DistanceVector::printDVTbl(){
	for(auto pairs: dvTbl){
		unordered_map<uint16_t, TblEntry> paths = pairs.second;
		for(auto path: paths){
			cout<<pairs.first << " " << path.first << " " << path.second.cost <<" " << path.second.time << "\n";
		}
	}
}

void DistanceVector::printRoutingTbl(){
	for(auto pairs: routingTbl){
		cout<< pairs.first << " " << pairs.second.first << " " << pairs.second.second << "\n";
	}
}


vector<pair<uint16_t, uint16_t>> DistanceVector::updateNgbr(uint16_t nextHop, uint16_t delay, unsigned short port){
        vector<pair<uint16_t, uint16_t>> changedMinDist;

        if(dvTbl.count(nextHop) == 0){
                dvTbl[nextHop] = unordered_map<uint16_t, TblEntry>();
        }
	uint32_t curTime = sys->time();
	if(routingTbl.count(nextHop) == 0 || routingTbl[nextHop].second == 0xffff){
		routingTbl[nextHop] = pair<uint16_t, uint16_t>(nextHop, delay);
		dvTbl[nextHop][nextHop] = TblEntry(delay, curTime);
		changedMinDist.push_back(pair<uint16_t, uint16_t>(nextHop, delay));
		return changedMinDist;
	}
	
	uint16_t oldDistToNextHop = 0;
	if(dvTbl[nextHop].count(nextHop) == 0){
		oldDistToNextHop = 0;
	}
	else{
		oldDistToNextHop = dvTbl[nextHop][nextHop].cost;	
	}

	if(oldDistToNextHop != delay){
		cout << "does not equal to dealy \n";
		changedMinDist.push_back(pair<uint16_t, uint16_t>(nextHop, delay));
		for(auto it: dvTbl){
			unordered_map<uint16_t, TblEntry> path = it.second;
			if(path.count(nextHop) != 0 && (path[nextHop].cost != 0xffff || it.first == nextHop)){
				uint16_t newDistToDest = path[nextHop].cost - oldDistToNextHop + delay;
				if(delay == 0xffff){
					newDistToDest = 0xffff;
				}
				cout<<"new DIst is " << newDistToDest << " to " << it.first  <<"\n";
				path[nextHop] = TblEntry(newDistToDest, curTime);
				it.second = path;
				dvTbl[it.first] = it.second;	
				pair<uint16_t, TblEntry> newMinPair = findMinPath(path);
				if(newMinPair.second.cost != routingTbl[it.first].second){
					routingTbl[it.first] = pair<uint16_t, uint16_t>(newMinPair.first, newMinPair.second.cost);
                                	changedMinDist.push_back(pair<uint16_t, uint16_t>(it.first, newMinPair.second.cost));
				}
			}
			if(it.first == nextHop){
				linkCosts[nextHop] = pair<uint16_t, uint16_t>(port, delay);
				portStatus[port] = pair<uint16_t, uint32_t>(nextHop, curTime);
			}
		}
		dvTbl[nextHop][nextHop] = TblEntry(delay, curTime);
	}
	else{
		portStatus[port] = pair<uint16_t, uint32_t>(nextHop, curTime);
		linkCosts[nextHop] = pair<uint16_t, uint16_t>(port, delay);
		dvTbl[nextHop][nextHop].time = curTime;
					
	}
	return changedMinDist;
}

vector<pair<uint16_t, uint16_t>> DistanceVector::updateNonNgbr(uint16_t src, uint16_t dest, uint16_t delay){
	cout<< "update non ngbr on node " << routerID << "\n";	
	vector<pair<uint16_t, uint16_t>> changedMinDist;
	if(linkCosts.count(src) == 0){
		cout<< "no way to reach " << src << "\n";
		return changedMinDist;
	}
	uint32_t curTime = sys->time();
	pair<uint16_t, uint16_t> distPairToSrc = linkCosts[src];
        uint16_t distToSrc = distPairToSrc.second;
	if(dvTbl.count(dest) == 0){
		dvTbl[dest] = unordered_map<uint16_t, TblEntry>();
		uint16_t newDist = distToSrc + delay;
		if(delay == 0xffff){
			newDist = 0xffff;
		}
		dvTbl[dest][src] = TblEntry(newDist, curTime);
		routingTbl[dest] = pair<uint16_t, uint16_t>(src, newDist);
		changedMinDist.push_back(pair<uint16_t, uint16_t>(dest, newDist));
		return changedMinDist;
	}
	
	unordered_map<uint16_t, TblEntry> paths = dvTbl[dest];
	uint16_t newDistToDest = distToSrc + delay;
	if(delay == 0xffff){
		newDistToDest = 0xffff;
	}
	paths[src] = TblEntry(newDistToDest, curTime);
	dvTbl[dest] = paths;
	pair<uint16_t, TblEntry> minPath = findMinPath(paths);
	if(minPath.second.cost != routingTbl[dest].second){
		routingTbl[dest] = pair<uint16_t, uint16_t>(minPath.first, minPath.second.cost);
        	changedMinDist.push_back(pair<uint16_t, uint16_t>(dest, minPath.second.cost));	
	}
	else{
		dvTbl[dest][src].time = curTime;
	}
	return changedMinDist;	
}

pair<uint16_t, TblEntry> DistanceVector::findMinPath(unordered_map<uint16_t, TblEntry> pathToDest){
	pair<uint16_t, TblEntry> minPath = pair<uint16_t, TblEntry>(0xffff, TblEntry(0xffff, 0xffffffff));
	for(auto path: pathToDest){
		if(path.second.cost < minPath.second.cost){
			minPath = path;
		}
	}
	return minPath;
}


pair<uint16_t, uint16_t> DistanceVector::getDistance(uint16_t dest){
	if(routingTbl.count(dest) == 0){
		return pair<uint16_t, uint32_t>(0xffff, 0xffff);	
	}
	return routingTbl[dest];
}

void DistanceVector::handleNewNeighbor(PortID port){
  this->log("handleNewNeighbor: Please implement me!\n");
};
void DistanceVector::handleTopologyChange(vector<NodeID> oldIDs){
  this->log("handleTopologyChange: Please implement me!\n");
};
void DistanceVector::route(Packet* pkt) {
  this->log("route: Please implement me!\n");
};