# USB CDC ECM

## Network configuration

Interface configuration:
- Mac address: 00:00:5e:00:53:00
- Static IP configuration
- IP address: 192.0.3.2
- Netmask: 255.255.255.0
- Gateway: 192.0.3.1

## Configuration on Linux host (Fedora 37)

On Linux host (Fedora in my case), some configuration is required to use the 
USB network interface when plugged.

Create file `/etc/udev/rules.d/50-zephyr-usb.rules` with the following content:

```
# mac address should be lower case
SUBSYSTEM=="net", ACTION=="add", ATTR{address}=="00:00:5e:00:53:01", NAME="zeth0", RUN+="/etc/udev/scripts/configure_zeth0"
```

This rule will match the USB network interface and run the script 
`/etc/udev/scripts/configure_zeth0` to configure it.

Create script `/etc/udev/scripts/configure_zeth0` with the following content:

```
#!/bin/sh
/usr/sbin/ip link set zeth0 down
/usr/sbin/ip addr add dev zeth0 192.0.3.1/24
/usr/sbin/ip link set zeth0 up
```

Make the interface not managed by NetworkManager by adding the following content
to `/etc/NetworkManager/NetworkManager.conf`:

```
[keyfile]
unmanaged-devices=interface-name:zeth0
```

Restart NetworkManager:

```
sudo systemctl restart NetworkManager
```


Reload udev rules:

```
sudo udevadm control --reload
```

## Expected result

Interface should look like this:

```
zeth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 192.0.3.1  netmask 255.255.255.0  broadcast 0.0.0.0
        inet6 fe80::200:5eff:fe00:5301  prefixlen 64  scopeid 0x20<link>
        ether 00:00:5e:00:53:01  txqueuelen 1000  (Ethernet)
        RX packets 6  bytes 408 (408.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 25  bytes 3293 (3.2 KiB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```

The controller should be pingable:

```
(.venv) [lucas@fedora zephyr-caniot-controller]$ ping 192.0.3.2
PING 192.0.3.2 (192.0.3.2) 56(84) bytes of data.
64 bytes from 192.0.3.2: icmp_seq=1 ttl=64 time=3.61 ms
```

---

## Yocto integration

In directory `recipes-support/usb-ethernet/`, create the following files:

- `usb-ethernet.bb`

```bash
DESCRIPTION = "udev rules for ble coprocessor ethernet interface over USB"
LICENSE = "CLOSED"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "file:///usb-ethernet.rules \
        file:///configure-iface \`
        "

do_install() {
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/usb-ethernet.rules ${D}${sysconfdir}/udev/rules.d/usb-ethernet.rules

    install -d ${D}${sysconfdir}/udev/scripts
    install -m 0755 ${WORKDIR}/configure-iface ${D}${sysconfdir}/udev/scripts/configure-iface
}
```

- `files/usb-ethernet.rules`

```
# mac address should be lower case
SUBSYSTEM=="net", ACTION=="add", ATTR{address}=="00:00:5e:00:53:01", NAME="zeth0", RUN+="/etc/udev/scripts/configure-iface"
```

- `configure-iface`

```bash
#!/bin/sh
/sbin/ip link set zeth0 down
/sbin/ip addr add dev zeth0 192.0.3.1/24
/sbin/ip link set zeth0 up
```