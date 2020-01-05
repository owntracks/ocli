Fake data generator for testing ocli(1).

1. run ./run-fakegps.sh in a terminal. This invokes gpsfake(1) on
   localhost:5000

   You can safely ignore the message
   gpsd:ERROR: unable to connect to the DBUS system bus

2. run ./run-ocli.sh in a second terminal. It connects ocli(1) to
   the gpsd running on localhost:5000 and publishes to a random
   topic at test.mosquitto.org. Note: for added privacy use your 
   own broker.
