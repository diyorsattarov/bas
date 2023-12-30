#include "../include/websocket_session.hpp"

// Constructor
websocket_session::websocket_session(tcp::socket&& socket, boost::shared_ptr<shared_state> const& state)
    : ws_(std::move(socket)), state_(state) {
}

// Destructor
websocket_session::~websocket_session() {
    state_->leave(this);
}

// Handle failure
void websocket_session::fail(beast::error_code ec, char const* what) {
    if (ec == net::error::operation_aborted || ec == websocket::error::closed)
        return;
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handle accept
void websocket_session::on_accept(beast::error_code ec) {
    if (ec)
        return fail(ec, "accept");
    state_->join(this);
    ws_.async_read(buffer_, beast::bind_front_handler(&websocket_session::on_read, shared_from_this()));
}

// Read operation
void websocket_session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    std::cout << "Read operation started. Bytes transferred: " << bytes_transferred << std::endl;
    if (ec) {
        std::cerr << "Read error: " << ec.message() << std::endl;
        return fail(ec, "read");
    }
    std::string received_msg = beast::buffers_to_string(buffer_.data());
    std::cout << "Received message: " << received_msg << std::endl;
    try {
        nlohmann::json json_msg = nlohmann::json::parse(received_msg);
        if (json_msg.contains("method")) {
            std::string method = json_msg["method"];
            if (method == "api") {
                handle_api(json_msg);
                // want to implement something much more modular...
            } else {
                std::string error_msg = "ERROR: Invalid method";
                state_->send(error_msg);
                std::cout << "Sent error response for invalid method." << std::endl;
            }
        } else {
            std::string error_msg = "ERROR: Missing 'method' parameter";
            state_->send(error_msg);
            std::cout << "Sent error response for missing 'method' parameter." << std::endl;
        }
    } catch (const nlohmann::json::exception& ex) {
        std::cerr << "JSON parsing error: " << ex.what() << std::endl;
        std::string error_msg = "ERROR: Invalid JSON format";
        state_->send(error_msg);
        std::cout << "Sent error response for invalid JSON format." << std::endl;
    }
    buffer_.consume(buffer_.size());
    ws_.async_read(buffer_, beast::bind_front_handler(&websocket_session::on_read, shared_from_this()));
    std::cout << "Read operation completed." << std::endl;
}

// Send operation
void websocket_session::send(boost::shared_ptr<std::string const> const& ss) { net::post(ws_.get_executor(), beast::bind_front_handler(&websocket_session::on_send, shared_from_this(), ss)); }

// On send
void websocket_session::on_send(boost::shared_ptr<std::string const> const& ss) {
    queue_.push_back(ss);
    if (queue_.size() > 1)
        return;
    ws_.async_write(net::buffer(*queue_.front()), beast::bind_front_handler(&websocket_session::on_write, shared_from_this()));
}

// On write
void websocket_session::on_write(beast::error_code ec, std::size_t) {
    if (ec)
        return fail(ec, "write");
    queue_.erase(queue_.begin());
    if (!queue_.empty())
        ws_.async_write(net::buffer(*queue_.front()), beast::bind_front_handler(&websocket_session::on_write, shared_from_this()));
}

void websocket_session::handle_api(const nlohmann::json& json_msg) {
}
