#include "RoutingProtocolImpl.h"
#include <iostream>
#include <arpa/inet.h>
#include "Node.h"
#include <set>
#define DATAPORT 0xffff
#define DATA_TYPE 0x01
#define PING_TYPE 0x02
#define PONG_TYPE 0x03
#define DV_TYPE 0x04
using namespace std;

/**
 * Min Distance: Map(nodeId, Pair(nextNodeId, distance))
 * Port Status: Map(nodeId, Pair(portNumber, time))
 * Routing Table: Map(nodeId, Map(nextNodeId, DIstance))
 */


RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
	sys = n;
	cout<< "start to run \n";
	fflush(stdout);	
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
	cout<< "Deconstructor is called \n";
	fflush(stdout);
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  // add your own code
	this->numOfPorts = num_ports;
	this->routerId = router_id;
	this->protocolType = protocol_type;	
//	int* pingEvent = (int*) malloc(sizeof(int));
//        *pingEvent = 1;
	cout<<"Try to init \n";
//	cout<<"PingEbent is " << *pingEvent << "at " << pingEvent << "\n";
//        sys->set_alarm(this, 10000, &(this->pingEvent));
	sendPing();
	sys->set_alarm(this, 1000, &(this->linkCheckEvent));
	sys->set_alarm(this, 1000, &(this->entryCheckEvent));
        sys->set_alarm(this, 30000, &(this->updateEvent)); 
	cout<<"Success in set alarm on " << router_id << "\n";
//	cout<<"Ping event is " << this->pingEvent << "\n";
	fflush(stdout);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
//	cout<<"handle alarm on node " << this->routerId << "\n";
//	fflush(stdout);
//	printPortStatus();
	int* typeOfEvent = (int*) data;
//	cout<<"data is "<<*typeOfEvent << " on node " << this->routerId << "\n"; 
	if(*typeOfEvent == this->pingEvent){
		cout<<"Ping event invoked \n";
                sendPing();
	}
	if(*typeOfEvent == this->linkCheckEvent){
		cout<<"link check invoked \n";
		linkCheck();
	}
	if(*typeOfEvent == this->entryCheckEvent){
		cout<<"entry check invoked \n";
		entryCheck();
	}
	if(*typeOfEvent == this->updateEvent){
		cout<<"update event invoked \n";
		handleUpdateEvent();
	}
//	printPortStatus();
//	switch(*typeOfEvent){
//		case this->pingEvent:
//			cout<<"Ping event invoked \n";
//			sendPing();
//			break;
//		default:
//			break;		
//	}
//	free(data);	
  // add your own code
}

void RoutingProtocolImpl::handleUpdateEvent(){
	vector<pair<uint16_t, uint16_t>> datas;
	for(auto routingEntry: routingTbl){
		if(routingEntry.second.second == 0xffff){
			continue;
		}
		datas.push_back(pair<uint16_t, uint16_t>(routingEntry.first, routingEntry.second.second));
	}
	sendUpdateToAll(datas, false, routerId);
	sys->set_alarm(this, 30000, &(this->updateEvent));	
}

void RoutingProtocolImpl::entryCheck(){
	uint32_t curTime = sys->time();
	for(auto tblPair: dvTbl){
		unordered_map<uint16_t, TblEntry> paths= tblPair.second;
		uint16_t dest = tblPair.first;
		for(auto path: paths){
			uint32_t lastSeen = path.second.time;
			uint16_t nextHop = path.first;
			uint16_t cost = path.second.cost;
			if(curTime - lastSeen > 45000 && cost != 0xffff){
				cout<<"path from " << routerId << " to " << dest << " through " << nextHop << " expires \n";
				if(nextHop != dest){
					sendUpdateToAll(updateNonNgbr(nextHop, dest, 0xffff), false, nextHop);
				}
			}
		}		
	}
	sys->set_alarm(this, 1000, &(this->entryCheckEvent));			
}


