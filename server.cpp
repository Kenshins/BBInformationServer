//
// server.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2020 Martin Kleberger
//
//

#ifndef SERVER_CPP
#define SERVER_CPP

//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#define GOOGLE_PROTOBUF_VERIFY_VERSION

#include <cstdlib>
#include <iostream>
#include <deque>
#include <set>
#include <iomanip>
#include <atomic> 
#include <mutex>

#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>            
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>

#include "chat_message.pb.h"
#include "message.hpp"
#include "message_queue.hpp"
#include "session_interface.h"
#include "distributor.hpp"

namespace po = boost::program_options;
namespace posix = boost::asio::posix;
namespace keywords = boost::log::keywords;
namespace logging = boost::log;

using boost::asio::ip::tcp;

class session : public session_interface, public boost::enable_shared_from_this<session>
{
	public:
		session(boost::asio::io_service& io_service, boost::shared_ptr<distributor> dist_service, boost::uuids::uuid uuid)
			: strand_(io_service), socket_(io_service), distributor_(dist_service), uuid_(uuid)
		{
		}

		tcp::socket& socket()
		{
			return socket_;
		}

		void stop()
		{
  			socket_.close();
		}

		void start()
		{
			BOOST_LOG_TRIVIAL(info) << "Session starting uuid: "  << uuid_ << std::endl;
			distributor_->subscribe(shared_from_this());
			boost::asio::async_read(socket_,boost::asio::buffer(read_msg_.data(), message::header_length),
					strand_.wrap(boost::bind(&session::handle_read_header, shared_from_this(),
						boost::asio::placeholders::error)));
		}

		void deliver_msg(std::shared_ptr<message> deliver_data)
		 {
			boost::asio::async_write(socket_,
                                                boost::asio::buffer(deliver_data->data(), deliver_data->length()),
                                                strand_.wrap(boost::bind(&session::deliver_done, shared_from_this(),
												boost::asio::placeholders::error)));
		}

	private:

		void handle_read_header(const boost::system::error_code &error)
		{
			if (!error)
			{
				read_msg_.decode_header();
				boost::asio::async_read(socket_,
										boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
										strand_.wrap(boost::bind(&session::handle_read_body, shared_from_this(),
													boost::asio::placeholders::error)));
			}
			else
			{
				distributor_->unsubscribe(shared_from_this());
			}
		}

		void handle_read_body(const boost::system::error_code &error)
		{
			if (!error)
			{
				//std::string s = read_msg_.body();
				//read_chat_message_.ParseFromString(s);
				//std::cout << read_chat_message_.message_content() << std::endl;
				std::shared_ptr<message> shared_message = std::make_shared<message>(std::move(read_msg_));	
				//out_messages_.push_back(shared_message);

				//This will most likely be a time series request in the future

				distributor_->distribute(shared_message);
				//read_msg_.reset();
				boost::asio::async_read(socket_,
										boost::asio::buffer(read_msg_.data(), message::header_length),
										strand_.wrap(boost::bind(&session::handle_read_header, shared_from_this(),
													boost::asio::placeholders::error)));
			}
			else
			{

				distributor_->unsubscribe(shared_from_this());
			}
		}

		void deliver_done(const boost::system::error_code& error)
		{
			if(!error)
			{
				//std::cout << "Deliver done! " << uuid_ << std::endl;
			}
			else
			{
					distributor_->unsubscribe(shared_from_this());
					//std::cout << "Socket cancel unsubscribe! " << this << std::endl;
			}
		}
		boost::uuids::uuid print_uuid()
		{
			return uuid_;
		}

	boost::asio::strand strand_;
	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length] = {0};
	message read_msg_;
	boost::shared_ptr<distributor> distributor_;
	boost::uuids::uuid uuid_;
};

class tcp_server
{
	public:
		tcp_server(boost::asio::io_service& io_service, int port) : strand_(io_service), io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), distributor_(boost::make_shared<distributor>(io_service_))
		{
			BOOST_LOG_TRIVIAL(info) << "Server starting up!" << std::endl;
			distributor_->start();
			start_accept();
			BOOST_LOG_TRIVIAL(info) << "Start accept! Ipv4 on port: " << port << std::endl;
		}
	private:
	void start_accept()
	{
	BOOST_LOG_TRIVIAL(info) << "Create session for accept " << std::endl;
	boost::shared_ptr<session> new_session(new session(io_service_, distributor_, boost::uuids::random_generator()()));
	BOOST_LOG_TRIVIAL(info) << "Created session for accept " << std::endl;
	acceptor_.async_accept(new_session->socket(),
			boost::bind(&tcp_server::handle_accept, this, new_session,
				boost::asio::placeholders::error));
	}

	void handle_accept(boost::shared_ptr<session> new_session,
			const boost::system::error_code& error)
	{
		if (!error)
		{
			new_session->start();
		}
	start_accept();
	}

	boost::asio::strand strand_;
	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	boost::shared_ptr<distributor> distributor_;
  	boost::asio::streambuf input_buffer_;
};

int main(int argument_count, char* argument_vector[])
{
	try
	{
		//boost::log::add_file_log(keywords::file_name = "server.log");

		logging::core::get()->set_filter
    	(
        	logging::trivial::severity >= logging::trivial::info
    	);

		po::options_description description("Usage: BBInformationServer [Options] --port arg \nAllowed options");
		description.add_options()("help,h", "Produce help message")("port,p", po::value<int>(), "Set server port");

		po::variables_map map_of_arguments;
		po::store(po::parse_command_line(argument_count, argument_vector, description), map_of_arguments);
		po::notify(map_of_arguments);

		if (map_of_arguments.count("help"))
		{
			std::cout << description << "\n";
			return 0;
		}

		if (map_of_arguments.count("port"))
		{
			std::cout << "Port was set to: "
				 << map_of_arguments["port"].as<int>() << ".\n";
		}
		else
		{
			std::cout << "Port was not set. Example: BBInformationServer --port 13\n";
			return 0;
		}

		BOOST_LOG_TRIVIAL(info) << "Starting server!" << std::endl;
		boost::thread_group worker_threads;
		boost::asio::io_service io_service;
		boost::shared_ptr<tcp_server> server(new tcp_server(io_service, map_of_arguments["port"].as<int>()));
		//io_service.run(); // if not running thread pool

		for (unsigned int i = 0; i < 4; ++i)
		{
			worker_threads.create_thread(
				[&]() {
					io_service.run();
				});
		}

		worker_threads.join_all();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	catch(...) {
        std::cerr << "Exception of unknown type!\n";
    }
return 0;
}

#endif // SERVER_CPP
