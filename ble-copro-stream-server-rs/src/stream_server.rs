use crate::stream_channel::StreamChannel;
use std::net::SocketAddrV4;
use thiserror::Error;
use tokio::net::TcpListener;

pub const DEFAULT_LISTEN_IP: &str = "192.0.3.2";
pub const DEFAULT_LISTEN_PORT: u16 = 4000;

#[derive(Error, Debug)]
pub enum ServerError {
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    #[error("Invalid IP address")]
    InvalidIpAddress,
}

pub struct StreamServer {
    listener: TcpListener,
}

impl StreamServer {
    pub async fn init(ip: &str, port: u16) -> Result<StreamServer, ServerError> {
        let ip = ip.parse().map_err(|_| ServerError::InvalidIpAddress)?;
        let addr = SocketAddrV4::new(ip, port);
        let listener = TcpListener::bind(addr).await?;
        Ok(StreamServer { listener })
    }

    pub async fn accept(&self) -> Result<StreamChannel, ServerError> {
        let (stream, _addr) = self.listener.accept().await?;
        Ok(StreamChannel::from(stream))
    }
}
