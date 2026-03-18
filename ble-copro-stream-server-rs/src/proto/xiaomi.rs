use std::fmt::Display;

use byteorder::{ByteOrder, LittleEndian};

use crate::{
    ble::BleAddress, stream_channel::StreamChannelError, timestamp::Timestamp, StreamChannelHandler,
};

//  xiaomi: [XIAOMI] mac: A4:C1:38:EC:1C:6D rssi: -33 bat: 3016 mV temp: 16 °C hum: 42 %
#[derive(Debug)]
pub struct XiaomiMeasurement {
    pub rssi: i8,
    pub temperature: f32,
    pub humidity: f32,
    pub battery_mv: u16,
    pub battery_percent: u8,
}

impl Display for XiaomiMeasurement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "rssi: {} bat: {} mV ({} %) temp: {} °C hum: {} %",
            self.rssi, self.battery_mv, self.battery_percent, self.temperature, self.humidity
        )
    }
}

#[derive(Debug)]
pub struct XiaomiRecord {
    pub version: u8,
    pub ble_addr: BleAddress,
    pub timestamp: Timestamp,
    pub measurement: XiaomiMeasurement,
}

impl Display for XiaomiRecord {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "mac: {} timestamp: {} {}",
            self.ble_addr, self.timestamp, self.measurement
        )
    }
}

pub struct XiaomiHandler;

const XIAOMI_RECORD_SIZE: usize = 24;

impl StreamChannelHandler for XiaomiHandler {
    const CHANNEL_ID: u32 = 0xfa30fa42;
    type Message = XiaomiRecord;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError> {
        if data.len() < XIAOMI_RECORD_SIZE {
            return Err(StreamChannelError::InvalidMessageLength);
        }

        let mut ble_mac = [0; 6];
        ble_mac.copy_from_slice(&data[0..6]);

        let ble_type = data[6];
        let rssi = data[7] as i8;
        let version = data[8];
        let timestamp = Timestamp::Uptime(LittleEndian::read_i64(&data[9..17]) as u64);
        let temperature = LittleEndian::read_i16(&data[17..19]) as f32 / 100.0;
        let humidity = LittleEndian::read_u16(&data[19..21]) as f32 / 100.0;
        let battery_mv = LittleEndian::read_u16(&data[21..23]);
        let battery_percent = data[23];

        let ble_addr = BleAddress::new(ble_mac, ble_type);

        Ok(XiaomiRecord {
            version,
            ble_addr,
            timestamp,
            measurement: XiaomiMeasurement {
                rssi,
                temperature,
                humidity,
                battery_mv,
                battery_percent,
            },
        })
    }
}
