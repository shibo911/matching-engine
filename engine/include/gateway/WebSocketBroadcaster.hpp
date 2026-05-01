#pragma once

#include "concurrency/SPMCQueue.hpp"
#include "data/TradeEvent.hpp"
#include <atomic>
#include <chrono>
#include <crow.h>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace matching_engine {
namespace gateway {

class WebSocketBroadcaster {
public:
  WebSocketBroadcaster(
      concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> &egress_queue)
      : egress_queue_(egress_queue), last_sequence_(0), running_(false) {}

  ~WebSocketBroadcaster() { stop(); }

  void start(uint16_t port = 8080) {
    if (running_)
      return;
    running_ = true;

    // 1. Start the Crow WebServer thread
    server_thread_ = std::thread([this, port]() {
      // Health check endpoint for deployment platforms (Railway, Docker, etc.)
      CROW_ROUTE(app_, "/health")([this]() {
        crow::json::wvalue response;
        response["status"] = "healthy";
        response["service"] = "matching-engine";
        response["timestamp"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        response["connected_clients"] = users_.size();
        response["queue_depth"] = 0; // Lock-free queue size not easily available
        return crow::response(200, response);
      });

      CROW_ROUTE(app_, "/ws")
          .websocket(&app_)
          .onopen([&](crow::websocket::connection &conn) {
            std::lock_guard<std::mutex> lock(mtx_);
            users_.insert(&conn);
          })
          .onclose([&](crow::websocket::connection &conn,
                       const std::string & /*reason*/) {
            std::lock_guard<std::mutex> lock(mtx_);
            users_.erase(&conn);
          })
          .onmessage([&](crow::websocket::connection & /*conn*/,
                         const std::string & /*data*/, bool /*is_binary*/) {
            // One-way egress broadcast; ignore incoming client messages
          });

      // Crow blocks this thread to serve websocket IO
      // Setting log level to WARNING to avoid polluting the ultra-low-latency
      // stdout
      app_.loglevel(crow::LogLevel::Warning);
      app_.bindaddr("0.0.0.0").port(port).multithreaded().run();
    });

    // 2. Start the Background Consumer & Serializer thread (20 FPS)
    broadcast_thread_ = std::thread([this]() {
      using namespace std::chrono_literals;

      data::TradeEvent event;
      while (running_) {
        std::vector<crow::json::wvalue> batch;

        // Drain the Lock-Free SPMC Egress Queue
        // This does NOT block the Engine thread that pushes to it!
        while (egress_queue_.pop(last_sequence_, event)) {
          crow::json::wvalue e;
          e["maker_id"] = event.maker_order_id;
          e["taker_id"] = event.taker_order_id;
          e["price"] = event.price;
          e["quantity"] = event.quantity;
          e["timestamp"] = event.timestamp;
          e["side"] = (static_cast<int>(event.side) == 0) ? "buy" : "sell";
          batch.push_back(std::move(e));
        }

        // If we executed trades, serialize to JSON array and broadcast
        if (!batch.empty()) {
          crow::json::wvalue data_obj;
          data_obj["events"] = std::move(batch);

          crow::json::wvalue payload;
          payload["type"] = "trade_batch";
          payload["data"] = std::move(data_obj);
          std::string json_str = payload.dump();

          std::lock_guard<std::mutex> lock(mtx_);
          for (auto u : users_) {
            u->send_text(json_str);
          }
        }

        // Throttle the visualizer broadcasts to 20 frames per second
        // This prevents overloading the web browser UI with thousands of
        // renders
        std::this_thread::sleep_for(50ms);
      }
    });
  }

  void stop() {
    if (running_) {
      running_ = false;
      app_.stop();
      if (server_thread_.joinable())
        server_thread_.join();
      if (broadcast_thread_.joinable())
        broadcast_thread_.join();
    }
  }

private:
  concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> &egress_queue_;
  std::size_t last_sequence_;

  crow::SimpleApp app_;
  std::mutex mtx_;
  std::unordered_set<crow::websocket::connection *> users_;

  std::atomic<bool> running_;
  std::thread server_thread_;
  std::thread broadcast_thread_;
};

} // namespace gateway
} // namespace matching_engine
