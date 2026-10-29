import SwiftUI

public struct NavDashboardView: View {
    @ObservedObject var navEngine: NavigationEngine
    @ObservedObject var bleManager: BLEManager

    public var body: some View {
        NavigationStack {
            VStack(spacing: 20) {
                // Connection/Stale Banner
                if !bleManager.isConnected {
                    HStack {
                        Image(systemName: "exclamationmark.triangle.fill")
                        Text("ESP32 Not Connected — Data displayed locally")
                    }
                    .font(.caption.bold())
                    .padding(8)
                    .frame(maxWidth: .infinity)
                    .background(Color.orange.opacity(0.2))
                    .foregroundStyle(.orange)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
                    .padding(.horizontal)
                }

                // Main Maneuver Card
                VStack(spacing: 12) {
                    HStack(spacing: 20) {
                        Image(systemName: navEngine.currentPayload.maneuver.iconName)
                            .font(.system(size: 64, weight: .bold))
                            .foregroundStyle(.cyan)
                            .frame(width: 80, height: 80)

                        VStack(alignment: .leading, spacing: 2) {
                            Text(formatDistance(navEngine.currentPayload.dist_m))
                                .font(.system(size: 42, weight: .black, design: .rounded))
                                .foregroundStyle(.primary)

                            Text("METERS")
                                .font(.caption.bold())
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                    }

                    Divider()

                    VStack(alignment: .leading, spacing: 4) {
                        Text(navEngine.currentPayload.road.isEmpty ? "Ready to Navigate" : navEngine.currentPayload.road)
                            .font(.title3.bold())

                        Text(navEngine.currentPayload.instr.isEmpty ? "Tap 'Start' below to begin route simulation" : navEngine.currentPayload.instr)
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .clipShape(RoundedRectangle(cornerRadius: 16))
                .padding(.horizontal)

                // Speed & Trip Statistics
                HStack(spacing: 16) {
                    StatCard(
                        title: "SPEED",
                        value: "\(Int(navEngine.currentPayload.spd_mps * 3.6))",
                        unit: "KM/H"
                    )
                    StatCard(
                        title: "REMAINING",
                        value: String(format: "%.1f", Double(navEngine.currentPayload.rem_dist_m) / 1000.0),
                        unit: "KM"
                    )
                    StatCard(
                        title: "ETA",
                        value: "\(navEngine.currentPayload.rem_time_s / 60)",
                        unit: "MIN"
                    )
                }
                .padding(.horizontal)

                // Trip Progress Bar
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Text("Route Progress")
                            .font(.caption.bold())
                            .foregroundStyle(.secondary)
                        Spacer()
                        Text("\(navEngine.currentPayload.progress_pct)%")
                            .font(.caption.bold())
                    }
                    ProgressView(value: Double(navEngine.currentPayload.progress_pct), total: 100.0)
                        .tint(.cyan)
                }
                .padding(.horizontal)

                Spacer()

                // Navigation Controls
                HStack(spacing: 14) {
                    if !navEngine.isNavigating {
                        Button(action: { navEngine.startNavigation() }) {
                            Label("Start Navigation", systemImage: "play.fill")
                                .font(.headline)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.blue)
                                .foregroundStyle(.white)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                        }
                    } else {
                        Button(action: {
                            if navEngine.isPaused {
                                navEngine.resumeNavigation()
                            } else {
                                navEngine.pauseNavigation()
                            }
                        }) {
                            Label(navEngine.isPaused ? "Resume" : "Pause", systemImage: navEngine.isPaused ? "play.fill" : "pause.fill")
                                .font(.headline)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.orange)
                                .foregroundStyle(.white)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                        }

                        Button(action: { navEngine.stopNavigation() }) {
                            Label("Stop", systemImage: "stop.fill")
                                .font(.headline)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.red)
                                .foregroundStyle(.white)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                        }
                    }
                }
                .padding(.horizontal)
                .padding(.bottom, 12)
            }
            .navigationTitle("RPMLopp Nav")
        }
    }

    private func formatDistance(_ m: UInt32) -> String {
        if m >= 1000 {
            return String(format: "%.1f km", Double(m) / 1000.0)
        }
        return "\(m)"
    }
}

struct StatCard: View {
    let title: String
    let value: String
    let unit: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .font(.caption2.bold())
                .foregroundStyle(.secondary)
            HStack(alignment: .lastTextBaseline, spacing: 2) {
                Text(value)
                    .font(.title2.bold().monospacedDigit())
                Text(unit)
                    .font(.caption2.bold())
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(Color(.secondarySystemBackground))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}
