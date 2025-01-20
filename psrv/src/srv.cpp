#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "pnodeif/srv/bench.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;
using ClientNode =
    std::pair<std::shared_ptr<rclcpp::Node>, std::shared_ptr<rclcpp::Client<pnodeif::srv::Bench>>>;
using ServiceNode =
    std::pair<std::shared_ptr<rclcpp::Node>, std::shared_ptr<rclcpp::Service<pnodeif::srv::Bench>>>;

constexpr int kNumRelays = 20;

// The client thread to initiate the service requests.
void client_thread(std::shared_ptr<rclcpp::Client<pnodeif::srv::Bench>> client) {
  std::cout << "Wating for relay ...";
  while (!client->wait_for_service(1s));
  std::cout << " ready.\n";

  for (int i = 0; i < 1000; i++) {
    auto request = std::make_shared<pnodeif::srv::Bench::Request>();
    request->timing.source = "client";
    request->timing.msgid = i;
    request->timing.nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();
    auto result = client->async_send_request(
        request, [](std::shared_future<std::shared_ptr<pnodeif::srv::Bench::Response>>) {});
    std::this_thread::sleep_for(1ms);
  }
  std::cout << "All requests sent.\n";
}

int main(int argc, char* argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::executors::MultiThreadedExecutor executor;

  // Create the clients.
  std::vector<ClientNode> clients;
  for (int i = 0; i <= kNumRelays; i++) {
    auto node = rclcpp::Node::make_shared("srv_client_" + std::to_string(i));
    auto client = node->create_client<pnodeif::srv::Bench>("srv_relay_" + std::to_string(i));
    clients.emplace_back(std::move(node), std::move(client));
  }

  // Create the relay services.
  // Each relay service gets requests from the previous relay hop and sends
  // a service request to the next hop.
  std::vector<ServiceNode> services;
  for (int i = 0; i < kNumRelays; i++) {
    auto node = rclcpp::Node::make_shared("srv_relay_" + std::to_string(i));
    auto client = clients[i + 1].second;
    auto service = node->create_service<pnodeif::srv::Bench>(
        "srv_relay_" + std::to_string(i),
        [i, client = std::move(client)](const std::shared_ptr<pnodeif::srv::Bench::Request> request,
                                        std::shared_ptr<pnodeif::srv::Bench::Response> response) {
          response->ack = request->timing.msgid;
          // std::cout << "relay[" << i << "] " << request->timing.msgid << "\n ";
          auto copy = std::make_shared<pnodeif::srv::Bench::Request>();
          copy->timing.source = "srv relay " + std::to_string(i);
          copy->timing.msgid = request->timing.msgid;
          copy->timing.nanosec = request->timing.nanosec;
          client->async_send_request(
              copy, [](std::shared_future<std::shared_ptr<pnodeif::srv::Bench::Response>>) {});
        });
    services.emplace_back(std::move(node), std::move(service));
    executor.add_node(services[i].first);
  }

  // Create the sink service.
  // The sink service gets requests from the last relay service, and computes
  // the per-hop communication latency.
  std::unordered_multiset<int64_t> data;
  auto sink_node = rclcpp::Node::make_shared("srv_sink");
  auto sink_service = sink_node->create_service<pnodeif::srv::Bench>(
      "srv_relay_" + std::to_string(kNumRelays),
      [&data](const std::shared_ptr<pnodeif::srv::Bench::Request> request,
              std::shared_ptr<pnodeif::srv::Bench::Response> response) {
        response->ack = request->timing.msgid;
        int64_t nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              std::chrono::steady_clock::now().time_since_epoch())
                              .count();
        int64_t nanosec_per_hop = (nanosec - request->timing.nanosec) / (kNumRelays + 1);
        data.insert(nanosec_per_hop);
        std::cout << "nano seconds per hop: " << nanosec_per_hop << "\n";
        if (data.size() >= 1000) {
          std::vector<int64_t> array(data.cbegin(), data.cend());
          std::sort(array.begin(), array.end());
          int p50_index = array.size() / 2;
          int p90_index = array.size() * 9 / 10;
          std::cout << "\nStats with " << array.size() << " data points, ns/hop:"
                    << "\nP50 = " << array[p50_index] / 1000
                    << "us, P90 = " << array[p90_index] / 1000 << "us\n\n";
          exit(0);
        }
      });
  executor.add_node(sink_node);

  // Create the client thread.
  std::thread client(client_thread, clients[0].second);
  // Spin the executor.
  executor.spin();
  rclcpp::shutdown();
  return 0;
}