void RoutingProtocolImpl::linkCheck(){
	uint32_t curTime = sys->time();
	vector<pair<uint16_t, uint16_t>> changes;
//	printPortStatus();
	cout<<"current time is " << curTime << "\n";
	unordered_map<uint16_t, pair<uint16_t, uint32_t>> portStatusCopy;
	for(auto portStatusPair: portStatus){
		portStatusCopy[portStatusPair.first] = portStatusPair.second;
	}
	for(auto portStatusPair: portStatusCopy){
		uint16_t src = portStatusPair.second.first;
		uint16_t port = portStatusPair.first;
//		cout<< "src is " << src << "\n";
		if(curTime - portStatusPair.second.second > 15000){
			cout << "link from " << routerId << " to " << src << "curshed " << "last seen "
			<< portStatusPair.second.second <<"\n";
		//	deletedPorts.push_back(portStatusPair.first);
	//		changes.push_back(updateNgbr(portStatusPair.first, 0xffff, portStatusPair.first));
		//	portStatus.erase(portStatusPair.first);
		//	linkCosts.erase(portStatusPair.second.first);;
			changes = updateNgbr(src, 0xffff, port);
			portStatus.erase(portStatusPair.first);
			linkCosts.erase(src);
			sendUpdateToAll(changes, false, src);						
		}
		
	}
//	printPortStatus();	
	sys->set_alarm(this, 1000, &(this->linkCheckEvent));			
}

void RoutingProtocolImpl::printPortStatus(){
	cout << "print port staus of node " <<  routerId << "\n";
	for(auto portStatusEntry: portStatus){
		cout << "port: " << portStatusEntry.first << " to Node: " << portStatusEntry.second.first
		 << " timestap: "  << portStatusEntry.second.second << "\n";
	}
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
  //	cout<<"Receive packet \n";
  //      fflush(stdout);
 // 	printPortStatus();
  	if(port == DATAPORT){
		cout<<"Receive data pkg \n";
		handleDataPkg(packet);
		return;
	}
	uint8_t* packetTypePkg = (uint8_t*) packet;
//	uint16_t* packetSizePkg = ((uint16_t*) packet) + 1;
	uint8_t packetType =  *packetTypePkg;
//	uint16_t packetSize = ntohs(*packetSizePkg);
//	cout <<"packet type is " << packetType << "\n";
//	cout <<"packet size is " << packetSize << "\n";
//	cout <<"expect size of " << size << "\n";	 	
	switch(packetType){
    		case PING:
//      			cout<< "Receive Ping packet"<< "\n";
			handlePingPkg(packet, port);
			break;
		case PONG:
//			cout<< "Receive Pong packet"<< "\n";
			handlePongPkg(packet, port);
			break;
		case DV:
//			cout<<"Receive DV packet" << "\n";
			handleDVPkg(packet, port);
			break;
		case DATA:
			cout <<"Receive data packet" << "\n";
			handleDataPkg(packet);			
			break;
    		default:
      			break;
  	}
//	free(packet);
//	printPortStatus();
}

void RoutingProtocolImpl::handleDataPkg(void *pkg){
	uint16_t* temp = (uint16_t*) pkg;
//        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
//        uint16_t src = ntohs(*(temp + 2));
        uint16_t dest = ntohs(*(temp + 3));
	if(dest != routerId){
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
		cout<<"data received at " << routerId << "\n";
		free(pkg);
		return;		
	}
}

void RoutingProtocolImpl::handleDVPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
        uint16_t src = ntohs(*(temp + 2));
	uint16_t dest = ntohs(*(temp + 3));
//	printPortStatus();
	if(dest != routerId){
		if(routingTbl.count(dest) == 0){
			cout<<"can not find a path to " << dest << " on node " << routerId << "\n";
			return;
		}
		unsigned short nextPort = linkCosts[routingTbl[dest].first].first;
		sys->send(nextPort, pkg, size);
		return;
	}
	cout<<"Receive DV  on node " << this->routerId << " with type "
        << pkgType << " with size " << size <<" from " << src << "\n";
	temp = temp + 4;
	vector<vector<pair<uint16_t, uint16_t>>> updatesForAll;
	for(int i = 0; i < (size - 8) / 4; i++){
//		printPortStatus();
		uint16_t nodeId = ntohs(*temp);
		temp = temp + 1;
		uint16_t cost = ntohs(*temp);
		temp = temp + 1;
		cout<< "Update distance to node " << nodeId << " to cost " << cost << " from " << src << "\n";
//		updates.push_back(pair<uint16_t, uint16_t>(nodeId, cost));
		if(portStatus.count(port) == 0){
			continue;
		}
		uint16_t nextHop = portStatus[port].first;
		if(nodeId == routerId && nextHop == src){
//			updatesForAll.push_back(updateNgbr(src, cost, portStatus[port].first));
			updatesForAll.push_back(updateNgbr(src, cost, port));
		}
		else if(nodeId == routerId){
			continue;
		}
		else{
			updatesForAll.push_back(updateNonNgbr(src, nodeId, cost));
		}
//		cout<< "Update node " << nodeId << " to cost " << cost << "\n";
	}
	cout<< "The Routing table on node " << routerId << " is " << "\n";
	printRoutingTbl();
	cout<< "The DV table on node" << routerId << " is " << "\n";
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
//	printPortStatus();
//	sendUpdateToAll(realChanges);	
}

