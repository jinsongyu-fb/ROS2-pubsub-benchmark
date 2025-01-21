#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <thread>

#include "gbench/timing.grpc.pb.h"

using namespace std::chrono_literals;
constexpr int kRelayPortStart = 5000;
constexpr int kNumRelays = 20;

// The base class for Relay and Sink services.
class BenchServiceBase : public timing::Bench::Service {
 public:
  BenchServiceBase(int id) : port_(id + kRelayPortStart) {}

  void run() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:" + std::to_string(port_), grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    server_ = builder.BuildAndStart();
    // std::cout << "Server Ready.\n";
  }
  void wait() { server_->Wait(); }

 protected:
  int port_;
  std::unique_ptr<grpc::Server> server_;
};

// Relay service. After it gets a request, it immediately makes a request to
// the next relay.
class Relay final : public BenchServiceBase {
 public:
  Relay(int id)
      : BenchServiceBase(id),
        client_(timing::Bench::NewStub(grpc::CreateChannel("127.0.0.1:" + std::to_string(port_ + 1),
                                                           grpc::InsecureChannelCredentials()))) {}

  grpc::Status bench(grpc::ServerContext* context, const timing::Request* request,
                     timing::Response* response) override {
    response->set_ack(request->msgid());
    grpc::ClientContext client_context;
    timing::Request copy;
    copy.set_msgid(request->msgid());
    copy.set_nanosec(request->nanosec());
    copy.set_source("relay " + std::to_string(port_));
    timing::Response rsp;
    // std::cout << "Sending request.\n";
    grpc::Status status = client_->bench(&client_context, copy, &rsp);
    return grpc::Status::OK;
  }

 private:
  std::unique_ptr<timing::Bench::Stub> client_;
};

// Sink service. This is the last hop. After it gets a request, it calculates
// the per-hop latency.
class Sink final : public BenchServiceBase {
 public:
  Sink(int id) : BenchServiceBase(id) {}
  grpc::Status bench(grpc::ServerContext* context, const timing::Request* request,
                     timing::Response* response) override {
    response->set_ack(request->msgid());
    int64_t nanosec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    int64_t nanosec_per_hop = (nanosec - request->nanosec()) / (kNumRelays + 1);
    data_.insert(nanosec_per_hop);
    std::cout << "nano seconds per hop: " << nanosec_per_hop << "\n";

    if (data_.size() >= 1000) {
      print_stats();
      exit(0);
    }

    return grpc::Status::OK;
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

int main() {
  grpc::EnableDefaultHealthCheckService(true);

  // Create the relay and sink services.
  std::vector<std::unique_ptr<Relay>> relays;
  for (int i = 0; i < kNumRelays; ++i) {
    relays.emplace_back(std::make_unique<Relay>(i));
    relays[i]->run();
  }
  Sink sink(kNumRelays);
  sink.run();
  // Give them a second to initialize so not to interfere with benchmark run.
  std::this_thread::sleep_for(1s);
  std::cout << "Services initialized.\n";

  // Create the client and send requests.
  std::unique_ptr<timing::Bench::Stub> client = timing::Bench::NewStub(
      grpc::CreateChannel("127.0.0.1:5000", grpc::InsecureChannelCredentials()));
  for (int i = 0; i < 1000; ++i) {
    grpc::ClientContext context;
    timing::Request request;
    request.set_msgid(10);
    request.set_source("client");
    request.set_nanosec(std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count());
    timing::Response response;
    grpc::Status status = client->bench(&context, request, &response);
    if (!status.ok()) {
      std::cout << "Status= " << status.error_message() << ", ack= " << response.ack() << "\n";
    }
    std::this_thread::sleep_for(100ms);
  }
  sink.wait();
  for (int i = 0; i < kNumRelays; ++i) {
    relays[i]->wait();
  }
  return 0;
}