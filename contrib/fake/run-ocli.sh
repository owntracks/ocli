#!/bin/sh

# use hashed hostname as last element of topic branch
# to try and anonymize accesses.

h=`hostname`
t=`md5 -s $h | awk '{print $4}'`

export MQTT_HOST=test.mosquitto.org
export MQTT_PORT=1883
export GPSD_PORT=5000
export OCLI_DISPLACEMENT=0
export OCLI_INTERVAL=0
export BASE_TOPIC="ouch/fake/${t}"

cat <<EOF
  Publishing to $BASE_TOPIC. You should be able to run
    mosquitto_sub -h "${MQTT_HOST}" -p ${MQTT_PORT} -v -t "${BASE_TOPIC}"

EOF

ocli
