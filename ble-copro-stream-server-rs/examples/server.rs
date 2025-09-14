use ble_copro_stream_server::{stream_message::ChannelMessage, StreamChannelError, StreamServer};

#[tokio::main]
async fn main() {
    let server = StreamServer::init("192.0.3.1", 4000)
        .await
        .expect("Failed to start server");

    loop {
        let mut channel = server.accept().await.expect("Failed to accept connection");

        loop {
            match channel.next().await {
                Ok(message) => match message {
                    ChannelMessage::Xiaomi(record) => {
                        println!("Xiaomi record: {}", record);
                    }
                    ChannelMessage::LinkyTic(record) => {
                        println!("LinkyTic record: {}", record);
                    }
                    _ => {
                        eprintln!("Unhandled message");
                    }
                },
                Err(StreamChannelError::UnhandledChannelId) => {
                    eprintln!("Unhandled channel ID");
                }
                Err(e) => {
                    eprintln!("Error: {}", e);
                    break;
                }
            }
        }

        println!("Connection closed");
    }
}
