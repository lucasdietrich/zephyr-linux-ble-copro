# Zephyr BLE Co-Processor Firmware

This project is a Zephyr RTOS firmware for the nRF52840 (nRF52840DK and Laird BL654 USB
dongle) that acts as a BLE co-processor for a Linux host. It collects data from BLE
sensors, exposes a GATT server to BLE clients, and streams all data bidirectionally to
the host over a USB Ethernet + TCP link.

A companion Rust library [`ble-copro-stream-server-rs`](./ble-copro-stream-server-rs/)
implements the host-side TCP server for receiving and dispatching the multiplexed stream
messages.

## Features

- **BLE Passive Scanning**: Detect and extract measurements from Xiaomi LYWSD03MMC
  sensors. More details on how to extract data from Xiaomi sensors can be found
  [here](./docs/ble_xiaomi.md).
- **Linky TIC Meter**: Passively scans for a BLE Linky meter device and streams its
  energy measurements.
- **BLE GATT Server** (`COPRO_BLE_SERVER`): Exposes a custom Garage Control Service
  (UUID `539f0000-…`) with characteristics for left door, right door, gate state
  (indicate) and firmware status flags (notify). Supports secure pairing with a numeric
  passkey, bond storage, and a configurable idle-connection timeout.
- **Device Control Channel** (`COPRO_DEVICE_CONTROL`): Bidirectional stream channel
  (`0xeb5d8977`) for garage-door automation. The firmware sends open/close commands to
  the host and receives door-state updates from the host.
- **BLE Control Channel** (`COPRO_BLE_SERVER`): Bidirectional stream channel
  (`0x4f154ca0`) that forwards BLE connection and pairing events (connected,
  disconnected, pairing code, pairing result, identity resolved, advertising window
  open/closed) to the host, and accepts bond-management actions (remove bond, remove all
  bonds, enable pairing advertising window) from the host.
- **Multiplexed TCP Stream** (`COPRO_STREAM_CLIENT`): All channels share a single TCP
  connection. Each frame is prefixed with a channel ID and payload length. A dedicated
  RX thread (enabled by `COPRO_STREAM_CHANNEL_RX`) dispatches incoming server frames to
  the matching channel queue.
- **USB Ethernet Link**: Implements a USB CDC ECM network interface for communication
  with a Linux host.
- **Rust Host Library**: `ble-copro-stream-server-rs` is an async Tokio library that
  parses all channel messages (Xiaomi, Linky, Device Control, BLE Control) and provides
  a typed API for the host application.

## Quick Start Guide

### Prerequisites

1. **Hardware**:
   - Laird BL654 USB dongle or any nRF52840-based hardware.

2. **Software**:
   - Zephyr SDK and development environment.
   - A Linux host for testing and receiving the data stream.
   - Rust toolchain (for the companion server library / example).

3. **Environment Setup**:
   Follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) to set up your environment.
   Otherwise create a new workspace with the following commands:
   
   ```bash
   mkdir zephyrproject-ble
   cd zephyrproject-ble
   python3 -m venv .venv
   source .venv/bin/activate
   pip install west
   west init -m https://github.com/lucasdietrich/zephyr-linux-ble-copro
   west update
   pip install -r zephyr/scripts/requirements.txt
   cd zephyr-linux-ble-copro
   ```

4. Create `.env`

  ```bash
  source ../.venv/bin/activate
  export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
  ```

### Build and Flash

Build and flash for `nrf52840dk`
   ```bash
   west build -b nrf52840dk/nrf52840
   west flash
   ```

Build for `laird-bl654`
   ```bash
   west build -b bl654_usb
   ```

Download the [build/zephyr/ble-copro.hex](build/zephyr/ble-copro.hex) file,
set the device in DFU mode (click the button on the dongle), then
use *nRF Connect Desktop Programmer* to flash the dongle.

### Configuration

USB Network Setup: the device will appear as a USB Ethernet adapter on the Linux
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
2. On the Linux host, run the companion Rust server example:
   ```bash
   cd ble-copro-stream-server-rs
   cargo run --example server
   ```
   Or, for quick raw inspection, listen with `nc`:
   ```bash
   nc -l -p 4000 -v | hexdump -C
   ```
