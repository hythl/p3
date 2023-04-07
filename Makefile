CC = g++
COPTS = -g -Wall -std=c++11
LKOPTS = 

OBJS =\
	Event.o\
	Link.o\
	Node.o\
	RoutingProtocolImpl.o\
	Simulator.o\
	TblEntry.o\
	Impl.o\
	DistanceVector.o\
	LinkState.o

HEADRES =\
	global.h\
	Event.h\
	Link.h\
	Node.h\
	RoutingProtocol.h\
	Simulator.h\
	Impl.h\
	TblEntry.h\
	Packet.h\
	lsdb.h

%.o: %.cc
	$(CC) $(COPTS) -c $< -o $@

all:	Simulator

Simulator: $(OBJS)
	$(CC) $(LKOPTS) -o Simulator $(OBJS)

$(OBJS): global.h
Event.o: Event.h Link.h Node.h Simulator.h 
Link.o: Event.h Link.h Node.h Simulator.h 
Node.o: Event.h Link.h Node.h Simulator.h 
Simulator.o: Event.h Link.h Node.h RoutingProtocol.h Simulator.h
Impl.o: Impl.h Packet.h
DistanceVector.o: Impl.h TblEntry.h
LinkState.o: Impl.h TblEntry.h lsdb.h
RoutingProtocolImpl.o: RoutingProtocolImpl.h Impl.h

clean:
	rm -f *.o Simulator
	rm -f core*

