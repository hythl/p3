CC = g++
COPTS = -g -Wall -std=c++11
LKOPTS = 

OBJS =\
	Event.o\
	Link.o\
	Node.o\
	RoutingProtocolImpl.o\
	Simulator.o\
	TblEntry.o

HEADRES =\
	global.h\
	Event.h\
	Link.h\
	Node.h\
	RoutingProtocol.h\
	Simulator.h\
	TblEntry.h

%.o: %.cc
	$(CC) $(COPTS) -c $< -o $@

all:	Simulator

Simulator: $(OBJS)
	$(CC) $(LKOPTS) -o Simulator $(OBJS)

$(OBJS): global.h
Event.o: Event.h Link.h Node.h Simulator.h TblEntry.h
Link.o: Event.h Link.h Node.h Simulator.h TblEntry.h
Node.o: Event.h Link.h Node.h Simulator.h TblEntry.h
Simulator.o: Event.h Link.h Node.h RoutingProtocol.h Simulator.h TblEntry.h 
RoutingProtocolImpl.o: RoutingProtocolImpl.h TblEntry.h
DvEntry.o: TblEntry.h

clean:
	rm -f *.o Simulator

