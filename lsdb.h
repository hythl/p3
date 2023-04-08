#ifndef LINKSTATE_DATABASE_H
#define LINKSTATE_DATABASE_H

#include <map>
#include <set>
#include <queue>
#include <iostream>

#include "ForwardTable.h"

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
    return lhs.cost > rhs.cost;
  }
};

class LinkStateDatabase {
  public:
    LinkStateDatabase() {};

    void addOrUpdateEntry(NodeID src, NodeID dst, uint32_t cost) {
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

    void updateForwardingTable(NodeID startNode, ForwardTable& forwardTable) {
      nodeToParent.clear();
      nodeToParent[startNode] = make_pair(startNode, 0);

      map<uint32_t, vector<NodeID>> costHeap;

      NodeID currentNode = startNode;
      set<NodeID> visited;
      while(true){
        visited.insert(currentNode);
        for (auto it = database[currentNode].begin(); it != database[currentNode].end(); it++) {
          // this node has been visited
          if (visited.find(it->second.dst) != visited.end())
            continue;

          uint32_t cost = nodeToParent[currentNode].second + it->second.cost;
          // this node has no recorded cost
          if (nodeToParent.find(it->second.dst) == nodeToParent.end()) {
            nodeToParent[it->second.dst] = make_pair(currentNode, cost);
            costHeap[cost].push_back(it->second.dst);
          } else {
            // this node has a recorded cost
            uint32_t oldCost = nodeToParent[it->second.dst].second;
            if (cost < oldCost) {
              nodeToParent[it->second.dst] = make_pair(currentNode, cost);
              for (auto it2 = costHeap[oldCost].begin(); it2 != costHeap[oldCost].end(); it2++)
                if (*it2 == it->second.dst) {
                  costHeap[oldCost].erase(it2);
                  break;
                }
              if (costHeap[oldCost].empty())
                costHeap.erase(oldCost);
              costHeap[cost].push_back(it->second.dst);
            }
          }
        }

        if (costHeap.empty())
          break;

        // pick the next node to visit
        while (visited.find(costHeap.begin()->second.back()) != visited.end()){
          costHeap.begin()->second.pop_back();
          if (costHeap.begin()->second.empty())
            costHeap.erase(costHeap.begin());
        }

        if (costHeap.empty())
          break;

        currentNode = costHeap.begin()->second.back();
        costHeap.begin()->second.pop_back();
        if (costHeap.begin()->second.empty())
          costHeap.erase(costHeap.begin());
      }

      // update forwarding table
      forwardTable.clear();
      for (auto it = nodeToParent.begin(); it != nodeToParent.end(); it++) {
        NodeID currentNode = it->first;
        while (nodeToParent[currentNode].first != startNode)
          currentNode = nodeToParent[currentNode].first;
        forwardTable.addEntry(it->first, currentNode);
      }
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
      cout << "\t\t\t**************************************" << endl;
      for (auto it = database.begin(); it != database.end(); it++) {
        cout << "\t\t\tNode " << it->first << " is at seqNum " << nodes[it->first].seqNum << " last seen at " << nodes[it->first].lastSeenTime << endl;
        for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
          cout << "\t\t\t\tNode " << it2->second.dst << " cost " << it2->second.cost << endl;
        }
      }
      cout << "\t\t\t**************************************" << endl;
    }

  private:
    map<NodeID, NodeStatus> nodes;
    map<NodeID, map<NodeID, LinkStateEntry>> database;
    map<NodeID, pair<NodeID, uint32_t>> nodeToParent;
};

#endif