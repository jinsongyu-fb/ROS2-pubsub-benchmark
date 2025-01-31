# Message Passing Benchmarking
The name of this project has become a misnomer. We started this as a
benchmark measuring ROS 2 message passing latency, and it has grown to become
a benchmark comparison of message passing latency on various pub/sub and
RPC frameworks. All implementations follow the same topology:
```
Source => Relay_0 => Relay_1 => ... => Relay_n => Sink.
```
We use a small message (see `pnodeif/msg/Timing.msg`, `thrift-bench/timing.thrift`,
and `grpc-bench/gbench/timing.proto`), measure the time it took for the
message to travel from the Source to the Sink by passing through the relays,
then divide by the number of hops to get per-hop latency.
Unless documented explicitly, all components run in the same process.

We'll present the result in the summary table first, then explain the details
of the implementation.

# Summary
All numbers are per-hop latency, at 90th percentile (P90), in microseconds.
The overall takeaways:
* ROS 2 latency is the highest among all frameworks
* Latency of frameworks implemented in Rust is competitive to C++ implementations.

### Pub/Sub
| Hardware     | Msg Freq | ROS 2 (C++) | zenoh (Rust) |
| ------------ | -------- | ----------- | ------------ |
| i7-9700K     | 10Hz     | 824         | 27           |
| i7-9700K     | 100Hz    | 113         | 24           |
| i7-9700K     | 1000Hz   | 138         | 23           |
| Ryzen 5975WX | 10Hz     | 217         | 9            |
| Ryzen 5975WX | 100Hz    | 219         | 8            |
| Ryzen 5975WX | 1000Hz   | 223         | 4            |
| AGX Orin     | 10Hz     | 4041        | 61           |
| AGX Orin     | 100Hz    | 5365        | 38           |
| AGX Orin     | 1000Hz   | 7968        | 20           |

### Client/Server
| Hardware     | Msg Freq | ROS 2 (C++) | gRPC (C++) | thrift (C++) | thrift (Rust) |
| ------------ | -------- | ----------- | ---------- | ------------ | ------------- |
| i7-9700      | 10Hz     | 689         | 479        | 144          | 143           |
| i7-9700      | 100Hz    | 130         | 105        | 46           | 141           |
| i7-9700      | 1000Hz   | 142         | 48         | 38           | 138           |
| Ryzen 5975WX | 10Hz     | 212         | 164        | 52           | 68            |
| Ryzen 5975WX | 100Hz    | 184         | 118        | 29           | 35            |
| Ryzen 5975WX | 1000Hz   | 220         | 99         | 20           | 21            |
| AGX Orin     | 10Hz     | 3179        | 468        |              | 125           |
| AGX Orin     | 100Hz    | 4233        | 312        |              | 34            |
| AGX Orin     | 1000Hz   | ERROR*      | 311        |              | 32            |

*: ROS 2 client/server benchmark runs on AGX Orin drop messages at 1000Hz. 
The fastest it could run successfully without dropping message on AGX Orin
is at 200Hz. 

# Benchmarking Details
### Hardware setup
We ran the code on 3 different hardware setups. The focus of the benchmarks is to
compare the performance of the various message
passing frameworks, not to compare the hardware performance. We run on the different
hardware setups to validate the relative performance of the frameworks is consistent
regardless of the hardware choice.

1. Intel i7-9700K CPU, Ubuntu 24.04.
2. AMD Ryzen Threadripper PRO 5975WX CPU, Ubuntu 24.04 docker container on Fedora 41.
3. nVidia Jetson AGX Orin (Cortex-A78AE CPU), Ubuntu 22.04.

### Measurement and reporting
We run the benchmark by sending 1000 messages at {10, 100, 1000}Hz, in order to observe
the differences caused by signaling and scheduling behaviors when the system is at
different level of idling.

We only report P90 (90th percentile) numbers. The code actually reports both P50 and P90.
We observe that they are highly correlated and the differences between P50 and P90 is
relatively small.


### ROS 2
The ROS 2 message is defined in `pnodeif` directory. The `pnode` package runs benchmark
to measure latency for pub/sub. And the `psrv` package runs benchmark to
measure latency between a client and a service.

### ROS 2 with single-threaded executor
If we change the executor in `pnode` and `psrv` to use single-threaded executor,
ROS 2 runs faster than using the multi-threaded executor.
This is not surprising because in this simple benchmarking app, each node
does no interesting work besides passing the messages and therefore
scheduling the nodes one after another on the same thread is in fact
the best execution strategy. But in real robots, each node is expected
to perform real work, and there will process boundaries between the nodes,
so using single-threaded executor is unrealistic.

Therefore, we didn't list the numbers from the runs using single-threaded executors
here.

### ROS 2 tuning
We observed that changing the QoS settings in various ways
has virtually no impact to the observed latency in ROS 2 benchmarks.
And DDS configuration changes had no meaningful effects on the numbers.

### thrift and gPRC
* The `thrift-bench` directory is for thrift client-server in C++.
* The `thrift-rs` directory is for thrift client-server in Rust.
* The `grpc-bench` directory is for gRPC client-server in C++.

Note these these implementations use synchronous blocking APIs.

### zenoh pub/sub (Rust)
zenoh is a lightweight pub/sub framework implemented in Rust and have language
bindings including Python and others, and it's used in robotics.rs.
The `zenohbench` directory implements benchmark for zenoh in Rust 
(run `cargo run --release --bin bench`).
It uses async APIs and it's the fastest among all benchmark runs.

Besides the same-process small message size benchmark, we also implemented
multi-process pub/sub benchmark with zenoh, where each the client,
each relay, and the sink are all running in their own process. 
And we run with various message sizes at 100Hz. On Intel i7-9700K 
(run `./run_mp_bench.sh`):

| Msg Size  | same-proc | multi-proc |
| --------- | --------- | ---------- |
| 10        | 20        | 401        |
| 1000      | 20        | 347        |
| 10,000    | 25        | 361        |
| 100,000   | 139       | 575        |
| 1,000,000 | 297       | 2468       |
