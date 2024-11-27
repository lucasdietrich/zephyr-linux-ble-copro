# BLE Xiaomi Mijia measurements collector application

This application scans and retrieves measurements from Xiaomi Mijia `LYWSD03MMC` 
devices, which are Bluetooth Low Energy (BLE) devices.

## Getting started

You will need :
- Xiaomi Mijia `LYWSD03MMC` devices with ~~firmware version `1.0.0_0130`~~ custom firmware (https://github.com/pvvx/ATC_MiThermometer) version 3.7.
  - Firmware can be upgraded using ~~"Xiaomi Home" app~~ [Telink Flasher for Mi Thermostat](https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html) (select subcategory : `OTA Flasher`).
  - Use default settings of the custom firmware.

## Measurements

Following measurements are periodically collected from Xiaomi devices are :

| Symbol | Measurement     | Unit | Maximum resolution |
| ------ | --------------- | ---- | ------------------ |
| T      | Temperature     | °C   | 1,00E-02           |
| H      | Humidity        | %    | 1,00E+00           |
| Bl     | Battery Level   | %    | 1,00E+00           |
| Bv     | Battery Voltage | V    | 1,00E-02           |

## Sources / links :

- [**Xiaomi Mijia LYWSD03MMC : Récupérer les données du capteur sur un Raspberry Pi avec gatttool**](https://www.fanjoe.be/?p=3911)