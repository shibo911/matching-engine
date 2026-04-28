#pragma once

#include "concurrency/SPMCQueue.hpp"
#include "data/TradeEvent.hpp"
#include "gateway/BinaryProtocol.hpp"
#include "gateway/Config.hpp"
#include "gateway/ConnectionManager.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#if UWS_ENABLE
// uWebSockets single-header include
#define ASIO_STANDALONE
#include <uWebSockets/App.h>
#else
// Stub types for when uWebSockets is disabled
namespace uWS {
template <bool SSL, bool PERMSG_DEFLATE> class WebSocket {};

class App {};

enum OpCode { TEXT = 1, BINARY = 2, CLOSE = 8, PING = 9, PONG = 10 };

constexpr auto SHARED_COMPRESSOR = 0;
constexpr auto DISABLED = 1;
} // namespace uWS
#endif

namespace matching_engine {
namespace gateway {

// Forward declaration for dummy app when uWebSockets is disabled
struct DummyApp;

class WebSocketGateway {
public:
  WebSocketGateway(
      concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> &egress_queue,
      const GatewayConfig &config = GatewayConfig{});

  ~WebSocketGateway();

  // Start the WebSocket server
  bool start();

  // Stop the server gracefully
  void stop();

  // Check if server is running
  bool is_running() const { return running_.load(std::memory_order_acquire); }

  // Get current statistics
  GatewayStats get_stats() const;

  // Get configuration
  const GatewayConfig &get_config() const { return config_; }

private:
  // uWebSockets event handlers (only when UWS_ENABLE is 1)
#if UWS_ENABLE
  void on_connection(uWS::WebSocket<false, true> *ws);
  void on_disconnection(uWS::WebSocket<false, true> *ws, int code,
                        std::string_view message);
  void on_message(uWS::WebSocket<false, true> *ws, std::string_view message,
                  uWS::OpCode opCode);
#else
  // Dummy implementations when uWebSockets is disabled
  void on_connection(void *ws) {}
  void on_disconnection(void *ws, int code, std::string_view message) {}
  void on_message(void *ws, std::string_view message, int opCode) {}
#endif

  // Background thread functions
  void broadcast_thread_func();
  void server_thread_func();

  // Statistics update
  void update_stats(uint64_t messages_sent, uint64_t bytes_sent);

  // Configuration
  GatewayConfig config_;

  // Queue reference
  concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> &egress_queue_;

  // Connection management
  LockFreeConnectionSet connections_;

  // Thread management
  std::atomic<bool> running_{false};
  std::thread server_thread_;
  std::thread broadcast_thread_;

  // Statistics
  mutable std::atomic<uint64_t> messages_sent_{0};
  mutable std::atomic<uint64_t> connections_total_{0};
  mutable std::atomic<uint64_t> bytes_sent_{0};
  mutable std::atomic<uint64_t> start_time_ms_{0};

#if UWS_ENABLE
  // uWebSockets app (only when enabled)
  std::unique_ptr<uWS::App> app_;
#else
  // Dummy pointer when disabled
  std::unique_ptr<DummyApp> app_;
#endif

  // Health check and metrics endpoints flag
  bool enable_http_endpoints_ = true;
};

} // namespace gateway
} // namespace matching_engine