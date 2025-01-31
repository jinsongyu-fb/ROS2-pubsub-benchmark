mod common;
mod timing;

#[tokio::main]
async fn main() {
  println!("Client starting.");
  zenoh::init_log_from_env_or("error");
  let session = zenoh::open(zenoh::Config::default()).await.unwrap();
  let publisher = session.declare_publisher("bench/hop0").await.unwrap();
  common::client_loop(publisher).await;
  async_std::task::sleep(std::time::Duration::from_secs(1)).await;
  println!("Client exiting.");
}
