use ble_copro_stream_server::{
    StreamChannelError, StreamServer, device_control::{DeviceCtrlCmd, DeviceCtrlStateMsg, DoorState, GarageDoorsState}, stream_message::ChannelMessage
};

#[derive(Debug, Clone, Default)]
struct GarageDoorController {
    state: GarageDoorsState,
}

impl GarageDoorController {

    fn get_initial_state(&self) -> DeviceCtrlStateMsg {
        DeviceCtrlStateMsg::GarageDoors(self.state.clone())
    }

    // Handle a device control command and update the garage door state accordingly.
    // Return an updated state if the command results in a state change that should be sent back to the device.
    fn handle_command(&mut self, cmd: DeviceCtrlCmd) -> Option<DeviceCtrlStateMsg> {
        match cmd {
            DeviceCtrlCmd::OpenLeftGarageDoor => {
                println!("Opening left garage door...");
                if self.state.left_door == DoorState::Closed {
                    self.state.left_door = DoorState::Open;
                    return Some(DeviceCtrlStateMsg::GarageDoors(self.state.clone()));
                } else if self.state.left_door == DoorState::Open {
                    self.state.left_door = DoorState::Closed;
                    return Some(DeviceCtrlStateMsg::GarageDoors(self.state.clone()));
                }
            }
            DeviceCtrlCmd::OpenRightGarageDoor => {
                println!("Opening right garage door...");
                if self.state.right_door == DoorState::Closed {
                    self.state.right_door = DoorState::Open;
                    return Some(DeviceCtrlStateMsg::GarageDoors(self.state.clone()));
                } else if self.state.right_door == DoorState::Open {
                    self.state.right_door = DoorState::Closed;
                    return Some(DeviceCtrlStateMsg::GarageDoors(self.state.clone()));
                }
            }
        }
        None
    }
}

#[tokio::main]
async fn main() {
    let server = StreamServer::init("192.0.3.1", 4000)
        .await
        .expect("Failed to start server");

    let mut garage_controller = GarageDoorController::default();

    loop {
        let mut channel = server.accept().await.expect("Failed to accept connection");

        channel.send_indication(garage_controller.get_initial_state()).await.expect("Failed to send initial state");

        loop {
            match channel.next().await {
                Ok(message) => match message {
                    ChannelMessage::Xiaomi(record) => {
                        println!("Xiaomi record: {}", record);
                    }
                    ChannelMessage::LinkyTic(record) => {
                        println!("LinkyTic record: {}", record);
                    }
                    ChannelMessage::BleControl(ctrl) => {
                        println!("BLE control message: {:?}", ctrl);
                    }
                    ChannelMessage::DeviceControl(cmd) => {
                        println!("Device control command: {:?}", cmd);
                        if let Some(new_state) = garage_controller.handle_command(cmd.cmd) {
                            println!("Updated garage door state: {:?}", new_state);
                            channel
                                .send_indication(new_state)
                                .await
                                .expect("Failed to send indication");
                        }
                    }
                    _ => {
                        eprintln!("Unhandled message");
                    }
                },
                Err(StreamChannelError::UnhandledChannelId(channel_id)) => {
                    eprintln!("Unhandled channel ID: 0x{:x}", channel_id);
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
