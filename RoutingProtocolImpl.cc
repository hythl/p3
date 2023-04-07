#include "RoutingProtocolImpl.h"
#include <iostream>
#include <arpa/inet.h>
#include "Node.h"
#include <set>
using namespace std;

/**
 * Min Distance: Map(nodeId, Pair(nextNodeId, distance))
 * Port Status: Map(nodeId, Pair(portNumber, time))
 * Routing Table: Map(nodeId, Map(nextNodeId, DIstance))
 */


RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
	sys = n;
}

RoutingProtocolImpl::~RoutingProtocolImpl() {}


void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
	this->numOfPorts = num_ports;
	this->routerId = router_id;

	NeighborSniff();

	sys->set_alarm(this, 1000, &(this->entryCheck)); // entry expiration check: runs every 1 second
//	sys->set_alarm((RoutingProtocol*)this, 10000, &(this->pingEvent)); // Trigger Ping
  	sys->set_alarm((RoutingProtocol*)this, 30000, &(this->update));    // update: runs every 30 seconds

  cout << "router " << router_id << " initialized, using DV" << endl;
  fflush(stdout);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  AlarmType at = *(AlarmType*)data;
  switch(at){
    case PING_ALARM:
 //     cout << "Router " << routerId << "> ping  triggered, time:" << sys->time() << endl;
      fflush(stdout);
      NeighborSniff();
      break;
    case ENTRY_CHECK:
//      cout << "Router " << routerId << "> entry check alarm triggered, time:" << sys->time() << endl;
      fflush(stdout);
      handleEntryCheck();
      break;
    case UPDATE:
  //    cout << "Router " << routerId << "> update alarm triggered, time:" << sys->time() << endl;
      fflush(stdout);
      handleUpdateEvent();
      break;
    default:

      break;
  }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  	if(port == DATAPORT){
		cout<<"Receive data pkg \n";
		handleDataPkg(packet);
		return;
	}
	uint8_t* packetTypePkg = (uint8_t*) packet;
	uint8_t packetType =  *packetTypePkg;
	switch(packetType){
    		case PING:
			handlePingPkg(packet, port);
			break;
		case PONG:
			handlePongPkg(packet, port);
			break;
		case DV:
			handleDVPkg(packet, port);
			break;
		case DATA:
			cout <<"Receive data packet" << "\n";
			handleDataPkg(packet);			
			break;
    		default:
      			break;
  	}
}

void RoutingProtocolImpl::handleUpdateEvent(){
	vector<pair<uint16_t, uint16_t>> datas;
	//Dump all of the info to the new ngbr
	for(auto routingEntry: routingTbl){
		datas.push_back(pair<uint16_t, uint16_t>(routingEntry.first, routingEntry.second.second));
	}
	sendUpdateToAll(datas, false, routerId);
	sys->set_alarm((RoutingProtocol*)this, 30000, &(this->update));	
}

/**
 * This function checkes the fresheness of DV entry
 */
void RoutingProtocolImpl::handleEntryCheck(){
	uint32_t curTime = sys->time();
	vector<vector<pair<uint16_t, uint16_t>>> updatesForAll;
	for(auto tblPair: dvTbl){
		unordered_map<uint16_t, TblEntry> paths= tblPair.second;
		uint16_t dest = tblPair.first;
		for(auto path: paths){
			uint32_t lastSeen = path.second.time;
			uint16_t nextHop = path.first;
			uint16_t cost = path.second.cost;
			//if the direct expires and the path exist, remove the entry
			if(curTime - lastSeen > 15000 && dest == nextHop && cost != 0xffff){
                                updatesForAll.push_back(updateNgbr(nextHop, 0xffff));
                        }
			// if the entry expires and the path exist, remove the entry
			else if(curTime - lastSeen > 45000 && cost != 0xffff){
				cout<<"path from " << routerId << " to " << dest << " through " << nextHop << " expires \n";
				updatesForAll.push_back(updateNonNgbr(nextHop, dest, 0xffff));
			}
			
		}		
	}
	// when multiple paths toward same destination failed, we only keep the newest change. 
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
	sendUpdateToAll(realChanges, false, routerId);
	sys->set_alarm((RoutingProtocol*)this, 1000, &(this->entryCheck));			
}

