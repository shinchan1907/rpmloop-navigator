import Foundation

public enum ManeuverType: String, Codable, CaseIterable, Sendable {
    case none = "NONE"
    case straight = "STRAIGHT"
    case slightRight = "SLIGHT_RIGHT"
    case turnRight = "TURN_RIGHT"
    case sharpRight = "SHARP_RIGHT"
    case uturn = "UTURN"
    case sharpLeft = "SHARP_LEFT"
    case turnLeft = "TURN_LEFT"
    case slightLeft = "SLIGHT_LEFT"
    case roundabout = "ROUNDABOUT"
    case destination = "DESTINATION"

    public var iconName: String {
        switch self {
        case .straight: return "arrow.up"
        case .slightRight: return "arrow.up.right"
        case .turnRight: return "arrow.turn.up.right"
        case .sharpRight: return "arrow.right"
        case .uturn: return "arrow.uturn.down"
        case .sharpLeft: return "arrow.left"
        case .turnLeft: return "arrow.turn.up.left"
        case .slightLeft: return "arrow.up.left"
        case .roundabout: return "arrow.triangle.2.circlepath"
        case .destination: return "mappin.and.ellipse"
        case .none: return "location.north"
        }
    }
}

public enum NavStatus: String, Codable, Sendable {
    case disconnected = "DISCONNECTED"
    case connectedIdle = "CONNECTED_IDLE"
    case navigating = "NAVIGATING"
    case paused = "PAUSED"
    case stale = "STALE"
}

public struct NavPayload: Codable, Sendable {
    public var pos_valid: Bool
    public var lat: Double
    public var lon: Double
    public var spd_mps: Float
    public var head_deg: Float
    public var maneuver: ManeuverType
    public var dist_m: UInt32
    public var road: String
    public var instr: String
    public var rem_dist_m: UInt32
    public var rem_time_s: UInt32
    public var progress_pct: UInt8
    public var off_route: Bool
    public var gps_acc_m: Float
    public var is_sim: Bool

    public init(
        pos_valid: Bool = true,
        lat: Double = 37.7749,
        lon: Double = -122.4194,
        spd_mps: Float = 0,
        head_deg: Float = 0,
        maneuver: ManeuverType = .straight,
        dist_m: UInt32 = 0,
        road: String = "",
        instr: String = "",
        rem_dist_m: UInt32 = 0,
        rem_time_s: UInt32 = 0,
        progress_pct: UInt8 = 0,
        off_route: Bool = false,
        gps_acc_m: Float = 3.0,
        is_sim: Bool = false
    ) {
        self.pos_valid = pos_valid
        self.lat = lat
        self.lon = lon
        self.spd_mps = spd_mps
        self.head_deg = head_deg
        self.maneuver = maneuver
        self.dist_m = dist_m
        self.road = road
        self.instr = instr
        self.rem_dist_m = rem_dist_m
        self.rem_time_s = rem_time_s
        self.progress_pct = progress_pct
        self.off_route = off_route
        self.gps_acc_m = gps_acc_m
        self.is_sim = is_sim
    }
}

public struct NavPacket: Codable, Sendable {
    public var v: Int
    public var type: String
    public var seq: UInt32
    public var sid: String
    public var rid: String
    public var t_ms: UInt64
    public var payload: NavPayload
    public var crc: UInt16

    public init(
        seq: UInt32,
        sid: String,
        rid: String,
        type: String = "NAV_UPDATE",
        payload: NavPayload
    ) {
        self.v = 1
        self.type = type
        self.seq = seq
        self.sid = sid
        self.rid = rid
        self.t_ms = UInt64(Date().timeIntervalSince1970 * 1000)
        self.payload = payload
        self.crc = 0
        self.crc = calculateCRC()
    }

    public func calculateCRC() -> UInt16 {
        var crc: UInt16 = 0xFFFF
        let str = "\(seq):\(sid):\(payload.dist_m):\(payload.road)"
        for byte in str.utf8 {
            crc ^= UInt16(byte) << 8
            for _ in 0..<8 {
                if (crc & 0x8000) != 0 {
                    crc = (crc << 1) ^ 0x1021
                } else {
                    crc = crc << 1
                }
            }
        }
        return crc
    }
}

public struct AckPacket: Codable, Sendable {
    public var v: Int
    public var type: String
    public var seq: UInt32
    public var status: String
    public var t_ms: UInt64?
}

public struct DeviceInfo: Codable, Sendable {
    public var model: String?
    public var fw_ver: String?
    public var proto_ver: Int?
    public var lcd_w: Int?
    public var lcd_h: Int?
}
