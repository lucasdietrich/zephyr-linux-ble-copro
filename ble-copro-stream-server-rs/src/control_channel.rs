use crate::{stream_channel::StreamChannelError, StreamChannelHandler};

pub const CHANNEL_NAME: &str = "control";

/// Control channel message type tag (byte 0 of every control payload).
///
/// Wire encoding: 1 byte.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ControlMsgType {
    FirmwareVersion = 0x01,
}

/// Firmware version event sent by the device on every new connection.
///
/// Wire layout (bytes 1–3 of the control payload):
///   - byte 1: major
///   - byte 2: minor
///   - byte 3: patch
#[derive(Debug, Clone)]
pub struct FirmwareVersion {
    pub major: u8,
    pub minor: u8,
    pub patch: u8,
}

impl FirmwareVersion {
    pub fn as_semver_string(&self) -> String {
        format!("{}.{}.{}", self.major, self.minor, self.patch)
    }
}

impl std::fmt::Display for FirmwareVersion {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}

/// Parsed message for the control channel (channel ID `0x00000000`).
///
/// The control channel is reserved for out-of-band device events that do not
/// belong to any application-specific channel. It is always active — the device
/// sends a [`ControlMessage::FirmwareVersion`] event immediately after each
/// successful TCP connection.
#[derive(Debug)]
pub enum ControlMessage {
    FirmwareVersion(FirmwareVersion),
}

pub struct ControlHandler;

impl StreamChannelHandler for ControlHandler {
    const CHANNEL_ID: u32 = 0x00000000;
    type Message = ControlMessage;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError> {
        if data.is_empty() {
            return Err(StreamChannelError::InvalidMessageLength);
        }

        match data[0] {
            0x01 => {
                // FirmwareVersion: needs 3 more bytes (major, minor, patch)
                if data.len() < 4 {
                    return Err(StreamChannelError::InvalidMessageLength);
                }
                Ok(ControlMessage::FirmwareVersion(FirmwareVersion {
                    major: data[1],
                    minor: data[2],
                    patch: data[3],
                }))
            }
            _ => Err(StreamChannelError::InvalidMessageData),
        }
    }
}
