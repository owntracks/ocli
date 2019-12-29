# ocli

This is the OwnTracks command line interface publisher, a.k.a. _ocli_, a small utility which connects to _gpsd_ and publishes position information in [OwnTracks JSON](https://owntracks.org/booklet/tech/json/) in order for [compatible software](https://owntracks.org/booklet/guide/clients/) to process location data. (Read up on [what OwnTracks does](https://owntracks.org/booklet/guide/whathow/) if you're new to it.)

[gpsd] is a daemon that receives data from one or more GPS receivers and provides data to multiple applications via a TCP/IP service. GPS data thus obtained is processed by _ocli_ and published via [MQTT]

![vk-172](assets/img_9643.jpg)

#### payload

The following defaults are used:

- topic defaults to `owntracks/<username>/<hostname>`, where _username_ is the name of the logged in user, and _hostname_ the short host name. The topic can be overriden by setting `t_prefix` in the environment.


Any number of path names can be passed as arguments to _ocli_ which interprets each in terms of an element which will be added to the OwnTracks JSON. The element name is the basename of the path. If a path points to an executable file the first line of _stdout_ produced by that executable will be used as the _key_'s _value_, otherwise the first line read from the file. In both cases, trailing newlines are removed from values.

```bash
$ echo 27.2 > parms/temp
$ ocli parms/temp /usr/bin/uname
```

In this example, we use a file and a program. When _ocli_ produces its JSON we'll see something like this:

```json
{
  "_type": "location",
  "tst": 1577654651,
  "lat": ...
  "temp" : "27.2",
  "uname": "FreeBSD"
}
```

Note that a _key_ may not overwrite JSON keys defined by _ocli_, so for example, a file called `lat` will not be accepted as it would clobber the latitude JSON element.

### FreeBSD

```bash
# pkg install mosquitto gpsd
$ make
```

### OpenBSD

```bash
# pkg_add mosquitto gpsd
$ make
```

### macOS

```bash
$ brew install mosquitto gpsd
$ make
```

### Debian

```bash
# apt-get install libmosquitto-dev libgps-dev
```

### CentOS

### systemd

This may be a way of getting _ocli_ working on machines with _systemd_. Basically we need two things:

1. an environment file, `ocli.env`:
    ```
    name="Testing"
    fixlog="/tmp/fix.log"
    tprefix="m/bus/b001"
    MQTT_HOST="localhost"
    MQTT_PORT=1888
    GPSD_HOST="localhost"
    GPSD_PORT="2947"
    OCLI_TID="OC"
    ```
2. a systemd Unit file:
    ```
    [Unit]
    Description=OwnTracks cli
    Requires=mosquitto.service
    
    [Service]
    Type=simple
    EnvironmentFile=/home/jpm/ocli.env
    ExecStartPre=/usr/bin/touch /tmp/ocli-started-${name}
    ExecStart=/home/jpm/bin/ocli
    Restart=always
    RestartSec=60
    User=mosquitto
    Group=mosquitto
    
    [Install]
    WantedBy=multi-user.target
    ```


### Credits

- Idea and initial implementation by Jan-Piet Mens
- [gpsd]
- [mosquitto](https://mosquitto.org), by Roger A. Light
- [utarray](https://troydhanson.github.io/uthash/utarray.html)
- [utstring](https://troydhanson.github.io/uthash/utstring.html)

  [gpsd]: https://gpsd.gitlab.io/gpsd/
  [mqtt]: http://mqtt.org
