use std::fmt::Display;

use byteorder::{ByteOrder, LittleEndian};

use crate::{
    stream_channel::StreamChannelError, stream_message::MessageHeader, StreamChannelHandler,
};

#[derive(Debug)]
pub struct XiaomiMeasurement {
    pub rssi: i8,
    pub temperature: f32,
    pub humidity: f32,
    pub battery_mv: u16,
    pub battery_percent: u8,
}

#[derive(Debug)]
pub struct XiaomiRecord {
    pub ble_mac: [u8; 6],
    pub ble_type: u8,
    pub timestamp: i64,
    pub measurement: XiaomiMeasurement,
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
        let timestamp = LittleEndian::read_i64(&data[9..17]);
        let temperature = LittleEndian::read_i16(&data[17..19]) as f32 / 100.0;
        let humidity = LittleEndian::read_u16(&data[19..21]) as f32 / 100.0;
        let battery_mv = LittleEndian::read_u16(&data[21..23]);
        let battery_percent = data[23];

        Ok(XiaomiRecord {
            ble_mac,
            ble_type,
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

impl Display for XiaomiRecord {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "XiaomiRecord {{ MAC {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x} [type {}] timestamp: {} rssi: {} dBm temperature: {} °C humidity: {}, battery: {} mv ({} %) }}",
            self.ble_mac[0], self.ble_mac[1], self.ble_mac[2], self.ble_mac[3], self.ble_mac[4], self.ble_mac[5],
            self.ble_type,
            self.timestamp,
            self.measurement.rssi,
            self.measurement.temperature,
            self.measurement.humidity,
            self.measurement.battery_mv,
            self.measurement.battery_percent
        )
    }
}
