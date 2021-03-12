
//
// session_interface.h
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2020 Martin Kleberger
//
//

#ifndef SESSION_INTERFACE_H
#define SESSION_INTERFACE_H

#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

class message;

class session_interface
{
public:
  virtual ~session_interface() {}
  virtual void deliver_msg(std::shared_ptr<message> message) = 0;
  
  virtual boost::uuids::uuid print_uuid() = 0;
  virtual void stop() = 0;
};

#endif // SESSION_INTERFACE_H
