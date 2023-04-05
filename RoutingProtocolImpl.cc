#include "RoutingProtocolImpl.h"
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
  trueImpl = protocol_type == P_DV ? (Impl*)new DistanceVector(sys) : (Impl*)new LinkState(sys);
  trueImpl->init(num_ports, router_id, protocol_type);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  trueImpl->handle_alarm(data);
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  trueImpl->recv(port, packet, size);
}