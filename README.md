My thanks goes to Junxiao Shi (yoursunny.com) for his original code and documentation - so full credit goes to him for 99.9% of this code.
I added OLED display support and had to make one wiring modification with the slave select line (SS) on the SPI interface.

# TcpFipsyLoader example

In this example, the ESP32 receives a JEDEC file via TCP, and programs Fipsy once the file has been verified.

## Wiring

ESP32 pin | Fipsy pin
----------|----------
3V3       | 1
GND       | 2
14        | 3
12        | 4
13        | 5
22        | 6

## Usage

1.  Modify WiFi setting in this sketch, and upload to ESP32.
2.  Look for the IP address in serial output.
3.  In Lattice Diamond, compile a JEDEC file for MachXO2-256.
4.  Execute `nc esp32-ip-address 34779 < filename.jed`.

Sample interaction:

```
$ nc 192.168.5.58 34779 < 1.jed
JEDEC OK, fuse checksum: A0A5
Feature Row: 0 0
FEABITS: 420
Programming ...
Success
```
