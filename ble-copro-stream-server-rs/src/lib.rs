pub mod stream_channel;
pub mod stream_message;
pub mod stream_server;
pub mod xiaomi;

use stream_channel::StreamChannelError;
pub use stream_server::{StreamServer, ServerError, DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT};

pub trait StreamChannelHandler {
    const CHANNEL_ID: u32;
    type Message;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError>;
}
