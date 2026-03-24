use crate::{StreamChannelHandler, StreamChannelIndication, stream_channel::StreamChannelError};

pub const CHANNEL_NAME: &str = "device-control";

// ---------------------------------------------------------------------------
// TX: Commands received from the device (device → server)
// ---------------------------------------------------------------------------

/// Command types sent by the device over the stream channel.
///
/// Wire encoding: 1 byte.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum DeviceCtrlCmd {
    OpenLeftGarageDoor = 0x01,
    OpenRightGarageDoor = 0x02,
}

/// Parsed command message (device → server).
///
/// Wire layout:
///   - byte 0: command type (`DeviceCtrlCmd`)
#[derive(Debug, Clone)]
pub struct DeviceCtrlCommandMsg {
    pub cmd: DeviceCtrlCmd,
}

pub struct DeviceControlHandler;

impl StreamChannelHandler for DeviceControlHandler {
    const CHANNEL_ID: u32 = 0xeb5d8977;
    type Message = DeviceCtrlCommandMsg;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError> {
        if data.is_empty() {
            return Err(StreamChannelError::InvalidMessageLength);
        }

        let cmd = match data[0] {
            0x01 => DeviceCtrlCmd::OpenLeftGarageDoor,
            0x02 => DeviceCtrlCmd::OpenRightGarageDoor,
            _ => return Err(StreamChannelError::InvalidMessageData),
        };

        Ok(DeviceCtrlCommandMsg { cmd })
    }
}

// ---------------------------------------------------------------------------
// RX: State messages sent by the server to the device (server → device)
// ---------------------------------------------------------------------------

/// Per-door / gate position state.
///
/// Wire encoding: 1 byte.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum DoorState {
    #[default]
    Closed = 0x00,
    Open = 0x01,
    Unknown = 0xFF,
}

/// State of all three garage doors / gate.
///
/// Wire layout (3 bytes): left_door, gate, right_door.
#[derive(Debug, Clone, Default)]
pub struct GarageDoorsState {
    pub left_door: DoorState,
    pub gate: DoorState,
    pub right_door: DoorState,
}

const STATE_TYPE_GARAGE_DOORS: u8 = 0x01;

/// Full state frame sent by the server to the device.
///
/// Wire layout:
///   - byte 0:   state type tag
///   - bytes 1…: state payload (depends on tag)
#[derive(Debug, Clone)]
pub enum DeviceCtrlStateMsg {
    GarageDoors(GarageDoorsState),
}

impl DeviceCtrlStateMsg {
    /// Serialize into `buf`.
    ///
    /// Returns the number of bytes written, or `InvalidMessageLength` if
    /// `buf` is too small.
    pub fn serialize(&self, buf: &mut [u8]) -> Result<usize, StreamChannelError> {
        match self {
            DeviceCtrlStateMsg::GarageDoors(state) => {
                // 1 byte type tag + 3 bytes payload
                if buf.len() < 4 {
                    return Err(StreamChannelError::InvalidMessageLength);
                }
                buf[0] = STATE_TYPE_GARAGE_DOORS;
                buf[1] = state.left_door as u8;
                buf[2] = state.gate as u8;
                buf[3] = state.right_door as u8;
                Ok(4)
            }
        }
    }

    /// Serialize into a freshly allocated `Vec<u8>`.
    pub fn to_bytes(&self) -> Vec<u8> {
        // Largest current payload: 4 bytes
        let mut buf = vec![0u8; 4];
        let n = self
            .serialize(&mut buf)
            .expect("buffer is always large enough");
        buf.truncate(n);
        buf
    }
}

impl StreamChannelIndication for DeviceCtrlStateMsg {
    const CHANNEL_ID: u32 = DeviceControlHandler::CHANNEL_ID;

    fn serialize_indication(&self) -> Vec<u8> {
        self.to_bytes()
    }
}