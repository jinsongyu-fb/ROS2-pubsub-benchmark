# Measurement
The purpose of this simple code is to measure the communication latency
of simple messages in ROS 2 inside the same process.
The communication topology is 
```
Source => Relay_0 => Relay_1 => ... => Relay_n => Sink.
```

The message is defined in `pnodeif/msg/Timing.msg` and the service interface
in `pnodeif/srv/Bench.srv`. The `pnode` package runs benchmark to measure
latency for publisher/subscriber. And the `psrv` package runs benchmark to 
measure latency between a client and a service.

In `pnode` for publisher/subscriber, the `PnodeSource` node publishes
a message at {10Hz, 100Hz, 1000Hz}, and the message travels down the relay chain.
Each relay immediately publishes to the next hop when it receives
the message from the previous hop. We report per-hop latency, by taking
the duration from the source to the sink, divided by the number of hops
in between. 

In `psrv` for client/service, a client thread sends request to the first
relay at {10Hz, 100Hz, 1000Hz}, and the request travels down the relay chain.
Each relay immediately sends a request to the next hop when it receives
the request from the previous hop. We report per-hop latency, by taking
the duration from the source to the sink, divided by the number of hops
in between. 

We run all nodes in a single process. Running them in different processes
will only increase latency. And the results in a single process already
shows how expensive the message passing is in ROS 2. 

Environment: Ubuntu 24.04, ROS 2 Jazzy, 
release build (with `--cmake-args -DCMAKE_BUILD_TYPE=Release`),
Intel i7-9700K.

# Results:
After experimenting with many runs, we make the following observations 
(applicable to both pub/sub and client/service unless specified otherwise):
1. The per-hop latency exhibits bimodal behavior: 
    most of the time it's in the 700-800 microseconds range, but sometimes drops
    to the 100-200 microseconds range. The 50th and 90th percentile numbers 
    are very stable. 
2. Using single-threaded executor makes it significantly faster than
    using multi-threaded executor. This is not surprising because in this
    simple benchmarking app, each node
    does no interesting work besides passing the messages and therefore
    scheduling the nodes one after another on the same thread is in fact
    the best execution strategy. But in real robots, each node is expected
    to perform real work, and there will process boundaries between the nodes,
    so using single-threaded executor is unrealistic. 
    Therefore, while we show both sets of numbers in the table below as 
    a reference, the numbers in the multi-threaded executor rows are we
    what should focus on.
3. The highest latency cost is associated with ROS 2 threading. We observe
    that the latency is at the highest at 10Hz where the CPUs are mostly idling.
    And it gets significantly better at 100Hz at 1000Hz where CPUs idle less.
    The numbers at 10Hz are what we should focus on, both because it represents
    the worst case performance, and also because in a well behaving system
    we expect significant CPU idle time.
4. [Only applicable to pub/sub] Changing the QoS settings in various ways
    has virtually no impact to the observed latency.

The numbers in the following table are per-hop latency in microseconds
using multi-threaded executor:

| type           | frequency | P50 (us) | P90 (us) |
| -------------- | --------- | -------- | -------- |
| pub/sub        | 10Hz      | 818      | 824      |
| pub/sub        | 100Hz     | 107      | 113      |
| pub/sub        | 1000Hz    | 134      | 138      |
| client/service | 10Hz      | 675      | 689      |
| client/service | 100Hz     | 124      | 130      |
| client/service | 1000Hz    | 132      | 142      |

# For Comparison
### Single-threaded executor
Here are numbers using single-threaded executor. As explained above, 
single-threaded executor is unrealistic, and the numbers are only provided as
a reference:

| type           | frequency | P50 (us) | P90 (us) |
| -------------- | --------- | -------- | -------- |
| pub/sub        | 10Hz      | 270      | 308      |
| pub/sub        | 100Hz     | 52       | 55       |
| pub/sub        | 1000Hz    | 49       | 50       |
| client/service | 10Hz      | 318      | 331      |
| client/service | 100Hz     | 56       | 63       |
| client/service | 1000Hz    | 52       | 54       |

### gPRC
We also implemented a chain of gRPC client-server calls with the
same topology as above, in the `grpc-bench` directory. This is the eqivalence
of the client/service benchmark above. The results are significantly faster
than ROS 2. Per-hop latency in microseconds:

| frequency | P50 (us) | P90 (us) |
| --------- | -------- | -------- |
| 10Hz      | 470      | 479      |
| 100Hz     | 88       | 105      |
| 1000Hz    | 41       | 48       |
