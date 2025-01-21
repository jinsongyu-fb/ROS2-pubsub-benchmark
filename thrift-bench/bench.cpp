#include "gen-cpp/Bench.h"

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include <iostream>
#include <memory>
#include <thread>
#include <unordered_set>

using namespace std::chrono_literals;
constexpr int kRelayPortStart = 5000;
constexpr int kNumRelays = 20;

// The client we use send requests to the server.
class RelayClient {
 public:
  RelayClient(int id)
      : port_(id + kRelayPortStart),
        socket_(new apache::thrift::transport::TSocket("localhost", port_)),
        transport_(new apache::thrift::transport::TBufferedTransport(socket_)),
        protocol_(new apache::thrift::protocol::TBinaryProtocol(transport_)),
        client_(protocol_) {}
  void prepare() { transport_->open(); }
  int64_t bench(const timing& msg) { return client_.bench(msg); }

 private:
  int port_;
  std::shared_ptr<apache::thrift::transport::TSocket> socket_;
  std::shared_ptr<apache::thrift::transport::TBufferedTransport> transport_;
  std::shared_ptr<apache::thrift::protocol::TBinaryProtocol> protocol_;
  BenchClient client_;
};

// Relay server handler. After it gets a request, it immediately makes
// a request to the next relay.
class RelayHandler : virtual public BenchIf {
 public:
  RelayHandler(int id) : id_(id), client_(id + 1) {}
  void prepare() { client_.prepare(); }
  int64_t bench(const timing& arg) {
    timing copy;
    copy.msgid = arg.msgid;
    copy.nanosec = arg.nanosec;
    copy.source = "relay " + std::to_string(id_);
    return client_.bench(copy);
  }

 private:
  int id_;
  RelayClient client_;
};

// Sink server handler. This is the last hop. After it gets a request,
// it calculates the per-hop latency.
class SinkHandler : virtual public BenchIf {
 public:
  int64_t bench(const timing& arg) {
    int64_t nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    int64_t nanosec_per_hop = (nanosec - arg.nanosec) / (kNumRelays + 1);
    data_.insert(nanosec_per_hop);
    std::cout << "nano seconds per hop: " << nanosec_per_hop << "\n";

    if (data_.size() >= 1000) {
      print_stats();
      exit(0);
    }
    return arg.msgid;
  }

 private:
  void print_stats() {
    std::vector<int64_t> array(data_.cbegin(), data_.cend());
    std::sort(array.begin(), array.end());
    int p50_index = array.size() / 2;
    int p90_index = array.size() * 9 / 10;
    std::cout << "\nStats with " << array.size() << " data points, ns/hop:"
              << "\nP50 = " << array[p50_index] / 1000 << "us, P90 = " << array[p90_index] / 1000
              << "us\n\n";
  }

  std::unordered_multiset<int64_t> data_;
};

// The server that runs the handling loop.
class BenchServer {
 public:
  BenchServer(int id, std::shared_ptr<BenchIf> handler)
      : port_(id + kRelayPortStart),
        handler_(handler),
        processor_(new BenchProcessor(handler_)),
        server_tx_(new apache::thrift::transport::TServerSocket(port_)),
        txf_(new apache::thrift::transport::TBufferedTransportFactory()),
        pf_(new apache::thrift::protocol::TBinaryProtocolFactory()),
        server_(processor_, server_tx_, txf_, pf_) {
    thread_ = std::make_unique<std::thread>([this]() { server_.serve(); });
  }

 private:
  int port_;
  std::shared_ptr<BenchIf> handler_;
  std::shared_ptr<BenchProcessor> processor_;
  std::shared_ptr<apache::thrift::transport::TServerTransport> server_tx_;
  std::shared_ptr<apache::thrift::transport::TTransportFactory> txf_;
  std::shared_ptr<apache::thrift::transport::TProtocolFactory> pf_;
  apache::thrift::server::TSimpleServer server_;
  std::unique_ptr<std::thread> thread_;
};

int main() {
  // Create the relay and sink services.
  std::vector<std::shared_ptr<RelayHandler>> handlers;
  std::vector<std::unique_ptr<BenchServer>> relays;
  for (int i = 0; i < kNumRelays; ++i) {
    handlers.emplace_back(std::make_shared<RelayHandler>(i));
    relays.emplace_back(std::make_unique<BenchServer>(i, handlers[i]));
  }
  BenchServer sink(kNumRelays, std::make_shared<SinkHandler>());
  // Give them a second to initialize so not to interfere with benchmark run.
  std::this_thread::sleep_for(1s);
  for (int i = 0; i < kNumRelays; ++i) {
    handlers[i]->prepare();
  }
  std::this_thread::sleep_for(1s);
  std::cout << "Services initialized.\n";

  // Create the client and send requests.
  RelayClient client(0);
  client.prepare();
  for (int i = 0; i < 1000; ++i) {
    timing msg;
    msg.msgid = 0;
    msg.source = "client";
    msg.nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
    int64_t ack = client.bench(msg);
    std::this_thread::sleep_for(100ms);
  }
  std::this_thread::sleep_for(100s);
  return 0;
}