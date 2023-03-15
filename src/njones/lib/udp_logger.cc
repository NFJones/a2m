#include "udp_logger.h"

namespace njones {
namespace audio {
UDPLogger::UDPLogger(const std::string& host, const int port)
    : host(host), port(port), socket(io_service), remote_endpoint(boost::asio::ip::address::from_string(host), port) {
    using namespace boost::asio;

    boost::system::error_code err;
    socket.open(ip::udp::v4(), err);
}

UDPLogger::~UDPLogger() {
    boost::system::error_code err;
    socket.close(err);
}

void UDPLogger::log(const std::string& msg) {
    using namespace boost::asio;

    boost::system::error_code err;
    socket.send_to(buffer(msg, msg.length()), remote_endpoint, 0, err);
}
}  // namespace audio
}  // namespace njones
