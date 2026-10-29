import Foundation
@preconcurrency import CoreBluetooth
import Combine

public struct DiscoveredPeripheral: Identifiable, @unchecked Sendable {
    public var id: UUID
    public var name: String
    public var rssi: Int
    public var peripheral: CBPeripheral

    public init(id: UUID, name: String, rssi: Int, peripheral: CBPeripheral) {
        self.id = id
        self.name = name
        self.rssi = rssi
        self.peripheral = peripheral
    }
}

@MainActor
public class BLEManager: NSObject, ObservableObject {
    public static let serviceUUID = CBUUID(string: "4E415601-5250-4D4C-6F70-704E61760001")
    public static let cmdNavUUID = CBUUID(string: "4E415602-5250-4D4C-6F70-704E61760002")
    public static let statusAckUUID = CBUUID(string: "4E415603-5250-4D4C-6F70-704E61760003")
    public static let deviceInfoUUID = CBUUID(string: "4E415604-5250-4D4C-6F70-704E61760004")

    @Published public var isScanning = false
    @Published public var isConnected = false
    @Published public var discoveredPeripherals: [DiscoveredPeripheral] = []
    @Published public var connectedPeripheralName: String?
    @Published public var lastAck: AckPacket?
    @Published public var lastLatencyMs: Double = 0
    @Published public var deviceInfo: DeviceInfo?
    @Published public var logs: [String] = []

    private var centralManager: CBCentralManager!
    private var targetPeripheral: CBPeripheral?
    private var cmdNavCharacteristic: CBCharacteristic?
    private var statusAckCharacteristic: CBCharacteristic?
    private var deviceInfoCharacteristic: CBCharacteristic?

    private var sendTimestamps: [UInt32: Date] = [:]

    public override init() {
        super.init()
        self.centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    public func startScanning() {
        guard centralManager.state == .poweredOn else {
            addLog("Bluetooth is not powered on: \(centralManager.state.rawValue)")
            return
        }
        discoveredPeripherals.removeAll()
        isScanning = true
        centralManager.scanForPeripherals(withServices: [Self.serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
        addLog("Started scanning for RPMLopp Nav...")
    }

    public func stopScanning() {
        centralManager.stopScan()
        isScanning = false
        addLog("Stopped scanning.")
    }

    public func connect(to peripheral: CBPeripheral) {
        stopScanning()
        targetPeripheral = peripheral
        peripheral.delegate = self
        centralManager.connect(peripheral, options: nil)
        addLog("Connecting to \(peripheral.name ?? "device")...")
    }

    public func disconnect() {
        guard let p = targetPeripheral else { return }
        centralManager.cancelPeripheralConnection(p)
        addLog("Disconnecting...")
    }

    public func sendPacket(_ packet: NavPacket) {
        guard isConnected, let p = targetPeripheral, let char = cmdNavCharacteristic else {
            addLog("Cannot send packet: Not connected or characteristic missing")
            return
        }

        do {
            let encoder = JSONEncoder()
            let data = try encoder.encode(packet)
            sendTimestamps[packet.seq] = Date()
            p.writeValue(data, for: char, type: .withResponse)
        } catch {
            addLog("Failed to encode NavPacket: \(error)")
        }
    }

    public func sendRawHello(sessionId: String) {
        guard isConnected, let p = targetPeripheral, let char = cmdNavCharacteristic else { return }
        let helloJson = """
        {"v":1,"type":"HELLO","seq":1,"sid":"\(sessionId)","payload":{"client":"RPMLopp-iOS","version":"1.0.0"},"crc":1234}
        """
        if let data = helloJson.data(using: .utf8) {
            sendTimestamps[1] = Date()
            p.writeValue(data, for: char, type: .withResponse)
            addLog("Sent HELLO handshake")
        }
    }

    public func sendRawPing(seq: UInt32) {
        guard isConnected, let p = targetPeripheral, let char = cmdNavCharacteristic else { return }
        let pingJson = """
        {"v":1,"type":"PING","seq":\(seq),"sid":"ping","payload":{},"crc":1}
        """
        if let data = pingJson.data(using: .utf8) {
            sendTimestamps[seq] = Date()
            p.writeValue(data, for: char, type: .withResponse)
            addLog("Sent PING #\(seq)")
        }
    }

    public func addLog(_ msg: String) {
        let timestamp = DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .medium)
        logs.append("[\(timestamp)] \(msg)")
        if logs.count > 100 { logs.removeFirst() }
    }
}

extension BLEManager: @preconcurrency CBCentralManagerDelegate {
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            addLog("Bluetooth powered ON")
        } else {
            addLog("Bluetooth state changed: \(central.state.rawValue)")
            isConnected = false
        }
    }

    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? "Unknown Device"
        if !discoveredPeripherals.contains(where: { $0.id == peripheral.identifier }) {
            let item = DiscoveredPeripheral(id: peripheral.identifier, name: name, rssi: RSSI.intValue, peripheral: peripheral)
            discoveredPeripherals.append(item)
            addLog("Discovered: \(name) (\(RSSI.intValue) dBm)")
        }
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        isConnected = true
        connectedPeripheralName = peripheral.name ?? "RPMLopp Nav"
        addLog("Connected to \(connectedPeripheralName!)! Discovering services...")
        peripheral.discoverServices([Self.serviceUUID])
    }

    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        addLog("Failed to connect: \(error?.localizedDescription ?? "unknown error")")
    }

    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        connectedPeripheralName = nil
        cmdNavCharacteristic = nil
        statusAckCharacteristic = nil
        addLog("Disconnected from device.")
    }
}

extension BLEManager: @preconcurrency CBPeripheralDelegate {
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services where service.uuid == Self.serviceUUID {
            addLog("Found RPMLopp Nav Service! Discovering characteristics...")
            peripheral.discoverCharacteristics([Self.cmdNavUUID, Self.statusAckUUID, Self.deviceInfoUUID], for: service)
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for char in characteristics {
            if char.uuid == Self.cmdNavUUID {
                cmdNavCharacteristic = char
                addLog("Ready: CMD_NAV characteristic acquired.")
            } else if char.uuid == Self.statusAckUUID {
                statusAckCharacteristic = char
                peripheral.setNotifyValue(true, for: char)
                addLog("Subscribed to STATUS_ACK notifications.")
            } else if char.uuid == Self.deviceInfoUUID {
                deviceInfoCharacteristic = char
                peripheral.readValue(for: char)
            }
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value else { return }

        if characteristic.uuid == Self.statusAckUUID {
            do {
                let ack = try JSONDecoder().decode(AckPacket.self, from: data)
                self.lastAck = ack
                if let sendTime = sendTimestamps.removeValue(forKey: ack.seq) {
                    let latency = Date().timeIntervalSince(sendTime) * 1000.0
                    self.lastLatencyMs = latency
                    addLog("ACK #\(ack.seq) received: \(ack.status) (Latency: \(String(format: "%.1f", latency)) ms)")
                } else {
                    addLog("ACK #\(ack.seq) received: \(ack.status)")
                }
            } catch {
                if let rawStr = String(data: data, encoding: .utf8) {
                    addLog("Raw ACK update: \(rawStr)")
                }
            }
        } else if characteristic.uuid == Self.deviceInfoUUID {
            do {
                let info = try JSONDecoder().decode(DeviceInfo.self, from: data)
                self.deviceInfo = info
                addLog("Device Info: \(info.model ?? "") | FW: \(info.fw_ver ?? "")")
            } catch {
                addLog("Could not parse Device Info")
            }
        }
    }
}
