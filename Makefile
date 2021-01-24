CPP = g++
CFLAGS = -lboost_thread -lboost_system -lboost_program_options -lprotobuf -pthread

BBInformationServer : server.cpp
	$(CPP) -Wall -pedantic -g -o BBInformationServer server.cpp chat_message.pb.cc $(CFLAGS)

clean:
	rm server.o
