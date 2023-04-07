#ifndef LINKSTATE_DATABASE_H
#define LINKSTATE_DATABASE_H

#include <map>
#include <set>
#include <queue>
#include <iostream>

#define ENTRY_TIMEOUT 45000

using namespace std;

typedef uint16_t NodeID;

typedef struct LinkStateEntry {
  NodeID src;
  NodeID dst;
  uint32_t cost;
} LinkStateEntry;

typedef struct NodeStatus {
  uint32_t seqNum;
  uint32_t lastSeenTime;
} NodeStatus;

struct CompareLInkStateEntryCost {
  bool operator()(const LinkStateEntry& lhs, const LinkStateEntry& rhs) const {
    return lhs.cost < rhs.cost;
  }
};

class LinkStateDatabase {
  public:
    LinkStateDatabase() {};

    void addEntry(NodeID src, NodeID dst, uint32_t cost) {
      LinkStateEntry entry;
      entry.src = src;
      entry.dst = dst;
      entry.cost = cost;
      database[src][dst] = entry;
    }

    void removeEntry(NodeID src, NodeID dst){
      database[src].erase(dst);
    }

    void addNode(NodeID src, uint32_t seqNum, uint32_t lastSeenTime) {
      NodeStatus status;
      status.seqNum = seqNum;
      status.lastSeenTime = lastSeenTime;
      nodes[src] = status;
    }

    void removeNodeLinks(NodeID src) {
      for (auto it = database[src].begin(); it != database[src].end(); it++)
        database[it->second.dst].erase(src);
      database.erase(src);
    }

    void updateNode(uint16_t src, uint32_t seqNum, uint32_t lastSeenTime) {
        nodes[src].seqNum = seqNum;
        nodes[src].lastSeenTime = lastSeenTime;
    }

    void updateRoutingTable(NodeID startNode) {
      routingTable.clear();

      set<NodeID> visited;
      priority_queue<LinkStateEntry, vector<LinkStateEntry>, CompareLInkStateEntryCost> pq;

      NodeID newNode = startNode;
      while(true){
        visited.insert(newNode);

        // put all neighbors(not visited) of newNode into pq
        for (auto it = database[newNode].begin(); it != database[newNode].end(); it++) {
          if (visited.find(it->second.dst) == visited.end()) {
            pq.push(it->second);
            // cout << "pushing " << it->second.src << " " << it->second.dst << " " << it->second.cost << endl;
          }
        }

        // find the next node to visit
        while (!pq.empty()) {
          LinkStateEntry entry = pq.top();
          pq.pop();
          // cout << "popping " << entry.src << " " << entry.dst << " " << entry.cost << endl;
          if (visited.find(entry.dst) == visited.end()) {
            newNode = entry.dst;
            routingTable[newNode] = entry.src;
            // cout << "newNode " << newNode << " last hop " << entry.src << endl;
            break;
          }
        }

        if (pq.empty() && visited.find(newNode) != visited.end())
          break;
      }
    }

    int route(NodeID from, NodeID to) {
      NodeID nextHop = to;
      while (routingTable.find(nextHop) != routingTable.end())
        if (routingTable[nextHop] == from)
          return nextHop;
        else
          nextHop = routingTable[nextHop];

      return -1;
    }

    bool needUpdate(NodeID src, uint32_t seqNum) {
      if (nodes.find(src) == nodes.end())
        return true;
      else
        return nodes[src].seqNum < seqNum;
    }

    void checkExpiredEntries(uint32_t currentTime) {
      for (auto it = nodes.begin(); it != nodes.end(); it++) {
        if (currentTime - it->second.lastSeenTime > ENTRY_TIMEOUT) {
          cout << "Node " << it->first << " is expired" << endl;
          removeNodeLinks(it->first);
          nodes.erase(it->first);
        }
      }
    }

    void displayLSDB() {
      cout << "**************************************" << endl;
      for (auto it = database.begin(); it != database.end(); it++) {
        cout << "\t\t\tNode " << it->first << " is at seqNum " << nodes[it->first].seqNum << " last seen at " << nodes[it->first].lastSeenTime << endl;
        for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
          cout << "\t\t\t\tNode " << it2->second.dst << " cost " << it2->second.cost << endl;
        }
      }
      cout << "**************************************" << endl;
    }

    void displayRoutingTable() {
      cout << "**************************************" << endl;
      for (auto it = routingTable.begin(); it != routingTable.end(); it++)
        cout << "\t\t\tNode " << it->first << " last hop " << it->second << endl;
      cout << "**************************************" << endl;
    }

  private:
    map<NodeID, map<NodeID, LinkStateEntry>> database;
    map<NodeID, NodeID> routingTable;
    map<NodeID, NodeStatus> nodes;
};

#endif