void RoutingProtocolImpl::handleDataPkg(void *pkg){
	uint16_t* temp = (uint16_t*) pkg;
  	uint16_t size = ntohs(*(temp + 1));
  	uint16_t dest = ntohs(*(temp + 3));
	if(dest != routerId){
		if(routingTbl.count(dest) == 0 || routingTbl[dest].second == 0xffff){
			cout << "drop packet since no path to destination found " << "\n";
			free(pkg);
			return;
		}
		unsigned short nextPort = linkInfo[routingTbl[dest].first]; 		
		sys->send(nextPort, pkg, size);
	}
	else{
		free(pkg);
		return;		
	}
}

void RoutingProtocolImpl::handleDVPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
        uint16_t src = ntohs(*(temp + 2));
	cout<<"Receive DV  on node " << this->routerId << " with type "
        << pkgType << " with size " << size <<" from " << src << "\n";
	temp = temp + 4;
	vector<vector<pair<uint16_t, uint16_t>>> updatesForAll;
	for(int i = 0; i < (size - 8) / 4; i++){
		uint16_t nodeId = ntohs(*temp);
		temp = temp + 1;
		uint16_t cost = ntohs(*temp);
		temp = temp + 1;
		cout<< "Update distance to node " << nodeId << " to cost " << cost << " from " << src << "\n";
		//Node should not receive a distance to itself from ngbr
		if(nodeId == routerId){
//			cout<< "This should not happen \n";
			continue;
		}
		else{
			updatesForAll.push_back(updateNonNgbr(src, nodeId, cost));
		}
	}

	cout<< "The Routing table on node " << routerId << " is " << "\n";
	printRoutingTbl();
	cout<< "The DV table on node" << routerId << " is " << "\n";
	printDVTbl();
	
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
	free(pkg);;
}

void RoutingProtocolImpl::handlePingPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
	uint16_t size = ntohs(*(temp + 1));
	uint16_t src = ntohs(*(temp + 2));
	uint32_t* temp32 = (uint32_t*) (temp + 4);
	uint32_t timeStamp = ntohl(*temp32);
	cout<<"Receive PING on node " << this->routerId << " with type PING"
	 << " with size " << size << " timeStamp " << timeStamp <<
	" from " << src << "\n";
	sendPong(src, timeStamp, port); 
	free(pkg);
}

void RoutingProtocolImpl::NeighborSniff(){
	for(unsigned short i = 0; i < this->numOfPorts; i++){
		uint16_t* pkg = (uint16_t*) malloc(6 * sizeof(uint16_t));
		uint8_t* temp1 = (uint8_t*)pkg;
		*temp1 = PING;
		*(pkg + 1) = htons(12);
		*(pkg + 2) = htons(this->routerId);
		uint32_t* temp = (uint32_t*) (pkg + 4);
		*temp = htonl(sys->time());
		sys->send(i, pkg, 12);		
	}
  sys->set_alarm((RoutingProtocol*)this, 10000, &(this->pingEvent));		
}

void RoutingProtocolImpl::sendPong(uint16_t src, uint32_t timeStamp, unsigned short port){	
	uint16_t* pkg = (uint16_t*) malloc(6 * sizeof(uint16_t));
        *pkg = PONG;
	*(pkg + 1) = htons(12);
        *(pkg + 2) = htons(this->routerId);
	*(pkg + 3) = htons(src);
        uint32_t* temp = (uint32_t*) (pkg + 4);
        *temp = htonl(timeStamp);
        sys->send(port, pkg, 12);
}

void RoutingProtocolImpl::handlePongPkg(void* pkg, unsigned short port){
	uint16_t* temp = (uint16_t*) pkg;
        uint16_t pkgType = ntohs(*temp);
        uint16_t size = ntohs(*(temp + 1));
        uint16_t src = ntohs(*(temp + 2));
        uint32_t* temp32 = (uint32_t*) (temp + 4);
        uint32_t timeStamp = ntohl(*temp32);
	uint16_t delay = (uint16_t)sys->time() - timeStamp;

        cout<<"Receive PONG on node " << this->routerId << " with type "
        << pkgType << " with size " << size << " timeStamp " << timeStamp <<
        " from " << src << "\n";

	vector<pair<uint16_t, uint16_t>> changedRouting = updateNgbr(src, delay);
	cout<< "DV table update to \n";
	printDVTbl();
	cout<<"Routing Table update to \n";
	printRoutingTbl();
	bool isNew = false;
	//find new ngbr, dump all info I know
	if(linkInfo.count(src) == 0){
		cout<< "Find a new link. Dump all info to " << src << "\n";
		isNew = true;
	}	
	linkInfo[src] = port;
	sendUpdateToAll(changedRouting, isNew, src);
	free(pkg);
}

