use std::fmt::Display;

use byteorder::{ByteOrder, LittleEndian};

use crate::{ble::BleAddress, StreamChannelError, StreamChannelHandler, Timestamp};

#[derive(Debug)]
pub struct LinkyTicMeasurements {
    pub base: u32,
    pub iinst: u16,
    pub ptec: u16,
    pub papp: u32,
}

impl Display for LinkyTicMeasurements {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "papp: {} W base: {} Wh iinst: {} A ptec: {}",
            self.papp, self.base, self.iinst, self.ptec
        )
    }
}

#[derive(Debug)]
pub struct LinkyTicInfos {
    pub acdo: String,
    pub imax: u16,
    pub isousc: u16,
}

impl Display for LinkyTicInfos {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "acdo: {} imax: {} isousc: {}",
            self.acdo, self.imax, self.isousc
        )
    }
}

#[derive(Debug)]
pub struct LinkyTicRecord {
    pub version: u8,
    pub ble_addr: BleAddress,
    pub timestamp: Timestamp,
    pub rssi: i8,
    pub infos: Option<LinkyTicInfos>,
    pub measurement: LinkyTicMeasurements,
    pub flags: u32,
}

impl Display for LinkyTicRecord {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "mac: {} timestamp: {} rssi: {} {}",
            self.ble_addr, self.timestamp, self.rssi, self.measurement
        )?;
        if let Some(infos) = &self.infos {
            write!(f, " {}", infos)?;
        }
        Ok(())
    }
}

pub struct LinkyTicHandler;

const LINKY_TIC_RECORD_SIZE: usize = 85;

impl StreamChannelHandler for LinkyTicHandler {
    const CHANNEL_ID: u32 = 0xcd1f14bd;
    type Message = LinkyTicRecord;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError> {
        if data.len() < LINKY_TIC_RECORD_SIZE {
            return Err(StreamChannelError::InvalidMessageLength);
        }

        let mut ble_mac = [0; 6];
        ble_mac.copy_from_slice(&data[0..6]);
        let ble_type = data[6];
        let ble_addr = BleAddress::new(ble_mac, ble_type);
        let rssi = data[7] as i8;
        let version = data[8];
        let flags = u32::from_le_bytes([data[9], data[10], data[11], data[12]]);
        let timestamp = Timestamp::Uptime(LittleEndian::read_i64(&data[13..21]) as u64);
        let raw = &data[21..];

        let base = LittleEndian::read_u32(&raw[1..5]);
        let iinst = LittleEndian::read_u16(&raw[5..7]);
        let ptec = LittleEndian::read_u16(&raw[7..9]);
        let papp = LittleEndian::read_u32(&raw[9..13]);

        Ok(LinkyTicRecord {
            version,
            ble_addr,
            timestamp,
            rssi,
            measurement: LinkyTicMeasurements {
                base,
                iinst,
                ptec,
                papp,
            },
            infos: None,
            flags,
        })
    }
}
