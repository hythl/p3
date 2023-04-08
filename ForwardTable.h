#ifndef ROUTING_TABLE_H
#define ROUTING_TABLE_H

typedef uint16_t NodeID;

class ForwardTable {
  public:
    ForwardTable() {};
    
    void addEntry(NodeID src, NodeID dst) {
      table[src] = dst;
    }

    void removeEntry(NodeID src) {
      table.erase(src);
    }

    NodeID getEntry(NodeID src) {
      return table[src];
    }

    int forward(NodeID src, NodeID dst) {
      if (src == dst)
        return src;
      if (table.find(src) == table.end())
        return -1;
      return table[dst];
    }

    void print() {
      for (auto it = table.begin(); it != table.end(); it++)
        cout << "\t\t\t" << it->first << " -> " << it->second << endl;
    }

    void clear() {
      table.clear();
    }
  private:
    map<NodeID, NodeID> table;
};

#endif