3. The device will connect, send a firmware-version control frame, then start
   streaming sensor and event messages on their respective channels.

### Expected output

Device console:

```
Starting BLE Co-Processor firmware 1.0.0
[00:00:00.253,082] <inf> usb_net: netusb initialized
[00:00:00.264,526] <inf> fs_nvs: data wra: 2, 2ac
--- 4 messages dropped ---
[00:00:00.267,761] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
[00:00:00.267,791] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
[00:00:00.267,822] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 4.3 Build 0
[00:00:00.268,218] <inf> bt_hci_core: No ID address. App must call settings_load()
[00:00:00.274,719] <inf> bt_hci_core: HCI transport: Controller
[00:00:00.274,810] <inf> bt_hci_core: Identity: FC:2F:11:9C:92:2E (random)
[00:00:00.274,841] <inf> bt_hci_core: HCI: version 5.4 (0x0d) revision 0x0000, manufacturer 0x05f1
[00:00:00.274,902] <inf> bt_hci_core: LMP: version 5.4 (0x0d) subver 0xffff
[00:00:00.276,672] <inf> ble: Bluetooth initialized 0
Creating new ID failed (err -12)
Bonded device: 7C:2A:DB:74:79:FA (public)
Bonded device: 40:DE:24:72:B5:9C (public)
[00:00:00.277,801] <inf> ble_server: Added following peer to whitelist: fa 79
[00:00:00.277,893] <inf> ble_server: Added following peer to whitelist: 9c b5
[00:00:00.277,893] <inf> ble_server: Advertising with accept list (2 bonded peer(s))
[00:00:00.278,442] <inf> ble_server: Advertising successfully started
[00:00:00.284,362] <wrn> net_ctx: DROP: src addr is unspecified
[00:00:00.284,393] <err> stream_client: Failed to connect: -1
[00:00:00.641,448] <inf> usb_net_mgmt: === NET interface 0x200015a4 ===
[00:00:00.641,510] <inf> usb_net_mgmt: Address: 192.0.3.2 [addr type NET_ADDR_MANUAL]
[00:00:00.641,571] <inf> usb_net_mgmt: Subnet:  255.255.255.0
[00:00:00.641,601] <inf> usb_net_mgmt: Router:  192.0.3.1
[00:00:00.641,815] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000c not supported
[00:00:00.711,364] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000e not supported
[00:00:00.712,280] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000e not supported
[00:00:01.288,299] <inf> stream_client: Connected to 192.0.3.1:4000
[00:00:01.288,940] <inf> stream_client: Firmware version 1.0.0 sent on control channel
[00:00:01.797,668] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -88)
[00:00:02.343,994] <inf> usb_ecm: Set Interface 0 Packet Filter 0x000e not supported
[00:00:03.499,969] <inf> ble_server: Connected
[00:00:03.500,518] <inf> ble_server: Added following peer to whitelist: fa 79
[00:00:03.500,579] <inf> ble_server: Added following peer to whitelist: 9c b5
[00:00:03.500,610] <inf> ble_server: Advertising with accept list (2 bonded peer(s))
[00:00:03.501,129] <inf> ble_server: Advertising successfully started
[00:00:03.599,395] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:3C:D3:21 rssi: -79 bat: 2857 mV temp: 17 °C hum: 45 %
[00:00:04.276,458] <dbg> ble_server: read_left_door: Read left door state, handle: 0, conn: 0x20002660
[00:00:04.366,455] <dbg> ble_server: read_right_door: Attribute read, handle: 0, conn: 0x20002660
[00:00:04.456,481] <dbg> ble_server: read_gate: Attribute read, handle: 0, conn: 0x20002660
[00:00:04.576,477] <dbg> ble_server: read_flags: Read firmware flags, handle: 0, conn: 0x20002660
[00:00:04.926,849] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -87)
[00:00:05.216,430] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:28:17:E3 rssi: -80 bat: 2875 mV temp: 15 °C hum: 49 %
[00:00:05.718,688] <inf> ble_server: Passkey for 40:DE:24:72:B5:9C (public): 826809

[00:00:06.188,903] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -86)
[00:00:07.025,726] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -86)
[00:00:07.327,606] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:AF:54:26 rssi: -71 bat: 3012 mV temp: 20 °C hum: 42 %
[00:00:07.556,457] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -87)
[00:00:07.978,240] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -88)
[00:00:09.662,322] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -86)
[00:00:10.181,915] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -87)
[00:00:10.906,494] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:EC:1C:6D rssi: -64 bat: 2923 mV temp: 19 °C hum: 42 %
[00:00:13.445,800] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -87)
[00:00:16.487,731] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:56:5E:CE rssi: -84 bat: 2627 mV temp: 18 °C hum: 44 %
[00:00:16.673,217] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:A7:30:C4 rssi: -86 bat: 2706 mV temp: 17 °C hum: 46 %
[00:00:16.910,308] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -87)
[00:00:20.213,775] <inf> ble_server: Security changed: 40:DE:24:72:B5:9C (public) level 4

[00:00:20.926,696] <dbg> ble_server: write_right_door: Attribute write, handle: 0, conn: 0x20002660
[00:00:21.062,194] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:21.241,638] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:21.331,604] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:21.782,104] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:21.916,625] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:22.695,495] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -87)
[00:00:22.996,612] <dbg> ble_server: write_left_door: Write left door state, handle: 0, conn: 0x20002660
[00:00:23.086,608] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:23.221,771] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:23.581,604] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:23.897,033] <dbg> ble_server: write_right_door: Attribute write, handle: 0, conn: 0x20002660
[00:00:24.076,751] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:24.841,644] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:25.021,636] <dbg> ble_server: garage_indicate_cb: Garage indication success
[00:00:25.109,588] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -86)
[00:00:25.207,855] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:28:17:E3 rssi: -80 bat: 2875 mV temp: 15 °C hum: 49 %
[00:00:26.462,493] <inf> ble_server: Disconnected. Reason 19
Connection object available from previous conn. Disconnect is complete!
[00:00:26.462,707] <wrn> bt_hci_core: opcode 0x2010 status 0x0c 
[00:00:26.462,738] <err> bt_hci_core: Failed to clear filter accept list
[00:00:26.462,738] <inf> ble_server: Cannot clear Filter Accept List (err: -13)
[00:00:26.462,768] <err> ble_server: Acceptlist setup failed (err:-13)
[00:00:26.478,088] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:56:5E:CE rssi: -69 bat: 2627 mV temp: 18 °C hum: 44 %
[00:00:26.485,290] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:8D:BA:B4 rssi: -73 bat: 2916 mV temp: 17 °C hum: 40 %
[00:00:26.657,531] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:A7:30:C4 rssi: -86 bat: 2706 mV temp: 17 °C hum: 46 %
[00:00:30.906,982] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:EC:1C:6D rssi: -62 bat: 2923 mV temp: 19 °C hum: 42 %
[00:00:32.010,833] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -90)
[00:00:36.487,182] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:8D:BA:B4 rssi: -71 bat: 2916 mV temp: 17 °C hum: 40 %
[00:00:45.195,220] <inf> xiaomi: [XIAOMI] mac: A4:C1:38:28:17:E3 rssi: -79 bat: 2875 mV temp: 15 °C hum: 49 %
[00:00:47.410,156] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -89)
[00:00:49.816,589] <inf> obv: Linky found: 2B:FC:47:A5:4F:59 (RSSI -88)
```

