#!/usr/bin/bash

cargo build --release

# Start the relays
for i in {0..19}; 
do 
  sleep 0.2
  target/release/mprelay $i & 
done

# Start the sink
sleep 0.2
target/release/mpsink 20 &

# Start the client
sleep 0.2
target/release/mpclient
