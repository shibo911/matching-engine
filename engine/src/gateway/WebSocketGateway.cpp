#include "gateway/WebSocketGateway.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace matching_engine {
namespace gateway {

using namespace std::chrono;

WebSocketGateway::WebSocketGateway(
    concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> &egress_queue,
    const GatewayConfig &config)
    : config_(config), egress_queue_(egress_queue),
      connections_(config.max_connections),
      start_time_ms_(
          duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count()) {

  if (!config_.validate()) {
    throw std::invalid_argument("Invalid WebSocketGateway configuration");
  }
}

WebSocketGateway::~WebSocketGateway() { stop(); }

bool WebSocketGateway::start() {
  if (running_.exchange(true)) {
    return false; // Already running
  }

  try {
#if UWS_ENABLE
    // Create uWebSockets app
    app_ = std::make_unique<uWS::App>();

    // Start server thread
    server_thread_ = std::thread([this]() { server_thread_func(); });

    // Start broadcast thread
    broadcast_thread_ = std::thread([this]() { broadcast_thread_func(); });

    std::cout << "[WebSocketGateway] Started on " << config_.host << ":"
              << config_.port << " with " << config_.broadcast_fps << " FPS"
              << std::endl;
#else
    // When uWebSockets is disabled, just start the broadcast thread
    // (which will consume from queue but not actually broadcast)
    broadcast_thread_ = std::thread([this]() { broadcast_thread_func(); });

    std::cout << "[WebSocketGateway] Started (uWebSockets disabled) - "
                 "broadcast thread running"
              << std::endl;
#endif
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[WebSocketGateway] Failed to start: " << e.what()
              << std::endl;
    running_.store(false);
    return false;
  }
}

void WebSocketGateway::stop() {
  if (!running_.exchange(false)) {
    return; // Already stopped
  }

  // Join threads
  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  if (broadcast_thread_.joinable()) {
    broadcast_thread_.join();
  }

  // Clear connections
  connections_.clear();

  std::cout << "[WebSocketGateway] Stopped" << std::endl;
}

#if UWS_ENABLE
void WebSocketGateway::server_thread_func() {
  try {
    // Setup WebSocket behavior
    app_->ws<PerSocketData>(
        "/*",
        {.compression = config_.enable_compression ? uWS::SHARED_COMPRESSOR
                                                   : uWS::DISABLED,
         .maxPayloadLength = config_.max_message_size,
         .idleTimeout = config_.send_timeout_ms / 1000, // Convert to seconds
         .open = [this](auto *ws) { on_connection(ws); },
         .close =
             [this](auto *ws, int code, std::string_view message) {
               on_disconnection(ws, code, message);
             },
         .message =
             [this](auto *ws, std::string_view message, uWS::OpCode opCode) {
               on_message(ws, message, opCode);
             }});

    // Add health check endpoint if enabled
    if (enable_http_endpoints_) {
      app_->get("/health", [](uWS::HttpResponse<false> *res,
                              uWS::HttpRequest *req) {
        res->writeStatus("200 OK");
        res->writeHeader("Content-Type", "application/json");
        res->end(
            R"({"status":"healthy","service":"matching-engine-websocket"})");
      });

      app_->get("/metrics",
                [this](uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
                  auto stats = get_stats();
                  std::stringstream ss;
                  ss << std::fixed << std::setprecision(2);
                  ss << R"({
  "uptime_ms": )" << stats.uptime_ms
                     << R"(,
  "connections_active": )"
                     << stats.connections_active << R"(,
  "connections_total": )"
                     << stats.connections_total << R"(,
  "messages_sent": )" << stats.messages_sent
                     << R"(,
  "bytes_sent": )" << stats.bytes_sent
                     << R"(,
  "queue_depth": )" << stats.queue_depth
                     << R"(,
  "messages_per_second": )"
                     << stats.messages_per_second << R"(
})";
                  res->writeStatus("200 OK");
                  res->writeHeader("Content-Type", "application/json");
                  res->end(ss.str());
                });
    }

    // Listen on configured host and port
    app_->listen(config_.host, config_.port, [this](auto *listenSocket) {
      if (listenSocket) {
        std::cout << "[WebSocketGateway] Listening on " << config_.host << ":"
                  << config_.port << std::endl;
      } else {
        std::cerr << "[WebSocketGateway] Failed to listen on " << config_.host
                  << ":" << config_.port << std::endl;
        running_.store(false);
      }
    });

    // Run the event loop
    app_->run();

  } catch (const std::exception &e) {
    std::cerr << "[WebSocketGateway] Server thread error: " << e.what()
              << std::endl;
    running_.store(false);
  }
}

