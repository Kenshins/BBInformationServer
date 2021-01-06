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

#include "message.hpp"

class message_queue {
    mutable std::mutex mutex_;
    std::deque<std::shared_ptr<message>> message_deque_;

public:

    void push_back(std::shared_ptr<message> msg)
    {
        std::lock_guard<std::mutex> l(mutex_);
        message_deque_.push_back(msg);
    }

    void pop_front()
    {
        std::lock_guard<std::mutex> l(mutex_);
        message_deque_.pop_front();
    }

    char * get_front_data()
    {
        std::lock_guard<std::mutex> l(mutex_);
        return message_deque_.front()->data();
    }

};

#endif // MESSAGE_QUEUE_HPP