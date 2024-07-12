### Proxy serial port over TCP for debugging

on the car:

```sh
socat TCP-LISTEN:9601,reuseaddr,fork /dev/ttyS0,raw,echo=0,b9600
```

locally:

```sh
socat -d -d pty,raw,echo=0 TCP-CONNECT:car.tail9b1f8.ts.net:9601
```

Get the port `/dev/pts/N` from the output and use it in:

```sh
cargo run -- --serial-port=/dev/pts/N --mqtt-host=h4.local
```
