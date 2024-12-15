use crate::stream_channel::StreamChannel;
use std::net::SocketAddrV4;
#[cfg(feature = "tcp-keep-alive")]
use std::{ffi::c_int, ffi::c_void, os::fd::AsRawFd};
use thiserror::Error;
use tokio::net::TcpListener;

pub const DEFAULT_LISTEN_IP: &str = "192.0.3.2";
pub const DEFAULT_LISTEN_PORT: u16 = 4000;

#[cfg(feature = "tcp-keep-alive")]
const KEEP_ALIVE_CNT: c_int = 1;
#[cfg(feature = "tcp-keep-alive")]
const KEEP_ALIVE_IDLE: c_int = 5;
#[cfg(feature = "tcp-keep-alive")]
const KEEP_ALIVE_INTVL: c_int = 1;

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

    #[cfg(feature = "tcp-keep-alive")]
    fn setsockopt(fd: i32, level: i32, optname: i32, optval: c_int) -> Result<(), ServerError> {
        let ret = unsafe {
            libc::setsockopt(
                fd,
                level,
                optname,
                &optval as *const _ as *const c_void,
                std::mem::size_of_val(&optval) as libc::socklen_t,
            )
        };

        if ret < 0 {
            return Err(ServerError::IoError(std::io::Error::last_os_error()));
        }

        Ok(())
    }

    #[cfg(feature = "tcp-keep-alive")]
    fn configure_keep_alive(fd: i32) -> Result<(), ServerError> {
        use libc::{
            IPPROTO_TCP, SOL_SOCKET, SO_KEEPALIVE, TCP_KEEPCNT, TCP_KEEPIDLE, TCP_KEEPINTVL,
        };

        Self::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, 1)?;
        Self::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, KEEP_ALIVE_IDLE)?;
        Self::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, KEEP_ALIVE_INTVL)?;
        Self::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, KEEP_ALIVE_CNT)?;

        Ok(())
    }

    pub async fn accept(&self) -> Result<StreamChannel, ServerError> {
        let (stream, _addr) = self.listener.accept().await?;

        #[cfg(feature = "tcp-keep-alive")]
        Self::configure_keep_alive(stream.as_raw_fd())?;

        Ok(StreamChannel::from(stream))
    }
}
