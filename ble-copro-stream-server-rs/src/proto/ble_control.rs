use std::fmt::Display;

use crate::{StreamChannelError, StreamChannelHandler, StreamChannelIndication, ble::BleAddress};

#[derive(Debug, Clone)]
pub enum PairingMessage {
    PairingCode { code: u32 },
    PairingCancelled,
    PairingSucceeded,
}

impl Display for PairingMessage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PairingMessage::PairingCode { code } => write!(f, "pairing code: {}", code),
            PairingMessage::PairingCancelled => write!(f, "pairing cancelled"),
            PairingMessage::PairingSucceeded => write!(f, "pairing succeeded"),
        }
    }
}

#[derive(Debug, Clone)]
pub enum ConnectionMessage {
    Connected,
    Disconnected,
}

impl Display for ConnectionMessage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConnectionMessage::Connected => write!(f, "connected"),
            ConnectionMessage::Disconnected => write!(f, "disconnected"),
        }
    }
}

#[derive(Debug, Clone)]
pub enum BleControlMessage {
    Connection(ConnectionMessage),
    Pairing(PairingMessage),
}

impl Display for BleControlMessage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BleControlMessage::Connection(conn_msg) => write!(f, "connection event: {}", conn_msg),
            BleControlMessage::Pairing(pairing_msg) => write!(f, "pairing event: {}", pairing_msg),
        }
    }
}

#[derive(Debug, Clone)]
pub struct BleControlPayload {
    pub addr: BleAddress,
    pub message: BleControlMessage,
}

impl Display for BleControlPayload {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "addr: {}, message: {}", self.addr, self.message)
    }
}

pub struct BleControlHandler;

const BLE_CONTROL_RECORD_MIN_SIZE: usize = 4 + 7; // cmd (4 bytes) + minimum addr length (7 bytes for "xx:xx:xx:xx:xx:xx")

const BLE_CONTROL_CMD_CONNECTED: u32 = 0x01;
const BLE_CONTROL_CMD_DISCONNECTED: u32 = 0x02;
const BLE_CONTROL_CMD_PAIRING_CODE: u32 = 0x03;
const BLE_CONTROL_CMD_PAIRING_RESULT: u32 = 0x04;

impl StreamChannelHandler for BleControlHandler {
    const CHANNEL_ID: u32 = 0x4f154ca0;
    type Message = BleControlPayload;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError> {
        if data.len() < BLE_CONTROL_RECORD_MIN_SIZE {
            return Err(StreamChannelError::InvalidMessageLength);
        }

        let cmd = u32::from_le_bytes(data[0..4].try_into().unwrap());
        let addr = BleAddress::from_raw(&data[4..11]).unwrap();

        let message = match cmd {
            BLE_CONTROL_CMD_CONNECTED => {
                BleControlMessage::Connection(ConnectionMessage::Connected)
            }
            BLE_CONTROL_CMD_DISCONNECTED => {
                BleControlMessage::Connection(ConnectionMessage::Disconnected)
            }
            BLE_CONTROL_CMD_PAIRING_CODE => {
                BleControlMessage::Pairing(PairingMessage::PairingCode {
                    code: u32::from_le_bytes(data[11..15].try_into().unwrap()),
                })
            }
            BLE_CONTROL_CMD_PAIRING_RESULT => {
                let success_code = u32::from_le_bytes(data[11..15].try_into().unwrap());
                match success_code {
                    0 => BleControlMessage::Pairing(PairingMessage::PairingSucceeded),
                    _ => BleControlMessage::Pairing(PairingMessage::PairingCancelled),
                }
            }
            _ => return Err(StreamChannelError::InvalidMessageData),
        };

        Ok(BleControlPayload { addr, message })
    }
}

#[derive(Debug, Clone)]
pub enum BleControlAction {
    RemoveAllBonds, // None = remove all binds
    RemoveBond(BleAddress), // None = remove all binds
}

const BLE_CONTROL_CMD_REMOVE_ALL_BINDS: u32 = 0xFFFFFFFF;
const BLE_CONTROL_CMD_REMOVE_BIND: u32 = 0xFFFFFFFE;

impl StreamChannelIndication for BleControlAction {
    const CHANNEL_ID: u32 = BleControlHandler::CHANNEL_ID;

    fn serialize_indication(&self) -> Vec<u8> {
        match self {
            BleControlAction::RemoveAllBonds => {
                let mut data = Vec::with_capacity(4);
                data.extend_from_slice(&BLE_CONTROL_CMD_REMOVE_ALL_BINDS.to_le_bytes()); // cmd for "remove all binds"
                data
            }
            BleControlAction::RemoveBond(addr) => {
                let mut data = Vec::with_capacity(4 + 7);
                data.extend_from_slice(&BLE_CONTROL_CMD_REMOVE_BIND.to_le_bytes()); // cmd for "remove bind"
                data.extend_from_slice(&addr.serialize()); // addr data
                data
            }
        }
    }
}