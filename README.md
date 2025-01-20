# Measurement
The purpose of this simple code is to measure the time it takes 
for simple messages to travel from the publisher to the subscriber.
The node topology is 
```
Source => Relay_0 => Relay_1 => ... => Relay_n => Sink.
```
The message is defined in `msg/Timing.msg`. Each relay immediately 
publishes to the next hop when it receives the message from the previous
hop. We report per-hop latency, by taking the duration from the source
to the sink, divided by the number of hops in between. 

We run all nodes in a single process. Running them in different processes
will only increase latency. And the results in a single process already
shows how expensive the message passing is in ROS 2. 

Environment: Ubuntu 24.04, ROS 2 Jazzy, release build, Intel i7-9700K.

# Results:
After experimenting with many runs, we make the following observations:
1. The per-hop latency exhibits bimodal behavior: 
    most of the time it's in the 800+ microseconds range, but sometimes drops
    to the ~150 microseconds range. But the 50th and 90th percentile numbers 
    are very stable. 
2. Changing the QoS settings in various ways has virtually no impact to
    the observed latency.
3. Using single-threaded executor makes it significantly faster than
    using multi-threaded executor. This is not surprising because in this
    simple benchmarking app, each node
    does no interesting work besides passing the messages and therefore
    scheduling the nodes one after another on the same thread is in fact
    the best execution strategy. But in real robots, each node is expected
    to perform real work and single-threaded executor is unrealistic. 
    So while we show both sets of numbers in the table below as a reference, 
    the numbers in the multi-threaded executor row are we what should focus on.

The numbers in the following table are per-hop latency in microseconds.

| executor        | P50 (us) | P90 (us) |
| --------------- | -------- | -------- |
| multi-threaded  | 818      | 824      |
| single-threaded | 270      | 308      |
