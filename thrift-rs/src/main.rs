mod timing;

use std::sync::Mutex;
use std::time::Duration;
use thrift::protocol::{
  TCompactInputProtocol, TCompactInputProtocolFactory, TCompactOutputProtocol,
  TCompactOutputProtocolFactory,
};
use thrift::transport::{
  ReadHalf, TBufferedReadTransport, TBufferedReadTransportFactory, TBufferedWriteTransport,
  TBufferedWriteTransportFactory, TIoChannel, TTcpChannel, WriteHalf,
};
use timing::TBenchSyncClient;

const NUM_RELAYS: i32 = 20;
const RELAY_PORT_START: i32 = 5000;
const NUM_MESSAGES: i32 = 1000;

type BenchClient = timing::BenchSyncClient<
  TCompactInputProtocol<TBufferedReadTransport<ReadHalf<TTcpChannel>>>,
  TCompactOutputProtocol<TBufferedWriteTransport<WriteHalf<TTcpChannel>>>,
>;

fn main() {
  let epoch = std::time::Instant::now();

  // Start the sink thread and the relay threads.
  // Use short sleeps to allow time for the listening threads to start.
  std::thread::spawn(move || sink_loop(epoch));
  std::thread::sleep(Duration::from_millis(20));
  for i in (0..NUM_RELAYS).rev() {
    std::thread::spawn(move || relay_loop(i));
    std::thread::sleep(Duration::from_millis(20));
  }

  // Create a client and send messages at a fixed frequency.
  {
    let mut client = create_client(0);
    for i in 0..NUM_MESSAGES {
      let elapsed = epoch.elapsed().as_nanos() as i64;
      let t = timing::Timing {
        msgid: i as i64,
        nanosec: elapsed,
        source: "client".to_string(),
      };
      client.bench(t).unwrap();
      std::thread::sleep(Duration::from_millis(1));
    }
  }
}

// Returns a thrift client, targeting localhost, port number
// determined by the index.
fn create_client(index: i32) -> BenchClient {
  let mut channel = TTcpChannel::new();
  channel
    .open(&format!("127.0.0.1:{}", index + RELAY_PORT_START))
    .unwrap();
  let (i_chan, o_chan) = channel.split().unwrap();
  let i_tran = TBufferedReadTransport::new(i_chan);
  let o_tran = TBufferedWriteTransport::new(o_chan);
  let i_prot = TCompactInputProtocol::new(i_tran);
  let o_prot = TCompactOutputProtocol::new(o_tran);
  timing::BenchSyncClient::new(i_prot, o_prot)
}

// Create the relay server and start listening.
fn relay_loop(index: i32) {
  let processor = timing::BenchSyncProcessor::new(RelayHandler {
    index: index,
    client: Mutex::new(create_client(index + 1)),
  });
  let mut server = thrift::server::TServer::new(
    TBufferedReadTransportFactory::new(),
    TCompactInputProtocolFactory::new(),
    TBufferedWriteTransportFactory::new(),
    TCompactOutputProtocolFactory::new(),
    processor,
    10,
  );

  server
    .listen(format!("127.0.0.1:{}", index + RELAY_PORT_START))
    .unwrap();
}

// The relay server handler to receive the `bench` call in
// the thrift service. After it receives the call, it immediately
// makes a call to the next relay hop.
struct RelayHandler {
  index: i32,
  client: Mutex<BenchClient>,
}
impl timing::BenchSyncHandler for RelayHandler {
  fn handle_bench(&self, arg: timing::Timing) -> thrift::Result<i64> {
    let copy = timing::Timing {
      source: format!("relay hop {}", self.index),
      ..arg
    };
    let mut c = self.client.lock().unwrap();
    c.bench(copy).unwrap();
    return Ok(0);
  }
}

// Create the sink server and start listening.
fn sink_loop(epoch: std::time::Instant) {
  let processor = timing::BenchSyncProcessor::new(SinkHandler {
    epoch: epoch,
    latency_vec: Mutex::new(Vec::new()),
  });
  let mut server = thrift::server::TServer::new(
    TBufferedReadTransportFactory::new(),
    TCompactInputProtocolFactory::new(),
    TBufferedWriteTransportFactory::new(),
    TCompactOutputProtocolFactory::new(),
    processor,
    4,
  );

  server
    .listen(format!("127.0.0.1:{}", NUM_RELAYS + RELAY_PORT_START))
    .unwrap();
}

// The sink server handler to receive the `bench` call in the thrift service.
// After it receives the call, it calculates the latency.
struct SinkHandler {
  epoch: std::time::Instant,
  latency_vec: Mutex<Vec<i64>>,
}
impl timing::BenchSyncHandler for SinkHandler {
  fn handle_bench(&self, arg: timing::Timing) -> thrift::Result<i64> {
    let elapsed = self.epoch.elapsed().as_nanos() as i64;
    let latency = (elapsed - arg.nanosec) / (NUM_RELAYS + 1) as i64;
    let mut vec = self.latency_vec.lock().unwrap();
    vec.push(latency);

    // Calculate and print the latency numbers.
    if vec.len() == NUM_MESSAGES as usize {
      vec.sort();
      let p50_us = vec[vec.len() / 2] / 1000;
      let p90_us = vec[vec.len() * 9 / 10] / 1000;
      println!("p50: {}us, p90: {}us", p50_us, p90_us);
      vec.clear();
    }

    return Ok(0);
  }
}
