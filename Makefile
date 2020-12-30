CPP = g++
CFLAGS = -lboost_thread -lboost_system -lboost_program_options

BBInformationServer : server.o
	$(CPP) -o BBInformationServer server.o $(CFLAGS)

server.o : server.cpp
	$(CPP) -c server.cpp

clean:
	rm server.o
