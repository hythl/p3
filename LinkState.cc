#include "impl.h"

LinkState::LinkState(Node *sysIn){
  sys = sysIn;
}

LinkState::~LinkState() {
  // add your own code (if needed)
}

void LinkState::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  // add your own code
  cout << "LinkState::init" << endl;
  fflush(stdout);
}

void LinkState::handle_alarm(void *data) {
  // add your own code
}

void LinkState::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
}

// add more of your own code
