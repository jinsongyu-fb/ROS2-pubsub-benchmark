mod common;
mod timing;

#[tokio::main]
async fn main() {
  let args: Vec<String> = std::env::args().collect();
  if args.len() <= 1 {
    println!("Error: index expected in the command line args");
    return;
  }
  let index = args[1].parse::<i32>().unwrap();
  println!("Relay {} starting.", index);

  zenoh::init_log_from_env_or("error");
  let session = zenoh::open(zenoh::Config::default()).await.unwrap();

  // Create the relay, and start the relay receive loops.
  let subscriber = session
    .declare_subscriber(format!("bench/hop{}", index))
    .await
    .unwrap();
  let publisher = session
    .declare_publisher(format!("bench/hop{}", index + 1))
    .await
    .unwrap();
  common::relay_loop(index, subscriber, publisher).await;
  async_std::task::sleep(std::time::Duration::from_secs(1)).await;
  println!("Relay {} exiting.", index);
}
