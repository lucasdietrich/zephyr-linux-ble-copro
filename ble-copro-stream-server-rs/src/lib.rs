pub mod ble;
pub mod control_channel;
pub mod linky;
pub mod stream_channel;
pub mod stream_message;
pub mod stream_server;
pub mod timestamp;
pub mod xiaomi;

pub use stream_channel::StreamChannelError;
pub use stream_server::{ServerError, StreamServer, DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT};
pub use timestamp::Timestamp;

pub trait StreamChannelHandler {
    const CHANNEL_ID: u32;
    type Message;

    fn parse_message(data: &[u8]) -> Result<Self::Message, StreamChannelError>;
}
