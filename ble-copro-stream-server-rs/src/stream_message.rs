use crate::{control_channel::ControlMessage, linky::LinkyTicRecord, xiaomi::XiaomiRecord};

#[derive(Debug)]
pub struct MessageHeader {
    pub channel_id: u32,
    pub message_len: u16,
}

impl MessageHeader {
    pub fn new(channel_id: u32, message_len: u16) -> MessageHeader {
        MessageHeader {
            channel_id,
            message_len,
        }
    }
}

#[derive(Debug)]
pub enum ChannelMessage {
    Xiaomi(XiaomiRecord),
    LinkyTic(LinkyTicRecord),
    Control(ControlMessage),
}
