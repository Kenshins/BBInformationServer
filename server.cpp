#include <cstdlib>
#include <iostream>
#include <deque>
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

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
					std::cout << "session added to distributor" << std::endl;	
					session_.insert(session);
                }

                void unsubscribe(boost::shared_ptr<session_interface> session)
                {
					std::cout << "session deleted from distibutor" << std::endl;
					session_.erase(session);
                }

                void distribute(char* data)
                {
					std::cout << "DISTRIBUTE!" << std::endl;
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
		session(boost::asio::io_service& io_service, distributor& dist_service)
			: socket_(io_service), distributor_(dist_service)
		{
		}

		tcp::socket& socket()
		{
			return socket_;
		}

		void start()
		{
			std::cout << "SESSION!" << std::endl;
			distributor_.subscribe(shared_from_this());
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
					boost::bind(&session::handle_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			session_counter++;
		}
		
		 void deliver(char* deliver_data)
		 {
		 std::cout << "DELIVER!" << std::endl;
		 	 size_t bytes_transferred = 0;
			boost::asio::async_write(socket_,
                                                boost::asio::buffer(deliver_data, max_length),
                                                boost::bind(&session::deliver_done, shared_from_this()));
		}

		void deliver_done()
		{
			memset(data_, 0, sizeof(data_));
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
					boost::bind(&session::handle_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
		}

	private:
		void handle_read(const boost::system::error_code& error,
				size_t bytes_transferred)
		{
			std::cout << "handle_read" << std::endl;
			distributor_.distribute(data_);
			if (error)
				distributor_.unsubscribe(shared_from_this());
		}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
	distributor& distributor_;
};

class tcp_server
{
	public:
		tcp_server(boost::asio::io_service& io_service) : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), 13))
		{
			start_accept();
			std::cout << "START ACCEPT! Ipv4 on port 13" << std::endl;
		}
	private:
	void start_accept()
	{
	boost::shared_ptr<session> new_session( new session(io_service_, distributor_));
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

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	distributor distributor_;
};

int main()
{
	try
	{
		std::cout << "Starting server!" << std::endl;
		boost::asio::io_service io_service;
		boost::shared_ptr<tcp_server> server(new tcp_server(io_service));
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
return 0;
}
