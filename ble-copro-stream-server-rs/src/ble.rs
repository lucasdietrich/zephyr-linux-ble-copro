use std::fmt::Display;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum BleType {
    Public = 0,
    Random = 1,
    PublicStatic = 2,
    RandomStatic = 3,
    Unknown = 255,
}

impl Display for BleType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BleType::Public => write!(f, "Public"),
            BleType::Random => write!(f, "Random"),
            BleType::PublicStatic => write!(f, "PublicStatic"),
            BleType::RandomStatic => write!(f, "RandomStatic"),
            BleType::Unknown => write!(f, "Unknown"),
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct BleAddress {
    pub mac: [u8; 6],
    pub ble_type: BleType,
}

impl BleAddress {
    pub fn new(mac: [u8; 6], ble_type: u8) -> BleAddress {
        let ble_type = match ble_type {
            0 => BleType::Public,
            1 => BleType::Random,
            2 => BleType::PublicStatic,
            3 => BleType::RandomStatic,
            _ => BleType::Unknown,
        };

        BleAddress { mac, ble_type }
    }

    pub fn mac_string(&self) -> String {
        format!(
            "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            self.mac[0], self.mac[1], self.mac[2], self.mac[3], self.mac[4], self.mac[5]
        )
    }

    pub fn to_slug(&self) -> String {
        format!(
            "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
            self.mac[0], self.mac[1], self.mac[2], self.mac[3], self.mac[4], self.mac[5]
        )
    }

    pub fn mac_manufacturer_part(&self) -> String {
        format!("{:02x}{:02x}{:02x}", self.mac[3], self.mac[4], self.mac[5])
    }
}

impl Display for BleAddress {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} ({})", self.mac_string(), self.ble_type)
    }
}
