#[tokio::main]
async fn main() {
  zenoh::init_log_from_env_or("error");
  let session = zenoh::open(zenoh::Config::default()).await.unwrap();
  let subscriber: zenoh::pubsub::Subscriber<
    zenoh::handlers::FifoChannelHandler<zenoh::sample::Sample>,
  > = session.declare_subscriber("bench/hop0").await.unwrap();

  println!("Hello, world!");
}

async fn subscribe_loop(
  subscriber: zenoh::pubsub::Subscriber<zenoh::handlers::FifoChannelHandler<zenoh::sample::Sample>>,
) {
  while let Ok(data) = subscriber.recv_async().await {
    println!("{:?}", data);
  }
}
