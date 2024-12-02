# Zephyr Firmware for Xiaomi Sensor Data Collection and TCP Streaming

This project demonstrates a firmware application built on the Zephyr RTOS for 
the nRF52840 (specifically the nrf528450dk and the Laird BL654 USB dongle). The 
firmware collects temperature and humidity data from Xiaomi LYWSD03MMC devices 
using BLE passive scanning. It then establishes a USB Ethernet link with a Linux 
host and streams the collected data over a simple TCP protocol.

## Features

- **BLE Passive Scanning**: Detect and extract measurements from Xiaomi LYWSD03MMC 
  sensors. More details on how to extract data from Xiaomi sensors can be found
   [here](./docs/ble_xiaomi.md).
- **USB Ethernet Link**: Implements a USB CDC ECM network interface for communication 
  with a Linux host. 
- **TCP Data Streaming**: Sends sensor data to the Linux host over a TCP connection.

## Quick Start Guide

### Prerequisites

1. **Hardware**:
   - Laird BL654 USB dongle or any nRF52840-based hardware.

2. **Software**:
   - Zephyr SDK and development environment.
   - A Linux host for testing and receiving the data stream.
   - Any TCP client tool (e.g., `nc` or custom Python script) on the host to receive data.

3. **Environment Setup**:
   Follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) to set up your environment.

### Build and Flash

1. Clone the repository:
   ```bash
   git clone https://github.com/lucasdietrich/zephyr-linux-ble-copro
   cd zephyr-linux-ble-copro
   ```

2. Build for `nrf52840dk`
   ```bash
   west build -b nrf52840dk_nrf52840
   ```

4. Flash the firmware:
   ```bash
   west flash
   ```

### Configuration

USB Network Setup: he device will appear as a USB Ethernet adapter on the Linux 
host. More details on how to configure the USB network interface can be 
found [here](./docs/usb_net.md).

You can manually configure the USB network interface with the following commands:

```bash
sudo ip addr add 192.0.3.1/24 dev usb0
sudo ip link set usb0 up
```

Test the connection with the device:

```bash
ping 192.0.3.2
```

### Running the Application

1. Start the device and connect it to the Linux host.
2. On the Linux host, listen for incoming data on the TCP port (default: `12345`):
   ```bash
   nc -l -p 4000 -v | hexdump -C
   ```
3. The device will start scanning for Xiaomi sensors and send data to the host.

### Expected output

Device console:

```
*** Booting Zephyr OS build v4.0.0-1-g17c6cd365d75 ***
Hello, World!
[00:00:00.255,493] <inf> ble: USB initialized 0
[00:00:00.257,171] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
[00:00:00.257,202] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
[00:00:00.257,232] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 4.0 Build 0
[00:00:00.258,026] <inf> bt_hci_core: Identity: FC:2F:11:9C:92:2E (random)
[00:00:00.258,056] <inf> bt_hci_core: HCI: version 5.4 (0x0d) revision 0x0000, manufacturer 0x05f1
[00:00:00.258,087] <inf> bt_hci_core: LMP: version 5.4 (0x0d) subver 0xffff
[00:00:00.258,087] <inf> ble: Bluetooth initialized 0
[00:00:00.259,094] <err> stream_client: Failed to connect: -1
[00:00:01.055,816] <wrn> usb_device: Endpoint 0x82 already disabled
[00:00:01.055,847] <wrn> usb_device: Endpoint 0x01 already disabled
[00:00:01.058,349] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000c not supported
[00:00:01.058,410] <inf> usb_net_mgmt: === NET interface 0x200013ac ===
[00:00:01.058,502] <inf> usb_net_mgmt: Address: 192.0.3.2 [addr type NET_ADDR_MANUAL]
[00:00:01.058,532] <inf> usb_net_mgmt: Subnet:  255.255.255.0
[00:00:01.058,593] <inf> usb_net_mgmt: Router:  192.0.3.1
[00:00:01.135,009] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000e not supported
[00:00:01.261,779] <inf> stream_client: Connected to 192.0.3.1:4000
[00:00:01.465,423] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:8D:BA:B4 rssi: -61 bat: 3219 mV temp: 20 °C hum: 47 %
[00:00:02.339,752] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000e not supported
[00:00:03.516,632] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:68:05:63 rssi: -53 bat: 3226 mV temp: 20 °C hum: 47 %
[00:00:03.945,770] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:EC:1C:6D rssi: -58 bat: 3250 mV temp: 20 °C hum: 46 %
[00:00:04.297,668] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:A7:30:C4 rssi: -81 bat: 2104 mV temp: 19 °C hum: 53 %
[00:00:06.021,514] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:68:05:63 rssi: -53 bat: 3226 mV temp: 20 °C hum: 47 %
[00:00:06.449,188] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:EC:1C:6D rssi: -58 bat: 3250 mV temp: 20 °C hum: 46 %
[00:00:06.465,423] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:8D:BA:B4 rssi: -54 bat: 3219 mV temp: 20 °C hum: 47 %
[00:00:08.965,759] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:8D:BA:B4 rssi: -53 bat: 3219 mV temp: 20 °C hum: 47 %
```

Linux host console:

```console
[lucas@fedora ~]$ nc -l 4000 -v | hexdump -c
Ncat: Version 7.93 ( https://nmap.org/ncat )
Ncat: Listening on :::4000
Ncat: Listening on 0.0.0.0:4000
Ncat: Connection from 192.0.3.2.
Ncat: Connection from 192.0.3.2:61748.
0000000  \0 264 272 215   8 301 244  \0 303  \0 004  \b 255 022 223  \f
0000010   d  \0  \0  \0  \0  \0  \0  \0 271 005  \0  \0  \0  \0  \0  \0
0000020 001  \0  \0  \0  \0  \0  \0  \0  \0   c 005   h   8 301 244  \0
0000030 313  \0 032  \b 210 022 232  \f   d  \0  \0  \0  \0  \0  \0  \0
0000040 274  \r  \0  \0  \0  \0  \0  \0 001  \0  \0  \0  \0  \0  \0  \0
0000050  \0   m 034 354   8 301 244  \0 306  \0   #  \b  \0 022 262  \f
0000060   d  \0  \0  \0  \0  \0  \0  \0   i 017  \0  \0  \0  \0  \0  \0
```
## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## TODOs

- Add keep-alive messages for the TCP connection.
- Use a fifo instead of the msgq