// add more of your own code
//void RoutingProtocolImpl::handleDataPkg(){
//  cout << "Data generated \n";
//  fflush(stdout);
//}

void RoutingProtocolImpl::handlePingPkg(void* pkg, unsigned short port){
//	cout<<"Receive pkg on node " << this->routerId << "\n";
	uint16_t* temp = (uint16_t*) pkg;
	uint16_t pkgType = ntohs(*temp);
	uint16_t size = ntohs(*(temp + 1));
	uint16_t src = ntohs(*(temp + 2));
	uint32_t* temp32 = (uint32_t*) (temp + 4);
	uint32_t timeStamp = ntohl(*temp32);
	cout<<"Receive PING on node " << this->routerId << " with type PING"
	 << " with size " << size << " timeStamp " << timeStamp <<
	" from " << src << "\n";
	sendPong(src, timeStamp, port); 
	fflush(stdout);
	free(pkg);
}

void RoutingProtocolImpl::sendPing(){
	cout<<"start to send Ping on node" << this->routerId << "\n";
	fflush(stdout);	
	for(unsigned short i = 0; i < this->numOfPorts; i++){
		uint16_t* pkg = (uint16_t*) malloc(6 * sizeof(uint16_t));
		uint8_t* temp1 = (uint8_t*)pkg;
		*temp1 = PING;
		*(pkg + 1) = htons(12);
		*(pkg + 2) = htons(this->routerId);
		uint32_t* temp = (uint32_t*) (pkg + 4);
//		cout<<"The routerId sent " << this->routerId << "\n";
		*temp = htonl(sys->time());
//		cout<<"The timestamp sent " << sys->time() << "\n";
		sys->send(i, pkg, 12);		
	}
        sys->set_alarm(this, 10000, &(this->pingEvent));		
}

void RoutingProtocolImpl::sendPong(uint16_t src, uint32_t timeStamp, unsigned short port){
	uint16_t* pkg = (uint16_t*) malloc(6 * sizeof(uint16_t));
        *pkg = PONG;
	*(pkg + 1) = htons(12);
        *(pkg + 2) = htons(this->routerId);
	*(pkg + 3) = htons(src);
        uint32_t* temp = (uint32_t*) (pkg + 4);
//        cout<<"The routerId sent " << this->routerId << "\n";
        *temp = htonl(timeStamp);
//        cout<<"The timestamp sent " << timeStamp  << "\n";
        sys->send(port, pkg, 12);	
}

void RoutingProtocolImpl::handlePongPkg(void* pkg, unsigned short port){
	//parse packet
	//update port status
	//Remember old distance from this node to sourceId
	//for every destination
		//update entry where nextNodeId = sourceId
			//if updated distance < min Distance
				//update minDistance

	uint16_t* temp = (uint16_t*) pkg;
        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
        uint16_t src = ntohs(*(temp + 2));
	uint16_t dest = ntohs(*(temp + 3));
        uint32_t* temp32 = (uint32_t*) (temp + 4);
        uint32_t timeStamp = ntohl(*temp32);
	uint16_t delay = (uint16_t)sys->time() - timeStamp;
        cout<<"Receive PONG on node " << this->routerId << " with type "
        << pkgType << " with size " << size << " timeStamp " << timeStamp <<
        " from " << src << "\n";
//	printPortStatus();
//	portStatus[port] = std::pair<uint16_t, uint32_t>(src, sys->time());
//	cout<<"src inserted is " << src << "\n";
//	linkCosts[src] = std::pair<unsigned short, uint16_t>(port, delay);
//	vector<pair<uint16_t, uint16_t>> changedRouting = updateNgbr(src, delay, src);
	unsigned short nextPort = port;
//	if(routingTbl.count(src) != 0){
//		uint16_t nextNode = routingTbl[src].first;
//		nextPort = linkCosts[nextNode].first;
//	}
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
//	sendUpdateToAll(changedRouting);
	portStatus[port] = std::pair<uint16_t, uint32_t>(src, sys->time());
      //  cout<<"src inserted is " << src << "\n";
	linkCosts[src] = std::pair<unsigned short, uint16_t>(port, delay);
//	printPortStatus();
	sendUpdateToAll(changedRouting, isNew, src);
	free(pkg);
//	printPortStatus();
//	sendUpdateToAll(changedRouting);	
//	cout<< "sucess until here \n";
//	minDistances[src] = std::pair<unsigned short, uint32_t>(port, delay);
	
//	cout<<"The delay of node " << src << " is " << portStatus[src].second << "\n";					  
}

