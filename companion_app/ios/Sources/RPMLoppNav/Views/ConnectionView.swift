import SwiftUI

public struct ConnectionView: View {
    @ObservedObject var bleManager: BLEManager

    public var body: some View {
        NavigationStack {
            List {
                Section("Connection Status") {
                    HStack {
                        Circle()
                            .fill(bleManager.isConnected ? Color.green : Color.red)
                            .frame(width: 12, height: 12)
                        Text(bleManager.isConnected ? "Connected to \(bleManager.connectedPeripheralName ?? "RPMLopp Nav")" : "Disconnected")
                            .font(.headline)
                        Spacer()
                        if bleManager.isConnected {
                            Button("Disconnect", role: .destructive) {
                                bleManager.disconnect()
                            }
                            .buttonStyle(.bordered)
                        }
                    }

                    if bleManager.isConnected {
                        HStack {
                            Text("Round-Trip Latency")
                            Spacer()
                            Text("\(String(format: "%.1f", bleManager.lastLatencyMs)) ms")
                                .monospacedDigit()
                                .foregroundStyle(.secondary)
                        }

                        if let info = bleManager.deviceInfo {
                            HStack {
                                Text("Model")
                                Spacer()
                                Text(info.model ?? "--").foregroundStyle(.secondary)
                            }
                            HStack {
                                Text("Firmware")
                                Spacer()
                                Text(info.fw_ver ?? "--").foregroundStyle(.secondary)
                            }
                        }

                        Button("Send Test Ping") {
                            bleManager.sendRawPing(seq: 99)
                        }
                        .buttonStyle(.borderedProminent)
                    }
                }

                Section("Discovered Devices (RPMLopp Nav)") {
                    if bleManager.discoveredPeripherals.isEmpty {
                        Text(bleManager.isScanning ? "Scanning for nearby BLE devices..." : "Tap 'Start Scan' to discover RPMLopp Nav")
                            .foregroundStyle(.secondary)
                            .font(.callout)
                    } else {
                        ForEach(bleManager.discoveredPeripherals) { dev in
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(dev.name)
                                        .font(.headline)
                                    Text("RSSI: \(dev.rssi) dBm")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                                Spacer()
                                Button("Connect") {
                                    bleManager.connect(to: dev.peripheral)
                                }
                                .buttonStyle(.borderedProminent)
                            }
                        }
                    }
                }

                Section("Notice on iOS BLE Pairing") {
                    Text("RPMLopp Nav uses custom Bluetooth Low Energy (GATT). Pairing happens directly inside this app and will not appear in the iOS Settings Bluetooth list.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Device Connection")
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Button(bleManager.isScanning ? "Stop Scan" : "Start Scan") {
                        if bleManager.isScanning {
                            bleManager.stopScanning()
                        } else {
                            bleManager.startScanning()
                        }
                    }
                }
            }
        }
    }
}
