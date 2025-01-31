#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "pnodeif/msg/timing.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

using namespace std::chrono_literals;
constexpr int kNumRelays = 20;

// Helper function that returns a QoS to use everywhere.
// This makes it easier to measure impact of QoS policies on timing.
rclcpp::QoS get_qos() {
  rclcpp::QoS qos(rclcpp::KeepLast(10));
  qos.reliability(rclcpp::ReliabilityPolicy::Reliable);
  qos.durability(rclcpp::DurabilityPolicy::TransientLocal);
  return qos;
}

// The source to generate messages.
class PnodeSource : public rclcpp::Node {
 public:
  PnodeSource(const rclcpp::NodeOptions&) : Node("source"), msgid_(0) {
    publisher_ = this->create_publisher<pnodeif::msg::Timing>("msg_0", get_qos());
    timer_ = this->create_wall_timer(1ms, [this]() { publish(); });
  }
  void publish() {
    pnodeif::msg::Timing t;
    t.msgid = ++msgid_;
    t.nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
    t.source = "pnode publisher";
    publisher_->publish(t);
    // std::cout << t.source << "\n";
  }

 private:
  std::shared_ptr<rclcpp::Publisher<pnodeif::msg::Timing>> publisher_;
  std::shared_ptr<rclcpp::TimerBase> timer_;
  int64_t msgid_;
};

// The relay to pass on messages.
class PnodeRelay : public rclcpp::Node {
 public:
  PnodeRelay(const rclcpp::NodeOptions& options)
      : Node("relay_" + options.arguments()[0]), index_(std::stoi(options.arguments()[0])) {
    publisher_ = this->create_publisher<pnodeif::msg::Timing>("msg_" + std::to_string(index_ + 1),
                                                              get_qos());
    subscriber_ = this->create_subscription<pnodeif::msg::Timing>(
        "msg_" + std::to_string(index_), get_qos(),
        [this](const pnodeif::msg::Timing& msg) { listen(msg); });
  }
  void listen(const pnodeif::msg::Timing& msg) {
    pnodeif::msg::Timing copy = msg;
    copy.source = "pnode relay " + std::to_string(index_);
    publisher_->publish(msg);
    // std::cout << copy.source << "\n";
  }
  int index() const { return index_; }

 private:
  int index_;
  std::shared_ptr<rclcpp::Publisher<pnodeif::msg::Timing>> publisher_;
  std::shared_ptr<rclcpp::Subscription<pnodeif::msg::Timing>> subscriber_;
};

// The sink to complete the final hop and calculate timing.
class PnodeSink : public rclcpp::Node {
 public:
  PnodeSink(const rclcpp::NodeOptions&) : Node("sink") {
    subscriber_ = this->create_subscription<pnodeif::msg::Timing>(
        "msg_" + std::to_string(kNumRelays), get_qos(),
        [this](const pnodeif::msg::Timing& msg) { listen(msg); });
  }
  void listen(const pnodeif::msg::Timing& msg) {
    int64_t nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    int64_t nanosec_per_hop = (nanosec - msg.nanosec) / (kNumRelays + 1);
    data_.insert(nanosec_per_hop);
    // std::cout << "nano seconds per hop: " << nanosec_per_hop << "\n";

    if (data_.size() >= 1000) {
      print_stats();
      exit(0);
    }
  }
  void print_stats() {
    std::vector<int64_t> array(data_.cbegin(), data_.cend());
    std::sort(array.begin(), array.end());
    int p50_index = array.size() / 2;
    int p90_index = array.size() * 9 / 10;
    std::cout << "\nStats with " << array.size() << " data points, ns/hop:"
              << "\nP50 = " << array[p50_index] / 1000 << "us, P90 = " << array[p90_index] / 1000
              << "us\n\n";
  }

 private:
  std::shared_ptr<rclcpp::Subscription<pnodeif::msg::Timing>> subscriber_;
  std::unordered_multiset<int64_t> data_;
};

int main(int argc, char* argv[]) {
  rclcpp::init(argc, argv);

  // Create the nodes, and keep them alive by holding the shared_ptrs here.
  rclcpp::NodeOptions node_options;
  std::cout << "Creating nodes ... ";
  auto source = std::make_shared<PnodeSource>(node_options);
  std::vector<std::shared_ptr<PnodeRelay>> relays;
  for (int i = 0; i < kNumRelays; ++i) {
    std::cout << i << ", ";
    node_options.arguments({std::to_string(i)});
    relays.push_back(std::make_shared<PnodeRelay>(node_options));
  }
  auto sink = std::make_shared<PnodeSink>(node_options);

  // Use a multi-threaded executor to spin all nodes.
  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(source);
  for (auto& relay : relays) {
    executor.add_node(relay);
  }
  executor.add_node(sink);
  std::cout << "\nAll nodes ready. Start spinning...\n";
  executor.spin();

  rclcpp::shutdown();
  return 0;
}