void RoutingProtocolImpl::sendUpdateToAll(vector<pair<uint16_t, uint16_t>> changes, bool isNew, uint16_t src){
//	cout<<"send update to all " << " \n";
//	fflush(stdout);
	if(changes.size() == 0){
		return;
	}
	for(auto link: linkCosts){
//		cout<< status.first << "\n";
		if(link.first == src && isNew){
			sendUpdate(changes, link.first, linkCosts[routingTbl[link.first].first].first, true);
		}
		else{		
			sendUpdate(changes, link.first, linkCosts[routingTbl[link.first].first].first, false);
		}
	} 
}

void RoutingProtocolImpl::sendUpdate(vector<pair<uint16_t, uint16_t>> changes, uint16_t dest, uint16_t port, bool isNew){
	cout<< "send update to " << dest << "from node" << routerId << "\n";
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
        *(pkg + 2) = htons(this->routerId);
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

void RoutingProtocolImpl::printDVTbl(){
//	cout<<"start to print dvTbl \n";
	for(auto pairs: dvTbl){
		unordered_map<uint16_t, TblEntry> paths = pairs.second;
		for(auto path: paths){
			cout<<pairs.first << " " << path.first << " " << path.second.cost <<" " << path.second.time << "\n";
		}
	}
}

void RoutingProtocolImpl::printRoutingTbl(){
//	cout<<"start to print Routing table \n";
	for(auto pairs: routingTbl){
		cout<< pairs.first << " " << pairs.second.first << " " << pairs.second.second << "\n";
	}
}


vector<pair<uint16_t, uint16_t>> RoutingProtocolImpl::updateNgbr(uint16_t nextHop, uint16_t delay, unsigned short port){
        vector<pair<uint16_t, uint16_t>> changedMinDist;
//	if(delay == 0xffff){
//		return changedMinDist;
//	}

        if(dvTbl.count(nextHop) == 0){
                dvTbl[nextHop] = unordered_map<uint16_t, TblEntry>();
        }
/**
        if(linkCosts.count(nextHop) == 0){
		cout << "Find a new neighbor. Dump all message I have \n";
		vector<pair<uint16_t, uint16_t>> allDestInfo;
		for(auto routingEntry: routingTbl){
			allDestInfo.push_back(pair<uint16_t, uint16_t>(routingEntry.first, routingEntry.second.second));
		}
		sendUpdate(allDestInfo, nextHop, port);
		dvTbl[nextHop][nextHop] = delay;
		pair<uint16_t, uint16_t> minPath = findMinPath(dvTbl[nextHop]);
		routingTbl[nextHop] = pair<uint16_t, uint16_t>(nextHop, delay);		
		routingTbl[nextHop] = pair<uint16_t, uint16_t>(nextHop, delay);
		changedMinDist.push_back(pair<uint16_t, uint16_t>(nextHop, delay));
		for(auto statusEntry: portStatus){
			sendUpdate(changedMinDist, statusEntry.second.first, statusEntry.first);
		}
		vector<pair<uint16_t, uint16_t>> empty;
		return empty;
	}
*/
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
		//do we need update link cost
	//	cout <<"equla to delay \n";
	//	cout <<"curTime is " << curTime << "\n";
		portStatus[port] = pair<uint16_t, uint32_t>(nextHop, curTime);
	//	cout <<"print port  status after equal \n";
	//	printPortStatus();
		linkCosts[nextHop] = pair<uint16_t, uint16_t>(port, delay);
		dvTbl[nextHop][nextHop].time = curTime;
					
	}
	return changedMinDist;
}

