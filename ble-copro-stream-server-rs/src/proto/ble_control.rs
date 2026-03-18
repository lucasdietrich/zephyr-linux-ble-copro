use crate::{ble::BleAddress, StreamChannelError, StreamChannelHandler};

#[derive(Debug, Clone)]
pub enum PairingMessage {
    PairingCode { code: u32 },
    PairingCancelled,
    PairingSucceeded,
}

#[derive(Debug, Clone)]
pub enum ConnectionMessage {
    Connected,
    Disconnected,
}

#[derive(Debug, Clone)]
pub enum BleControlMessage {
    Connection(ConnectionMessage),
    Pairing(PairingMessage),
}

#[derive(Debug, Clone)]
pub struct BleControlPayload {
    addr: BleAddress,
    message: BleControlMessage,
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
