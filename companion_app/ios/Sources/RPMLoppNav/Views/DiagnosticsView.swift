import SwiftUI

public struct DiagnosticsView: View {
    @ObservedObject var bleManager: BLEManager

    public var body: some View {
        NavigationStack {
            List {
                Section("Protocol & Telemetry") {
                    HStack {
                        Text("BLE Connected")
                        Spacer()
                        Text(bleManager.isConnected ? "YES" : "NO")
                            .bold()
                            .foregroundStyle(bleManager.isConnected ? .green : .red)
                    }

                    if let ack = bleManager.lastAck {
                        HStack {
                            Text("Last Accepted SEQ")
                            Spacer()
                            Text("#\(ack.seq)")
                                .monospacedDigit()
                        }
                        HStack {
                            Text("Last ACK Status")
                            Spacer()
                            Text(ack.status)
                                .bold()
                                .foregroundStyle(ack.status == "ACCEPTED" ? .green : .orange)
                        }
                    }

                    HStack {
                        Text("Measured Latency")
                        Spacer()
                        Text("\(String(format: "%.1f", bleManager.lastLatencyMs)) ms")
                            .monospacedDigit()
                    }
                }

                Section("Diagnostic Event Log") {
                    if bleManager.logs.isEmpty {
                        Text("No logs recorded yet.")
                            .foregroundStyle(.secondary)
                            .font(.caption)
                    } else {
                        ForEach(bleManager.logs.reversed(), id: \.self) { log in
                            Text(log)
                                .font(.system(size: 11, design: .monospaced))
                        }
                    }
                }
            }
            .navigationTitle("Diagnostics")
        }
    }
}