vector<pair<uint16_t, uint16_t>> RoutingProtocolImpl::updateNonNgbr(uint16_t src, uint16_t dest, uint16_t delay){
	cout<< "update non ngbr on node " << routerId << "\n";	
	vector<pair<uint16_t, uint16_t>> changedMinDist;
	if(linkCosts.count(src) == 0){
		cout<< "no way to reach " << src << "\n";
		return changedMinDist;
	}
	uint32_t curTime = sys->time();
//	if(delay == 0xffff){
//                changedMinDist.push_back(pair<uint16_t, uint16_t>(src, routingTbl[src].second));
//		return changedMinDist;
//        }
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
//		cout<<"change add to list dest: " << distPairToSrc.first << " cost: " << distToSrc + delay << "\n";
//		cout<<"The size of changedMinDist is " << changedMinDist.size() <<"\n";
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
//	for(auto pathsPair: dvTbl[dest]){
//		unordered_map<uint16_t, uint16_t> paths = pathsPair.second;
//		paths[src] = distToSrc + delay;
//		pair<uint16_t, uint16_t> newMinToDest		
//		if(paths.count(src) == 0){
//			paths[src] = distToSrc + delay;
//			routingTbl[src] = pair<uint16_t, uint16_t>(dest, distToSrc + delay);
//			changedMinDist.push_back(pair<uint16_t, uint16_t>(dest, distToSrc + delay));
//			
//		}		
//	} 	
}


//vector<pair<uint16_t, uint32_t>> RoutingProtocolImpl::updateNgbr(uint16_t dest, uint32_t delay, uint16_t nextHop){
//	vector<pair<uint16_t, uint32_t>> changedMinDist;
//	pair<uint16_t, uint32_t> oldDistancePair = getDistance(dest);		
//	uint32_t oldDistance = oldDistancePair.second;
//	if(dvTbl.count(dest) == 0){
//		dbTbl[dest] = unordered_map<uint16_t, uint32_t>();			
//	}
//	if(routingTbl.count(dest) == 0){
//		routingTbl[dest] = pair<uint16_t, uint32_t>(nextHop, delay);
//	}
	
//	for(auto it: dvTbl){
/**		if(it.second.count(dest) != 0){
			uint32_t oldDistDest = it.second[dest];
			uint32_t newDistDest = oldDistDest - oldDistance + delay;
			it.second[dest] = newDistDest;
			
		}
		else if(it.second.count(dest) == 0 && it.first == dest){
			it.second[dest] = delay;	
		}
		else{
			continue;
		}
		pair<uint16_t, uint32_t> oldDistanceItPair = getDistance(it.first);
		uint32_t newDistIt = it.second[dest];
		if(newDistIt <= oldDistanceItPair.second){
			pair<uint16_t, uint32_t> newDistanceItPair = pair<uint16_t, uint32_t>(it.first, newDistIt);
			routingTbl[it.first] = pair<uint16_t, uint32_t>(nextHop, newDistIt);
			changedMinDist.push_back(newDistanceItPair);	
		}
		pair<uint16_t, uint32_t> oldMinPath = routingTbl[it.first];
		pair<uint16_t, uint32_t> newMinPath = findMinPath(it.second);
		if(newMinPath.first != oldMinPath.first || newMinPath.second != oldMinPath.second){
			routingTbl[it.first] = newMinPath;
			changedMinDist.insert_back(pair<uint16_t, uint32_t>(it.first, newMinPath.second));
		} 
	} 
	
	return changedMinDist;
}
*/
pair<uint16_t, TblEntry> RoutingProtocolImpl::findMinPath(unordered_map<uint16_t, TblEntry> pathToDest){
	pair<uint16_t, TblEntry> minPath = pair<uint16_t, TblEntry>(0xffff, TblEntry(0xffff, 0xffffffff));
	for(auto path: pathToDest){
		if(path.second.cost < minPath.second.cost){
			minPath = path;
		}
	}
	return minPath;
}


pair<uint16_t, uint16_t> RoutingProtocolImpl::getDistance(uint16_t dest){
	if(routingTbl.count(dest) == 0){
		return pair<uint16_t, uint32_t>(0xffff, 0xffff);	
	}
	return routingTbl[dest];
}