After 17 days of continuous operation:

```
[17:57:55.580,505] <inf> main: TIC PAPP: 1290 W IINST: 5 A (IMAX 90 A ISOUSC 30) BASE 43840006 Wh - ADCO XXXXXXXXXXXX (rx B: 156641545 N dt: 10305380 C err: 0)
[17:57:56.929,687] <inf> main: TIC PAPP: 1260 W IINST: 5 A (IMAX 90 A ISOUSC 30) BASE 43840006 Wh - ADCO XXXXXXXXXXXX (rx B: 156641673 N dt: 10305390 C err: 0)
[17:57:58.279,724] <inf> main: TIC PAPP: 1300 W IINST: 5 A (IMAX 90 A ISOUSC 30) BASE 43840007 Wh - ADCO XXXXXXXXXXXX (rx B: 156641801 N dt: 10305400 C err: 0)
[17:58:00.272,399] <inf> main: TIC PAPP: 1280 W IINST: 5 A (IMAX 90 A ISOUSC 30) BASE 43840007 Wh - ADCO XXXXXXXXXXXX (rx B: 156641993 N dt: 10305410 C err: 0)
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

## Stream Protocol

All data travels over a single TCP connection (device IP `192.0.3.2`, host port `4000`
by default). Frames are multiplexed by channel ID:

| Channel name   | Channel ID   | Direction     | Description                                                         |
| -------------- | ------------ | ------------- | ------------------------------------------------------------------- |
| Control        | `0x00000000` | device → host | Firmware version frame sent on connect                              |
| Xiaomi         | `0xfa30fa42` | device → host | Xiaomi LYWSD03MMC temperature/humidity records                      |
| Linky TIC      | `0xcd1f14bd` | device → host | Linky energy meter measurements                                     |
| BLE Control    | `0x4f154ca0` | bidirectional | BLE connection & pairing events (TX); bond management commands (RX) |
| Device Control | `0xeb5d8977` | bidirectional | Garage-door open/close commands (TX); door state updates (RX)       |

The RX path (host → device) requires `CONFIG_COPRO_STREAM_CHANNEL_RX=y`.

## BLE GATT Server

When `CONFIG_COPRO_BLE_SERVER=y` the firmware advertises a **Garage Control Service**
(`539f0000-43b5-4c29-9ea2-99a56589ca60`) with the following characteristics:

| Characteristic | UUID suffix | Properties | Description                                       |
| -------------- | ----------- | ---------- | ------------------------------------------------- |
| Left door      | `…0001`     | Indicate   | Door state (`CLOSED=0`, `OPEN=1`, `UNKNOWN=0xFF`) |
| Right door     | `…0002`     | Indicate   | Door state                                        |
| Gate           | `…0003`     | Indicate   | Gate state                                        |
| Firmware flags | `…0004`     | Notify     | Bit 0: TCP stream server connected                |

Pairing uses a 6-digit numeric passkey. Bonds are persisted via the Zephyr settings
subsystem. A configurable idle-connection timeout (`CONFIG_COPRO_BLE_IDLE_TIMEOUT_S`,
default 30 s) disconnects idle clients. The button triggers bond removal or a timed
pairing-advertising window.

## Rust Companion Library

`ble-copro-stream-server-rs` is an async Tokio library for the host side:

```toml
# Cargo.toml
ble-copro-stream-server-rs = { path = "ble-copro-stream-server-rs" }
```

```rust
use ble_copro_stream_server::{StreamServer, stream_message::ChannelMessage};

let server = StreamServer::init("192.0.3.1", 4000).await?;
let mut channel = server.accept().await?;
loop {
    match channel.next().await? {
        ChannelMessage::Xiaomi(r)        => println!("{}", r),
        ChannelMessage::LinkyTic(r)      => println!("{}", r),
        ChannelMessage::BleControl(r)    => println!("{:?}", r),
        ChannelMessage::DeviceControl(r) => println!("{:?}", r),
        _ => {}
    }
}
```

See [`ble-copro-stream-server-rs/examples/server.rs`](./ble-copro-stream-server-rs/examples/server.rs)
for a complete example including garage-door state management.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## TODOs

- **Reset or pause the idle timeout when a pairing operation is in progress**
- Use a *ring buffer* API instead of the msgq: <https://docs.zephyrproject.org/latest/doxygen/html/group__ring__buffer__apis.html#ga6c7e76e3ca798e994f738d114cb9a7e3>

## Net diag

Wireshark

   ssh pcluc sudo tcpdump -U -s0 -i zeth0 -w - | "C:\Program Files\Wireshark\Wireshark.exe" -k -i -