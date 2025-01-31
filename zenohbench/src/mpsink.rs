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
  println!("Sink {} starting.", index);

  zenoh::init_log_from_env_or("error");
  let session = zenoh::open(zenoh::Config::default()).await.unwrap();

  // Create the sink
  let subscriber = session
    .declare_subscriber(format!("bench/hop{}", index))
    .await
    .unwrap();
  common::sink_loop(index, subscriber).await;
  async_std::task::sleep(std::time::Duration::from_secs(1)).await;
  println!("Sink {} exiting.", index);
}
