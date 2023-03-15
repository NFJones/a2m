#pragma once

#include <boost/asio.hpp>
#include <string>

namespace njones {
namespace audio {
class UDPLogger {
   public:
    UDPLogger(const std::string& host, const int port);
    ~UDPLogger();

    void log(const std::string& msg);

   private:
    UDPLogger() = delete;
    UDPLogger(const UDPLogger&) = delete;
    UDPLogger(UDPLogger&&) = delete;

    std::string host;
    int port;
    boost::asio::io_service io_service;
    boost::asio::ip::udp::socket socket;
    boost::asio::ip::udp::endpoint remote_endpoint;
};
}  // namespace audio
}  // namespace njones