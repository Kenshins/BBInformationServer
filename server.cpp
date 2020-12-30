#include <cstdlib>
#include <iostream>
#include <deque>
#include <set>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>            
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp> 

namespace po = boost::program_options;
namespace posix = boost::asio::posix;

using boost::asio::ip::tcp;

static int session_counter = 0;

class session_interface
{
public:
  virtual ~session_interface() {}
  virtual void deliver(char* deliver_data) = 0;
};

class distributor
{
        public:
                void subscribe(boost::shared_ptr<session_interface> session)
				{
					std::cout << "Session added to distributor" << std::endl;	
					session_.insert(session);
                }

                void unsubscribe(boost::shared_ptr<session_interface> session)
                {
					std::cout << "Session deleted from distributor" << std::endl;
					session_.erase(session);
                }

                void distribute(char* data)
                {
					std::cout << "Distribute!" << std::endl;
					std::for_each(session_.begin(), session_.end(),
        				boost::bind(&session_interface::deliver, _1, boost::ref(data)));
                }
			
        private:
			std::set<boost::shared_ptr<session_interface>> session_;
            std::deque<int> messages;
};

class session : public session_interface, public boost::enable_shared_from_this<session>
{
	public:
		session(boost::asio::io_service& io_service, distributor& dist_service, boost::uuids::uuid uuid)
			: socket_(io_service), distributor_(dist_service), uuid_(uuid)
		{
		}

		tcp::socket& socket()
		{
			return socket_;
		}

		void start()
		{
			std::cout << "Session "  << uuid_ << std::endl;
			distributor_.subscribe(shared_from_this());
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
					boost::bind(&session::handle_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			session_counter++;
		}
		
		 void deliver(char* deliver_data)
		 {
		 std::cout << "Deliver " << uuid_ << std::endl;
		 	 size_t bytes_transferred = 0;
			boost::asio::async_write(socket_,
                                                boost::asio::buffer(deliver_data, max_length),
                                                boost::bind(&session::deliver_done, shared_from_this(),
												boost::asio::placeholders::error));
		}

	private:
		void handle_read(const boost::system::error_code& error,
				size_t bytes_transferred)
		{
			if(!error)
			{
			std::cout << "Handle_read " << uuid_ << "bytes transfered " << bytes_transferred <<std::endl;
			distributor_.distribute(data_);
			memset(data_, 0, sizeof(data_));
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
									boost::bind(&session::handle_read, shared_from_this(),
												boost::asio::placeholders::error,
												boost::asio::placeholders::bytes_transferred));
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
				std::cout << "Deliver done! " << uuid_ << std::endl;
			}
			else
			{
				distributor_.unsubscribe(shared_from_this());
			}
		}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length] = {0};
	distributor& distributor_;
	boost::uuids::uuid uuid_;
};

class tcp_server
{
	public:
		tcp_server(boost::asio::io_service& io_service, int port) : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),input_(io_service, ::dup(STDIN_FILENO)), input_buffer_(100)
		{
			start_accept();
			std::cout << "Start accept! Ipv4 on port 13" << std::endl;
			start_read_keyboard();
		}
	private:
	void start_accept()
	{
	boost::shared_ptr<session> new_session( new session(io_service_, distributor_, boost::uuids::random_generator()()));
	acceptor_.async_accept(new_session->socket(),
			boost::bind(&tcp_server::handle_accept, this, new_session,
				boost::asio::placeholders::error));
	}

	void start_read_keyboard()
	{
			boost::asio::async_read_until(input_, input_buffer_, '\n',
										  boost::bind(&tcp_server::handle_keyboard_input, this,
													  boost::asio::placeholders::error,
													  boost::asio::placeholders::bytes_transferred));
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

	void handle_keyboard_input(const boost::system::error_code& error,
				size_t bytes_transferred)
	{
		if (!error)
		{
		boost::asio::streambuf::const_buffers_type bufs = input_buffer_.data();
		std::string str(boost::asio::buffers_begin(bufs),
                boost::asio::buffers_begin(bufs) + bytes_transferred);
		std::cout << "Keyboard input: " << str;
		// This does nothing now. For future CLI commands to server.
		input_buffer_.consume(bytes_transferred);
		start_read_keyboard();
		}
		else
		{
			std::cout << "Keyboard input error, closing server" << std::endl;
		}
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	distributor distributor_;
	posix::stream_descriptor input_;
  	boost::asio::streambuf input_buffer_;
};

int main(int argument_count, char* argument_vector[])
{
	try
	{
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

		std::cout << "Starting server!" << std::endl;
		//boost::thread_group worker_threads;
		boost::asio::io_service io_service;
		//boost::asio::io_context io_context; -- should be used in the future
		boost::shared_ptr<tcp_server> server(new tcp_server(io_service, map_of_arguments["port"].as<int>()));
		io_service.run(); // if not running thread pool

		//for (unsigned int i = 0; i < 3; ++i)
		//{
		//	worker_threads.create_thread(
		//		[&]() {
		//			io_service.run();
		//		});
		//}

		//worker_threads.join_all();
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
