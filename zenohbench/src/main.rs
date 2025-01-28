mod timing;

use std::time::Duration;

use thrift::protocol::TSerializable;
type Handler = zenoh::handlers::FifoChannelHandler<zenoh::sample::Sample>;
const NUM_RELAYS: i32 = 20;

#[tokio::main]
async fn main() {
  zenoh::init_log_from_env_or("error");
  let epoch = std::time::Instant::now();
  let session = zenoh::open(zenoh::Config::default()).await.unwrap();

  // Create the relays, and start the relay receive loops.
  for i in 0..NUM_RELAYS {
    let subscriber = session
      .declare_subscriber(format!("bench/hop{}", i))
      .await
      .unwrap();
    let publisher = session
      .declare_publisher(format!("bench/hop{}", i + 1))
      .await
      .unwrap();
    async_std::task::spawn(relay_loop(i, subscriber, publisher));
  }

  // Create the sink
  {
    let subscriber = session.declare_subscriber(format!("bench/hop{}", NUM_RELAYS)).await.unwrap();
    async_std::task::spawn(sink_loop(subscriber, epoch));
  }

  // Create the publisher and publishes messages at a set frequency.
  {
    let publisher = session.declare_publisher("bench/hop0").await.unwrap();

    for i in 0..1000 {
      // Create an in-memory channel for thrift serialization.
      let mut channel = thrift::transport::TBufferChannel::with_capacity(0, 128);
      // Create a Timing struct and serialize it to thrift.
      let mut proto = thrift::protocol::TCompactOutputProtocol::new(&mut channel);
      let elapsed = epoch.elapsed().as_nanos() as i64;
      let t = timing::Timing {
        msgid: i,
        nanosec: elapsed,
        source: "publisher".to_string(),
      };
      t.write_to_out_protocol(&mut proto).unwrap();

      // Publish the serialized message to zenoh.
      publisher.put(channel.write_bytes()).await.unwrap();
      async_std::task::sleep(Duration::from_millis(1)).await;
    }
  }
  println!("Hello, world!");
  async_std::task::sleep(Duration::from_secs(2)).await;
}

// This function let a subscriber receive messages from zenoh. Once a message
// is received, it immediately sends another message to the next hop.
async fn relay_loop(
  index: i32,
  subscriber: zenoh::pubsub::Subscriber<Handler>,
  publisher: zenoh::pubsub::Publisher<'_>,
) {
  // Run the receive loop.
  while let Ok(data) = subscriber.recv_async().await {
    // Create an in-memory channel for thrift serialization/deserialization.
    let mut channel = thrift::transport::TBufferChannel::with_capacity(128, 128);
    // Read the thrift struct from the message payload.
    channel.set_readable_bytes(&data.payload().to_bytes());
    let mut in_proto = thrift::protocol::TCompactInputProtocol::new(&mut channel);
    let mut t = timing::Timing::read_from_in_protocol(&mut in_proto).unwrap();
    // Overwrite part of the message.
    t.source = format!("hop {}", index);

    // Serialize the message to thrift and publish it.
    let mut out_proto = thrift::protocol::TCompactOutputProtocol::new(&mut channel);
    t.write_to_out_protocol(&mut out_proto).unwrap();
    publisher.put(channel.write_bytes()).await.unwrap();
  }
}

// This function runs a loop to receive messages from the last pub/sub hop,
// and calculate the latency numbers.
async fn sink_loop(subscriber: zenoh::pubsub::Subscriber<Handler>, epoch: std::time::Instant) {
  let mut latency_vec = Vec::new();

  // Run the receive loop.
  while let Ok(data) = subscriber.recv_async().await {
    // Create an in-memory channel for thrift serialization/deserialization.
    let mut channel = thrift::transport::TBufferChannel::with_capacity(128, 0);

    // Read the thrift struct from the message payload.
    channel.set_readable_bytes(&data.payload().to_bytes());
    let mut in_proto = thrift::protocol::TCompactInputProtocol::new(&mut channel);
    let t = timing::Timing::read_from_in_protocol(&mut in_proto).unwrap();
    let elapsed = epoch.elapsed().as_nanos() as i64;
    let latency = (elapsed - t.nanosec) / (NUM_RELAYS + 1) as i64;
    latency_vec.push(latency);

    // Calculate and print the latency numbers.
    if latency_vec.len() == 1000 {
      latency_vec.sort();
      let p50_us = latency_vec[latency_vec.len() / 2] / 1000;
      let p90_us = latency_vec[latency_vec.len() * 9 / 10]/ 1000;
      println!("p50: {}us, p90: {}us", p50_us, p90_us);
      latency_vec.clear();
      return ();
    }
  }
}