void RoutingProtocolImpl::sendUpdateToAll(vector<pair<uint16_t, uint16_t>> changes, bool isNew, uint16_t src){
	if(changes.size() == 0){
		return;
	}
	for(auto link: linkInfo){
		if(link.first == src && isNew){
			sendUpdate(changes, link.first, link.second, true);
		}
		else{		
			sendUpdate(changes, link.first, link.second, false);
		}
	} 
}

void RoutingProtocolImpl::sendUpdate(vector<pair<uint16_t, uint16_t>> changes, uint16_t dest, uint16_t port, bool isNew){
	cout<< "send update to " << dest << "from node" << routerId << "\n";

	vector<pair<uint16_t, uint16_t>> changesPoisonRv;
	if(isNew){
		for(auto routingEntry: routingTbl){
			changes.push_back(pair<uint16_t, uint16_t>(routingEntry.first, routingEntry.second.second));			
		}
	}

	for(auto change: changes){
		cout<< "change is dest: " <<change.first <<" cost: " << change.second << "\n";
                uint16_t nextHop = routingTbl[change.first].first;
		//changed entry is the link with ngbr
		//if(change.first == dest){
                //        continue;
               // }

		//poison reverse
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
	for(auto pairs: dvTbl){
		unordered_map<uint16_t, TblEntry> paths = pairs.second;
		for(auto path: paths){
			cout<<pairs.first << " " << path.first << " " << path.second.cost <<" " << path.second.time << "\n";
		}
	}
}

void RoutingProtocolImpl::printRoutingTbl(){
	for(auto pairs: routingTbl){
		cout<< pairs.first << " " << pairs.second.first << " " << pairs.second.second << "\n";
	}
}


vector<pair<uint16_t, uint16_t>> RoutingProtocolImpl::updateNgbr(uint16_t nextHop, uint16_t delay){
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
	if(dvTbl[nextHop].count(nextHop) != 0){
		oldDistToNextHop = dvTbl[nextHop][nextHop].cost;
	}

	if(oldDistToNextHop != delay){
		
		for(auto it: dvTbl){
			unordered_map<uint16_t, TblEntry> path = it.second;
			//cost == 0xffff means that there wasn't a route to destination, then updating ngbr does not work
			//except when it is ngbr
			if((path.count(nextHop) != 0 && path[nextHop].cost != 0xffff) || it.first == nextHop){
				uint16_t newDistToDest = path[nextHop].cost - oldDistToNextHop + delay;
				if(delay == 0xffff){
					newDistToDest = 0xffff;
				}
				cout<<"new DIst is " << newDistToDest << " to " << it.first  <<"\n";
				path[nextHop] = TblEntry(newDistToDest, curTime);
				it.second = path;
				dvTbl[it.first] = it.second;	
				pair<uint16_t, TblEntry> newMinPair = findMinPath(path);
				//The minimum is updated
				if(newMinPair.second.cost != routingTbl[it.first].second){
					routingTbl[it.first] = pair<uint16_t, uint16_t>(newMinPair.first, newMinPair.second.cost);
                                	changedMinDist.push_back(pair<uint16_t, uint16_t>(it.first, newMinPair.second.cost));
				}
			}
		}
	}
	if(delay == 0xffff){
		linkInfo.erase(nextHop);
	}
	dvTbl[nextHop][nextHop] = TblEntry(delay, curTime);
	return changedMinDist;
}

vector<pair<uint16_t, uint16_t>> RoutingProtocolImpl::updateNonNgbr(uint16_t src, uint16_t dest, uint16_t delay){
	cout<< "update non ngbr on node " << routerId << "\n";	
	vector<pair<uint16_t, uint16_t>> changedMinDist;
	if(linkInfo.count(src) == 0){
		cout<< "no way to reach " << src << "\n";
		return changedMinDist;
	}
	uint32_t curTime = sys->time();
        uint16_t distToSrc = dvTbl[src][src].cost;
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

