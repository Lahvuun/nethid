# nethid

Share an HID device to another Linux machine.

Warning: This might break your HID device and blow your computer up, because
the code is in C and I don't know C. It didn't do either when I tested, but
you never know.

Also, here's another one from libusb/hidapi:
> Warning: Only run the code you understand, and only when it conforms to the
> device spec. Writing data (`hid_write`) at random to your HID devices can
> break them.

## Requirements
- `CONFIG_HIDRAW`.
- `CONFIG_UHID`.
- meson.
- A C compiler.

## Building

`meson build && ninja -C build`

## Usage

After building you will end up with two programs: `nethidserver` and
`nethidclient`. The first one will be run on the machine that the HID device
is connected to.

Both are written in a way to not depend on any particular type of connection
to communicate (despite the name *net*hid). `nethidserver` will receive data
on stdin and write data to stdout. A reasonably sane reader will then assume
that `nethidclient` does the same, and would be incorrect. `nethidclient`
reads from FD 6 and writes to FD 7, as per the
[UCSPI](https://cr.yp.to/proto/ucspi.txt).

So, the easiest way to use these two programs is with UCSPI tools. I used the
ones from s6-networking, like so:
- `s6-tcpserver 0.0.0.0 9999 ./build/nethidserver /dev/hidraw0`
- `s6-tcpclient raspberrypi 9999 ./build/nethidclient`

You might have to set up correct permissions for `/dev/hidraw*` and
`dev/uhid`. Half of the code is error checking and logging, so if you get
something like `permission denied`, chances are you need to acquire access to
those files. `/dev/hidraw*` for `nethidserver` and `/dev/uhid` for
`nethidclient`.

## Issues

- HID data from the client to the server (to the real device) won't be sent.
  This is probably for the best, since it lowers the chance of bricking your
  device, but also means stuff like force feedback likely won't work.
- If multiple events are queued from the device, the server will drop all but
  the newest one to minimize input lag.
- Version will always be 0.01. Don't know if this causes any issues.
