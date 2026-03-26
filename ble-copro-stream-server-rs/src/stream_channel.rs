use thiserror::Error;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

use crate::ble_control::BleControlHandler;
use crate::control_channel::ControlHandler;
use crate::device_control::DeviceControlHandler;
use crate::linky::LinkyTicHandler;
use crate::stream_message::{ChannelMessage, MessageHeader};
use crate::xiaomi::XiaomiHandler;
use crate::{StreamChannelHandler, StreamChannelIndication};

pub struct StreamChannel {
    stream: TcpStream,
}

#[derive(Error, Debug)]
pub enum StreamChannelError {
    #[error("Invalid message header")]
    InvalidMessageHeader,
    #[error("Invalid message data")]
    InvalidMessageData,
    #[error("Invalid message Length")]
    InvalidMessageLength,
    #[error("Unhandled channel ID: {0}")]
    UnhandledChannelId(u32),
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
}

impl StreamChannel {
    pub(crate) fn from(stream: TcpStream) -> StreamChannel {
        StreamChannel { stream }
    }

    fn parse_message_header(&mut self, data: &[u8]) -> Result<MessageHeader, StreamChannelError> {
        if data.len() < 6 {
            return Err(StreamChannelError::InvalidMessageHeader);
        }

        let channel_id = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
        let message_len = u16::from_le_bytes([data[4], data[5]]);

        Ok(MessageHeader::new(channel_id, message_len))
    }

    async fn read_next_message(&mut self) -> Result<(MessageHeader, Vec<u8>), StreamChannelError> {
        let mut header_buf = [0; 6];
        self.stream.read_exact(&mut header_buf).await?;

        let header = self.parse_message_header(&header_buf)?;

        let mut data = vec![0; header.message_len as usize];
        self.stream.read_exact(&mut data).await?;

        Ok((header, data))
    }

    pub async fn next(&mut self) -> Result<ChannelMessage, StreamChannelError> {
        let (header, data) = self.read_next_message().await?;

        let data = data.as_slice();

        match header.channel_id {
            XiaomiHandler::CHANNEL_ID => {
                XiaomiHandler::parse_message(data).map(ChannelMessage::Xiaomi)
            }
            LinkyTicHandler::CHANNEL_ID => {
                LinkyTicHandler::parse_message(data).map(ChannelMessage::LinkyTic)
            }
            ControlHandler::CHANNEL_ID => {
                ControlHandler::parse_message(data).map(ChannelMessage::Control)
            }
            BleControlHandler::CHANNEL_ID => {
                BleControlHandler::parse_message(data).map(ChannelMessage::BleControl)
            }
            DeviceControlHandler::CHANNEL_ID => {
                DeviceControlHandler::parse_message(data).map(ChannelMessage::DeviceControl)
            }
            _ => Err(StreamChannelError::UnhandledChannelId(header.channel_id)),
        }
    }

    pub async fn send_indication<I: StreamChannelIndication>(
        &mut self,
        indication: I,
    ) -> Result<(), StreamChannelError> {
        let data = indication.serialize_indication();
        let header = MessageHeader::new(I::CHANNEL_ID, data.len() as u16);

        let mut buf = Vec::with_capacity(6 + data.len());
        buf.extend_from_slice(&header.channel_id.to_le_bytes());
        buf.extend_from_slice(&header.message_len.to_le_bytes());
        buf.extend_from_slice(&data);

        self.stream.write_all(&buf).await?;

        Ok(())
    }
}
