mod common;
mod timing;

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
    async_std::task::spawn(common::relay_loop(i, subscriber, publisher));
  }

  // Create the sink
  {
    let subscriber = session
      .declare_subscriber(format!("bench/hop{}", NUM_RELAYS))
      .await
      .unwrap();
    async_std::task::spawn(common::sink_loop(NUM_RELAYS, subscriber));
  }

  // Create the publisher and publishes messages at a set frequency.
  {
    let publisher = session.declare_publisher("bench/hop0").await.unwrap();
    common::client_loop(publisher).await;
  }
  async_std::task::sleep(std::time::Duration::from_secs(2)).await;
}
