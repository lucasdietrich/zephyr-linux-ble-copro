use thiserror::Error;
use tokio::io::AsyncReadExt;
use tokio::net::TcpStream;

use crate::control_channel::ControlHandler;
use crate::stream_message::{ChannelMessage, MessageHeader};
use crate::xiaomi::XiaomiHandler;
use crate::StreamChannelHandler;

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
    #[error("Unhandled channel ID")]
    UnhandledChannelId,
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
            ControlHandler::CHANNEL_ID => {
                ControlHandler::parse_message(data).map(ChannelMessage::Control)
            }
            _ => Err(StreamChannelError::UnhandledChannelId),
        }
    }
}
