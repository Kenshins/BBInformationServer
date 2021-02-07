CPP = g++
CFLAGS = -DBOOST_LOG_DYN_LINK -lzmq -lboost_regex -lboost_thread -lboost_system -lboost_program_options -lboost_log -lboost_log_setup -lprotobuf -pthread

BBInformationServer : server.cpp
	$(CPP) -std=c++11 -Wall -pedantic -g -o BBInformationServer server.cpp chat_message.pb.cc $(CFLAGS)

clean:
	rm server.o