void WebSocketGateway::on_connection(uWS::WebSocket<false, true> *ws) {
  connections_.add(ws);
  connections_total_.fetch_add(1, std::memory_order_relaxed);

  // Send welcome message
  std::string welcome = R"({"type":"welcome","version":"1.0","fps":)" +
                        std::to_string(config_.broadcast_fps) + "}";
  ws->send(welcome, uWS::OpCode::TEXT);

  std::cout << "[WebSocketGateway] New connection, total: "
            << connections_.size() << std::endl;
}

void WebSocketGateway::on_disconnection(uWS::WebSocket<false, true> *ws,
                                        int code, std::string_view message) {
  connections_.remove(ws);
  std::cout << "[WebSocketGateway] Connection closed, remaining: "
            << connections_.size() << std::endl;
}

void WebSocketGateway::on_message(uWS::WebSocket<false, true> *ws,
                                  std::string_view message,
                                  uWS::OpCode opCode) {
  // One-way egress broadcast; ignore incoming client messages
  // Could implement ping/pong or control messages here
  if (opCode == uWS::OpCode::PING) {
    ws->send("", uWS::OpCode::PONG);
  }
}
#else
// Dummy implementations when uWebSockets is disabled
void WebSocketGateway::server_thread_func() {
  // No-op when uWebSockets is disabled
  while (running_.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void WebSocketGateway::on_connection(void *ws) {
  // No-op
}

void WebSocketGateway::on_disconnection(void *ws, int code,
                                        std::string_view message) {
  // No-op
}

void WebSocketGateway::on_message(void *ws, std::string_view message,
                                  int opCode) {
  // No-op
}
#endif

void WebSocketGateway::broadcast_thread_func() {
  using namespace std::chrono;

  data::TradeEvent trade_event;
  BinaryTradeEvent binary_event;
  std::vector<BinaryTradeEvent> batch;
  batch.reserve(config_.queue_batch_size);

  const auto frame_interval = milliseconds(1000 / config_.broadcast_fps);
  auto next_frame_time = steady_clock::now();

  while (running_.load(std::memory_order_acquire)) {
    // 1. Collect batch from SPMC queue
    batch.clear();
    size_t messages_collected = 0;

    while (batch.size() < config_.queue_batch_size) {
      if (egress_queue_.pop(trade_event)) {
        trade_event_to_binary(trade_event, binary_event);
        batch.push_back(binary_event);
        ++messages_collected;
      } else {
        // Queue is empty, break out
        break;
      }
    }

    // 2. Broadcast if we have data
    if (!batch.empty()) {
      // Convert batch to binary buffer
      const size_t batch_size = batch.size() * sizeof(BinaryTradeEvent);
      const char *batch_data = reinterpret_cast<const char *>(batch.data());

      // Broadcast to all connections
      size_t broadcast_count = connections_.broadcast(batch_data, batch_size);

      if (broadcast_count > 0) {
        // Update statistics
        update_stats(batch.size(), batch_size);

        // Log periodically (every 1000 messages)
        static size_t log_counter = 0;
        if (++log_counter % 1000 == 0) {
          std::cout << "[WebSocketGateway] Broadcast " << batch.size()
                    << " events to " << broadcast_count << " clients"
                    << std::endl;
        }
      }
    }

    // 3. Throttle to target FPS
    next_frame_time += frame_interval;
    std::this_thread::sleep_until(next_frame_time);
  }
}

void WebSocketGateway::update_stats(uint64_t messages_sent,
                                    uint64_t bytes_sent) {
  messages_sent_.fetch_add(messages_sent, std::memory_order_relaxed);
  bytes_sent_.fetch_add(bytes_sent, std::memory_order_relaxed);

  // Update messages per second calculation
  static auto last_update = steady_clock::now();
  static uint64_t last_message_count = 0;

  auto now = steady_clock::now();
  auto elapsed = duration_cast<milliseconds>(now - last_update).count();

  if (elapsed >= 1000) { // Update every second
    uint64_t current_messages = messages_sent_.load(std::memory_order_relaxed);
    uint64_t messages_since_last = current_messages - last_message_count;

    // Store for next calculation
    last_update = now;
    last_message_count = current_messages;
  }
}

GatewayStats WebSocketGateway::get_stats() const {
  GatewayStats stats;

  auto now_ms =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();

  stats.uptime_ms = now_ms - start_time_ms_.load(std::memory_order_relaxed);
  stats.connections_active = connections_.size();
  stats.connections_total = connections_total_.load(std::memory_order_relaxed);
  stats.messages_sent = messages_sent_.load(std::memory_order_relaxed);
  stats.bytes_sent = bytes_sent_.load(std::memory_order_relaxed);

  // Estimate queue depth (this is approximate)
  // In a real implementation, we'd track this more accurately
  stats.queue_depth = 0;

  // Calculate messages per second
  if (stats.uptime_ms > 0) {
    stats.messages_per_second =
        (stats.messages_sent * 1000.0) / stats.uptime_ms;
  }

  return stats;
}

} // namespace gateway
} // namespace matching_engine