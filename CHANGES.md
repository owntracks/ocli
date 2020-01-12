# ocli changelog

## in progress

## 2020-01-12 0.8.0
- add support for pledge(2) and unveil(2) in OpenBSD
- Improve fake gps data for Paris, thanks to @ckrey
- Clarify subscription
- Add version, adapt CFLAGS, and -s and -v
- Add infrastructure for Docker-based packaging (#4)
- Switch to Semantic Versioning

## 2020-01-04 0.06
- Decrease default publish interval to 1s
- Add fake data generator

## 2020-01-04 0.05
- Getting CFLAGS right, seventeenth attempt
- Improve wording

## 2020-01-04 0.04
- Fix cflags

## 2020-01-04 0.03
- Add install target
- Add manual page

## 2020-01-02 0.01
- Replace `bash` markdown code blocks with `console`; by @linusg

## 2019-12-31 0.00
- Initial import from jps2m
- Add support for file params
- Support for reading from executable params
- Add -I and -L for *BSD
- Bring to GPSD API level 8 (for OpenBSD)
- Started on documentation
- Carry mosq pointer in udata
- Put all configurables in udata structure
- Document parms
- Rename minsecs and minmove consistently
- Document building; lol centos8
- Add some support for remoteConfig
- Make clientid configurable
- OwnTracks on macOS
- Add license file
- Change executable uname to contrib/platform
- Thank you, @ckrey, for the logo!
- Change default MQTT clientid
- Dump crashes if tid unset; reported by @ckrey
- reportLocation should print now() in tst; reported by @ckrey
- Publish all locations with retain; reported by @ckrey
- Selectively use retain; cleanup
- Subscribe on connect so we subscribe after auto-reconnect
- [platform] Be more specific for FreeBSD
- Add support for TLS; general cleanup; closes #1
