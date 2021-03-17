//
// distributor.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2020 Martin Kleberger
//
//

#ifndef DISTRIBUTOR_HPP
#define DISTRIBUTOR_HPP

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/asio.hpp>

#include <azmq/socket.hpp>

#include "session_interface.h"

class distributor : public boost::enable_shared_from_this<distributor>
{
        public:

				distributor(boost::asio::io_service& io_service) : subscriber_(io_service), io_service_(io_service), strand_(io_service)
				{
					BOOST_LOG_TRIVIAL(info) << "Distributor started!";
				}

				void start()
				{
					BOOST_LOG_TRIVIAL(debug) << "Connect to ZeroMQ Publisher";
					subscriber_.connect("tcp://localhost:5555");
					subscriber_.set_option(azmq::socket::subscribe(""));
					subscriber_.async_receive(boost::asio::buffer(read_msg_.data(), message::header_length), strand_.wrap(boost::bind(&distributor::subscriber_read_header, shared_from_this(),
																						   boost::asio::placeholders::error)));
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

			void subscriber_read_header(const boost::system::error_code &error)
			{
				if (!error)
				{
					BOOST_LOG_TRIVIAL(debug) << "subscriber_read_header";
						read_msg_.decode_header();
						subscriber_.async_receive(boost::asio::buffer(read_msg_.body(), read_msg_.body_length()), strand_.wrap(boost::bind(&distributor::subscriber_read_body, shared_from_this(),
																											 boost::asio::placeholders::error)));
				}
				else
				{
					BOOST_LOG_TRIVIAL(info) << "subscriber_read_header error";
				}
			}

			void subscriber_read_body(const boost::system::error_code &error)
			{
				if (!error)
				{
					//BOOST_LOG_TRIVIAL(info) << "subscriber_read_body";

					//std::string s = read_msg_.body();
					//read_chat_message_.ParseFromString(s);
					//std::cout << read_chat_message_.message_content() << std::endl;
					std::shared_ptr<message> shared_message = std::make_shared<message>(std::move(read_msg_));
					distribute(shared_message);
					subscriber_.async_receive(boost::asio::buffer(read_msg_.data(), message::header_length), strand_.wrap(boost::bind(&distributor::subscriber_read_header, shared_from_this(),
																												   boost::asio::placeholders::error)));
																												
				}
				else
				{
					BOOST_LOG_TRIVIAL(info) << "subscriber_read_body";
				}
			}

			//chat_message read_chat_message_;
			std::array<char, 256> sub_buf_;
			azmq::sub_socket subscriber_;
			message read_msg_;
			boost::asio::io_service& io_service_;
		    mutable std::mutex mutex_;
			std::set<boost::shared_ptr<session_interface>> session_;
			boost::asio::strand strand_;
};

#endif // DISTRIBUTOR_HPP