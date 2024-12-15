use crate::{stream_channel::StreamChannelError, StreamChannelHandler};

pub struct ControlHandler;

#[derive(Debug, Default)]
pub struct ControlMessage;

impl StreamChannelHandler for ControlHandler {
    const CHANNEL_ID: u32 = 0x00000000;
    type Message = ControlMessage;
    fn parse_message(_data: &[u8]) -> Result<Self::Message, StreamChannelError> {
        Ok(ControlMessage)
    }
}
