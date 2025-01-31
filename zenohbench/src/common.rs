#![allow(dead_code)]

use crate::timing;

use thrift::protocol::TSerializable;
type Handler = zenoh::handlers::FifoChannelHandler<zenoh::sample::Sample>;
const NUM_MESSAGES: usize = 1000;
const PAYLOAD_SIZE: usize = 10000000;

// Runs the publisher loop, and publishes messages at a set frequency.
pub async fn client_loop(publisher: zenoh::pubsub::Publisher<'_>) {
  let payload = "publisher ".to_string().repeat(PAYLOAD_SIZE / 10);
  println!("  Payload size {}", payload.len());
  for i in 0..NUM_MESSAGES {
    // Create an in-memory channel for thrift serialization.
    let mut channel = thrift::transport::TBufferChannel::with_capacity(0, PAYLOAD_SIZE + 128);
    // Create a Timing struct and serialize it to thrift.
    let mut proto = thrift::protocol::TCompactOutputProtocol::new(&mut channel);
    let elapsed = std::time::SystemTime::now()
      .duration_since(std::time::UNIX_EPOCH)
      .unwrap()
      .as_nanos() as i64;
    let t = timing::Timing {
      msgid: i as i64,
      nanosec: elapsed,
      source: payload.clone(),
    };
    t.write_to_out_protocol(&mut proto).unwrap();

    // Publish the serialized message to zenoh.
    publisher.put(channel.write_bytes()).await.unwrap();
    async_std::task::sleep(std::time::Duration::from_millis(10)).await;
  }
}

// This function let a subscriber receive messages from zenoh. Once a message
// is received, it immediately sends another message to the next hop.
pub async fn relay_loop(
  _: i32,
  subscriber: zenoh::pubsub::Subscriber<Handler>,
  publisher: zenoh::pubsub::Publisher<'_>,
) {
  // Run the receive loop.
  let mut message_count = 0;
  while let Ok(data) = subscriber.recv_async().await {
    // Create an in-memory channel for thrift serialization/deserialization.
    let mut channel =
      thrift::transport::TBufferChannel::with_capacity(PAYLOAD_SIZE + 128, PAYLOAD_SIZE + 128);
    // Read the thrift struct from the message payload.
    channel.set_readable_bytes(&data.payload().to_bytes());
    let mut in_proto = thrift::protocol::TCompactInputProtocol::new(&mut channel);
    let t = timing::Timing::read_from_in_protocol(&mut in_proto).unwrap();

    // Serialize the message to thrift and publish it.
    let mut out_proto = thrift::protocol::TCompactOutputProtocol::new(&mut channel);
    t.write_to_out_protocol(&mut out_proto).unwrap();
    publisher.put(channel.write_bytes()).await.unwrap();

    message_count = message_count + 1;
    if message_count == NUM_MESSAGES {
      return;
    }
  }
}

// This function runs a loop to receive messages from the last pub/sub hop,
// and calculate the latency numbers.
pub async fn sink_loop(index: i32, subscriber: zenoh::pubsub::Subscriber<Handler>) {
  let mut latency_vec = Vec::new();

  // Run the receive loop.
  while let Ok(data) = subscriber.recv_async().await {
    // Create an in-memory channel for thrift serialization/deserialization.
    let mut channel = thrift::transport::TBufferChannel::with_capacity(PAYLOAD_SIZE + 128, 0);

    // Read the thrift struct from the message payload.
    channel.set_readable_bytes(&data.payload().to_bytes());
    let mut in_proto = thrift::protocol::TCompactInputProtocol::new(&mut channel);
    let t = timing::Timing::read_from_in_protocol(&mut in_proto).unwrap();
    let elapsed = std::time::SystemTime::now()
      .duration_since(std::time::UNIX_EPOCH)
      .unwrap()
      .as_nanos() as i64;
    let latency = (elapsed - t.nanosec) / (index + 1) as i64;
    latency_vec.push(latency);

    // Calculate and print the latency numbers.
    if latency_vec.len() == NUM_MESSAGES {
      latency_vec.sort();
      let p50_us = latency_vec[latency_vec.len() / 2] / 1000;
      let p90_us = latency_vec[latency_vec.len() * 9 / 10] / 1000;
      println!("p50: {}us, p90: {}us", p50_us, p90_us);
      latency_vec.clear();
      return ();
    }
  }
}
