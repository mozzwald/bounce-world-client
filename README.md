# bounce-world-client

A FujiNet application to connect to bouncy-world service.

Atari 8-bit builds use FujiNet Netstream mode for the live TCP connection. Other supported platforms continue to use `fujinet-lib`.
The Atari Netstream handler is built with `mads` from `/home/ahlegna/.mozzwald/fujinet-atari-netstream` and embedded into the `.com` as a fixed `$2800` load segment.

The bouncy world service can be found at https://github.com/markjfisher/bounce-world

## building

Standard make for cc65 projects. Set TARGETS for make if you wish to only build one platform.

```
make clean
make release disk
```

Specify `TARGETS` value if you only wish to compile a set of clients. This is normal cc65 behaviour.

## running

Deploy the released binary for the platform to a FujiNet (e.g. via SD or TNFS), and run it in your usual way.
It will ask you for the location of the bouncy world service. Enter as a simple tcp url, e.g. `tcp://localhost:9002`.

The "tcp://" part will be pre-pended if you omit it.
This version of the client uses TCP instead of HTTP, so you must pick the TCP port the service is running on.

## testing

See the `makefiles/custom-<platform>.mk` files for requirements to run code under emulation.

Supported at the moment is Altirra on Atari, set the `ALTIRRA_BIN` value to point to the appropriate executable, then run:

```shell
make TARGETS=atari test
```

## copying to SD

The fujinet supports webdav copying to the SD, and the cyberduck cli command `duck` can be used to do this on the command line.
This is particularly useful for testing c64 programs.

```shell
duck --upload dav://anonymous@fujinet/dav/bwc.prg dist/bwc.c64.prg -existing overwrite
```
