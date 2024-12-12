use std::fmt::Display;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Timestamp {
    None,
    Uptime(u64),
    #[cfg(feature = "chrono")]
    Utc(chrono::DateTime<chrono::Utc>),
}

impl Timestamp {
    #[cfg(feature = "chrono")]
    pub fn to_utc(&self) -> Option<chrono::DateTime<chrono::Utc>> {
        match self {
            Timestamp::Utc(utc) => Some(*utc),
            _ => None,
        }
    }
}

impl Display for Timestamp {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Timestamp::None => write!(f, "None"),
            Timestamp::Uptime(uptime) => write!(f, "Uptime: {}", uptime),
            #[cfg(feature = "chrono")]
            Timestamp::Utc(utc) => write!(f, "UTC: {}", utc),
        }
    }
}
