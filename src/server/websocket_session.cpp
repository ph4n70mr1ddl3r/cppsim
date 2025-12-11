#include "websocket_session.hpp"

#include <iostream>

#include "connection_manager.hpp"

namespace cppsim {
namespace server {

websocket_session::websocket_session(
    boost::asio::ip::tcp::socket socket,
    std::shared_ptr<connection_manager> mgr)
    : ws_(std::move(socket)), conn_mgr_(mgr) {}

void websocket_session::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(
      boost::beast::websocket::stream_base::timeout::suggested(
          boost::beast::role_type::server));

  // Accept the websocket handshake
  do_accept();
}

void websocket_session::do_accept() {
  ws_.async_accept(boost::beast::bind_front_handler(
      &websocket_session::on_accept, shared_from_this()));
}

void websocket_session::on_accept(boost::beast::error_code ec) {
  if (ec) {
    std::cerr << "[WebSocketSession] Accept failed: " << ec.message()
              << std::endl;
    return;
  }

  std::cout << "[WebSocketSession] Connection accepted for session: "
            << session_id_ << std::endl;

  // Start reading messages
  do_read();
}

void websocket_session::do_read() {
  // Read a message into our buffer
  ws_.async_read(buffer_, boost::beast::bind_front_handler(
                              &websocket_session::on_read, shared_from_this()));
}

void websocket_session::on_read(boost::beast::error_code ec,
                                 std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  // Handle disconnect
  if (ec == boost::beast::websocket::error::closed) {
    std::cout << "[WebSocketSession] Client disconnected: " << session_id_
              << std::endl;
    if (conn_mgr_) {
      conn_mgr_->unregister_session(session_id_);
    }
    return;
  }

  if (ec) {
    std::cerr << "[WebSocketSession] Read error for " << session_id_ << ": "
              << ec.message() << std::endl;
    if (conn_mgr_) {
      conn_mgr_->unregister_session(session_id_);
    }
    return;
  }

  // Log received message (will forward to message handler in future stories)
  std::string message = boost::beast::buffers_to_string(buffer_.data());
  std::cout << "[WebSocketSession] Received from " << session_id_ << ": "
            << message << std::endl;

  // Clear the buffer
  buffer_.consume(buffer_.size());

  // Continue reading
  do_read();
}

void websocket_session::send(const std::string& message) {
  // Post to the websocket's executor to ensure thread safety
  boost::asio::post(ws_.get_executor(),
                    [self = shared_from_this(), message]() {
                      self->write_queue_.push(message);
                      if (!self->writing_) {
                        self->do_write();
                      }
                    });
}

void websocket_session::do_write() {
  if (write_queue_.empty()) {
    writing_ = false;
    return;
  }

  writing_ = true;
  const std::string& message = write_queue_.front();

  // Write the message
  ws_.async_write(boost::asio::buffer(message),
                  boost::beast::bind_front_handler(&websocket_session::on_write,
                                                    shared_from_this()));
}

void websocket_session::on_write(boost::beast::error_code ec,
                                  std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    std::cerr << "[WebSocketSession] Write error for " << session_id_ << ": "
              << ec.message() << std::endl;
    writing_ = false;
    return;
  }

  // Remove the sent message from queue
  write_queue_.pop();

  // Continue writing if more messages queued
  do_write();
}

void websocket_session::close() {
  // Close the websocket gracefully
  ws_.async_close(boost::beast::websocket::close_code::normal,
                  [self = shared_from_this()](boost::beast::error_code ec) {
                    if (ec) {
                      std::cerr << "[WebSocketSession] Close error: "
                                << ec.message() << std::endl;
                    }
                  });
}

}  // namespace server
}  // namespace cppsim
