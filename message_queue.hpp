//
// message_queue.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2020 Martin Kleberger
//
//

#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include <deque>
#include <mutex>
#include <memory>
#include <iostream>

#include "message.hpp"
#include "chat_message.pb.h"

class message_queue {
    mutable std::mutex mutex_;
    std::deque<message> message_deque_;

public:

    void push_back(message msg)
    {
        std::lock_guard<std::mutex> l(mutex_);
        message_deque_.push_back(msg);
    }

    void pop_front()
    {
        std::lock_guard<std::mutex> l(mutex_);
        message_deque_.pop_front();
    }

    message get_front_data()
    {
        std::lock_guard<std::mutex> l(mutex_);
        return message_deque_.front();
    }

    size_t get_size()
    {
        return message_deque_.size();
    }

    bool get_empty()
    {
        return message_deque_.empty();
    }

    void print_length_of_items()
    {
        for (message m : message_deque_) { std::cout << m.length() <<  " "; }
    }

    void print_content()
    {
        for (message m : message_deque_) 
        { 
            std::string s = m.body();
            chat_message c;
			c.ParseFromString(s);
			std::cout << c.message_content() << std::endl; 
        }
    }
};

#endif // MESSAGE_QUEUE_HPP