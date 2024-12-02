use crate::stream_channel::StreamChannel;
use std::net::{Ipv4Addr, SocketAddrV4};
use thiserror::Error;
use tokio::net::TcpListener;

pub const DEFAULT_LISTEN_IP: Ipv4Addr = Ipv4Addr::new(192, 0, 3, 1);
pub const DEFAULT_LISTEN_PORT: u16 = 12345;

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
        let addr = SocketAddrV4::new(ip.parse().map_err(|_| ServerError::InvalidIpAddress)?, port);
        let listener = TcpListener::bind(addr).await?;
        Ok(StreamServer { listener })
    }

    pub async fn accept(&self) -> Result<StreamChannel, ServerError> {
        let (stream, addr) = self.listener.accept().await?;

        println!("New connection: {}", addr);

        Ok(StreamChannel::from(stream))
    }
}
