#include "websocket_session.hpp"

#include <iostream>

#include "connection_manager.hpp"
#include "protocol.hpp"

namespace cppsim {
namespace server {

websocket_session::websocket_session(
    boost::asio::ip::tcp::socket socket,
    std::shared_ptr<connection_manager> mgr)
    : ws_(std::move(socket)), conn_mgr_(mgr), deadline_(ws_.get_executor()) {
      deadline_.expires_at(boost::asio::steady_timer::time_point::max());
    }

void websocket_session::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(
      boost::beast::websocket::stream_base::timeout::suggested(
          boost::beast::role_type::server));

  // Start the deadline timer for authentication
  deadline_.expires_after(std::chrono::seconds(10));
  check_deadline();

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

  std::cout << "[WebSocketSession] Connection accepted. Waiting for handshake..." << std::endl;

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

  std::string message = boost::beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());

  if (state_ == state::unauthenticated) {
    try {
      auto json = nlohmann::json::parse(message);
      protocol::message_envelope envelope = json.get<protocol::message_envelope>();

      if (envelope.message_type != "HANDSHAKE") {
         std::cerr << "[WebSocketSession] Handshake error: Protocol error (Not HANDSHAKE)" << std::endl;
         // Send Error
         protocol::error_message err;
         err.error_code = "PROTOCOL_ERROR";
         err.message = "Expected HANDSHAKE message";
         
         protocol::message_envelope err_env;
         err_env.message_type = "ERROR";
         err_env.protocol_version = protocol::PROTOCOL_VERSION;
         nlohmann::json err_payload;
         protocol::to_json(err_payload, err);
         err_env.payload = err_payload;
         
         nlohmann::json j;
         protocol::to_json(j, err_env);
         
         send(j.dump()); 
         close();
         return;
      }

      // Check Protocol Version
      if (envelope.protocol_version != protocol::PROTOCOL_VERSION) {
         std::cerr << "[WebSocketSession] Handshake error: Incompatible version " << envelope.protocol_version << std::endl;
         protocol::error_message err;
         err.error_code = "INCOMPATIBLE_VERSION";
         err.message = "Expected " + std::string(protocol::PROTOCOL_VERSION);
         
         protocol::message_envelope err_env;
         err_env.message_type = "ERROR";
         err_env.protocol_version = protocol::PROTOCOL_VERSION;
         nlohmann::json err_payload;
         protocol::to_json(err_payload, err);
         err_env.payload = err_payload;
         
         nlohmann::json j;
         protocol::to_json(j, err_env);
         
         send(j.dump());
         close();
         return;
      }

      // Valid Handshake
      state_ = state::authenticated;
      deadline_.cancel(); // Cancel timeout

      std::cout << "[WebSocketSession] Handshake successful for session: " << session_id_ << std::endl;

      protocol::handshake_response resp;
      resp.session_id = session_id_;
      resp.seat_number = -1; // Placeholder
      resp.starting_stack = 0.0; // Placeholder
      
      protocol::message_envelope resp_env;
      resp_env.message_type = "HANDSHAKE_RESPONSE";
      resp_env.protocol_version = protocol::PROTOCOL_VERSION;
      nlohmann::json resp_payload;
      protocol::to_json(resp_payload, resp);
      resp_env.payload = resp_payload;
      
      nlohmann::json j;
      protocol::to_json(j, resp_env);
      
      send(j.dump());

    } catch (const std::exception& e) {
       std::cerr << "[WebSocketSession] Handshake error: Malformed message - " << e.what() << std::endl;
       protocol::error_message err;
       err.error_code = "MALFORMED_HANDSHAKE";
       err.message = e.what();
       
       protocol::message_envelope err_env;
       err_env.message_type = "ERROR";
       err_env.protocol_version = protocol::PROTOCOL_VERSION;
       nlohmann::json err_payload;
       protocol::to_json(err_payload, err);
       err_env.payload = err_payload;
       
       nlohmann::json j;
       protocol::to_json(j, err_env);
       
       send(j.dump());
       close();
       return;
    }
  } else {
    // Authenticated - just log for now
    std::cout << "[WebSocketSession] Received from " << session_id_ << ": "
              << message << std::endl;
  }

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

  if (write_queue_.empty()) {
    writing_ = false;
    if (should_close_) {
      do_close();
    }
    return;
  }

  // Continue writing if more messages queued
  do_write();
}

void websocket_session::check_deadline() {
  auto self = shared_from_this();

  deadline_.async_wait(
      [self](boost::beast::error_code ec) {
        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
             std::cerr << "[WebSocketSession] Timer error: " << ec.message() << std::endl;
          }
          return;
        }

        if (self->state_ == state::unauthenticated) {
           // Timeout occurred
           std::cerr << "[WebSocketSession] Handshake timeout for session " << self->session_id_ << std::endl;
           self->close();
        }
      });
}

void websocket_session::close() {
  boost::asio::post(ws_.get_executor(), [self = shared_from_this()]() {
     if (self->writing_) {
         self->should_close_ = true;
     } else {
         self->do_close();
     }
  });
}

void websocket_session::do_close() {
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
