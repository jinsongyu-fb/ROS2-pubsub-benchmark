mod timing;

use std::time::Duration;

use thrift::protocol::TSerializable;
type Handler = zenoh::handlers::FifoChannelHandler<zenoh::sample::Sample>;
const NUM_RELAYS: i32 = 20;

#[tokio::main]
async fn main() {
  zenoh::init_log_from_env_or("error");
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
  // TODO

  // Create the publisher and publishes messages at a set frequency.
  {
    let publisher = session.declare_publisher("bench/hop0").await.unwrap();

    for i in 0..1000 {
      // Create an in-memory channel for thrift serialization.
      let mut channel = thrift::transport::TBufferChannel::with_capacity(0, 128);
      // Create a Timing struct and serialize it to thrift.
      let mut proto = thrift::protocol::TCompactOutputProtocol::new(&mut channel);
      let t = timing::Timing {
        msgid: i,
        nanosec: 0,
        source: "publisher".to_string(),
      };
      t.write_to_out_protocol(&mut proto).unwrap();

      // Publish the serialized message to zenoh.
      publisher.put(channel.write_bytes()).await.unwrap();
      async_std::task::sleep(Duration::from_millis(1)).await;
    }
  }
  println!("Hello, world!");
  async_std::task::sleep(Duration::from_secs(1)).await;
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
    println!("{} : {}, {}, {}", index, t.msgid, t.nanosec, t.source);

    // Serialize the message to thrift and publish it.
    let mut out_proto = thrift::protocol::TCompactOutputProtocol::new(&mut channel);
    t.write_to_out_protocol(&mut out_proto).unwrap();
    publisher.put(channel.write_bytes()).await.unwrap();
  }
}
