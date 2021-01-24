//
// message.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2020 Martin Kleberger
//
//

#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

class message
{
public:
  enum { header_length = 4 };
  enum { max_body_length = 512 };

  message()
    : body_length_(0)
  {
  }

  const char* data() const
  {
    return data_;
  }

  char* data()
  {
    return data_;
  }

  size_t length() const
  {
    return header_length + body_length_;
  }

  const char* body() const
  {
    return data_ + header_length;
  }

  char* body()
  {
    return data_ + header_length;
  }

  size_t body_length() const
  {
    return body_length_;
  }

  void body_length(size_t new_length)
  {
    body_length_ = new_length;
    if (body_length_ > max_body_length)
      body_length_ = max_body_length;
  }

  bool decode_header()
  {
    char header[header_length + 1] = "";
    memcpy(header, data_, header_length);
    body_length_ = size_t((unsigned char)(header[0]) << 24 |
          (unsigned char)(header[1]) << 16 |
          (unsigned char)(header[2]) << 8 |
          (unsigned char)(header[3]));
    if (body_length_ > max_body_length)
    {
      body_length_ = 0;
      return false;
    }
    return true;
  }

  void encode_header()
  {
    char header[header_length + 1] = "";
    header[0] = (body_length_ >> 24) & 0xFF;
    header[1] = (body_length_ >> 16) & 0xFF;
    header[2] = (body_length_ >> 8) & 0xFF;
    header[3] = body_length_ & 0xFF;

    std::memcpy(data_, header, header_length);
  }

  void reset()
  {
    memset(data_, 0, sizeof(data_));
  }

private:
  char data_[header_length + max_body_length];
  size_t body_length_;
};

#endif // MESSAGE_HPP