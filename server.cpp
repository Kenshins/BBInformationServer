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

#include <azmq/socket.hpp>

#include "chat_message.pb.h"
#include "message.hpp"
#include "message_queue.hpp"

namespace po = boost::program_options;
namespace posix = boost::asio::posix;
namespace keywords = boost::log::keywords;
namespace logging = boost::log;

using boost::asio::ip::tcp;


class session_interface
{
public:
  virtual ~session_interface() {}
  virtual void deliver_msg(std::shared_ptr<message> message) = 0;
  
  virtual boost::uuids::uuid print_uuid() = 0;
  virtual void stop() = 0;
};

class distributor : public boost::enable_shared_from_this<distributor>
{
        public:

				distributor(boost::asio::io_service& io_service) : subscriber_(io_service), io_service_(io_service)
				{
					start();
				}

				void start()
				{
					subscriber_.connect("tcp://localhost:5555");
					//subscriber_.connect("tcp://192.168.55.112:5556");
					//subscriber_.connect("tcp://192.168.55.201:7721");
					subscriber_.set_option(azmq::socket::subscribe("hello"));

					//auto size = subscriber_.receive(boost::asio::buffer(sub_buf_));

					//BOOST_LOG_TRIVIAL(info) << "size:" << size << std::endl;

					subscriber_.async_receive(boost::asio::buffer(data_, 1024), boost::bind(&distributor::subscriber_read, this,
																						   boost::asio::placeholders::error));
					//azmq::pub_socket publisher(io_service_);
					//publisher.bind("ipc://nasdaq-feed");
				}

				void subscribe(boost::shared_ptr<session_interface> session)
				{
					std::lock_guard<std::mutex> l(mutex_);
					session_.insert(session);
                }

                void unsubscribe(boost::shared_ptr<session_interface> session)
                {
					std::lock_guard<std::mutex> l(mutex_);
					session_.erase(session);
					session->stop();
                }

				void distribute(std::shared_ptr<message> m)
				{
					std::lock_guard<std::mutex> l(mutex_);
					std::for_each(session_.begin(), session_.end(),
									boost::bind(&session_interface::deliver_msg, _1, m));
				}
			
        private:

			void subscriber_read(const boost::system::error_code &error)
			{
				if (!error)
				{
					BOOST_LOG_TRIVIAL(info) << "subscriber_read" << std::endl;
					subscriber_.async_receive(boost::asio::buffer(data_, 1024), boost::bind(&distributor::subscriber_read, this,
																						   boost::asio::placeholders::error));
				}
				else
				{
					BOOST_LOG_TRIVIAL(info) << "subscriber_read" << std::endl;
				}
			}

			std::array<char, 256> sub_buf_;
			azmq::sub_socket subscriber_;
			char data_[1024] = {0};
			boost::asio::io_service& io_service_;
		    mutable std::mutex mutex_;
			std::set<boost::shared_ptr<session_interface>> session_;
};

class session : public session_interface, public boost::enable_shared_from_this<session>
{
	public:
		session(boost::asio::io_service& io_service, distributor& dist_service, boost::uuids::uuid uuid)
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
			distributor_.subscribe(shared_from_this());
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
				distributor_.unsubscribe(shared_from_this());
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
				distributor_.distribute(shared_message);
				read_msg_.reset();
				boost::asio::async_read(socket_,
										boost::asio::buffer(read_msg_.data(), message::header_length),
										strand_.wrap(boost::bind(&session::handle_read_header, shared_from_this(),
													boost::asio::placeholders::error)));
			}
			else
			{

				distributor_.unsubscribe(shared_from_this());
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
					distributor_.unsubscribe(shared_from_this());
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
	distributor& distributor_;
	boost::uuids::uuid uuid_;
};

class tcp_server
{
	public:
		tcp_server(boost::asio::io_service& io_service, int port) : strand_(io_service), io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), distributor_(io_service)
		{
			start_accept();
			BOOST_LOG_TRIVIAL(info) << "Start accept! Ipv4 on port: " << port << std::endl;
		}
	private:
	void start_accept()
	{
	boost::shared_ptr<session> new_session(new session(io_service_, distributor_, boost::uuids::random_generator()()));
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
	distributor distributor_;
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
