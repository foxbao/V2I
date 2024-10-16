SRCDIR	=	.
LINK_PATH = /home/coder/mec/targets/x86-dbg
INCS	= -I../../inc -I../../vehicle-indexing/inc -I./ -I/home/coder/zassys/x86/include/zmq

LIBS	= -lzmq -lwebcore
LIBINC	= -L/home/coder/zassys/x86/lib -L$(LINK_PATH)

.PHONY: all
all :
	g++ $(SRCDIR)/vehicle_client/main.cpp $(SRCDIR)/zmsg.cpp $(INCS) $(LIBS) $(LIBINC) -o $(LINK_PATH)/test/vehicle_client
	g++ $(SRCDIR)/vehicle_snapshot/main.cpp $(SRCDIR)/zmsg.cpp $(INCS) $(LIBS) $(LIBINC) -o $(LINK_PATH)/test/vehicle_snapshot

.PHONY: client
client :
	g++ $(SRCDIR)/vehicle_client/main.cpp $(SRCDIR)/zmsg.cpp $(INCS) $(LIBS) $(LIBINC) -o $(LINK_PATH)/test/vehicle_client

.PHONY: service
service :
	g++ $(SRCDIR)/vehicle_snapshot/main.cpp $(SRCDIR)/zmsg.cpp $(INCS) $(LIBS) $(LIBINC) -o $(LINK_PATH)/test/vehicle_snapshot