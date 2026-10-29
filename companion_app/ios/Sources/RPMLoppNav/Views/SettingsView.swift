import SwiftUI

public struct SettingsView: View {
    @AppStorage("units") private var units = "Metric (km/h, m)"
    @AppStorage("updateRateHz") private var updateRateHz = 1

    public var body: some View {
        NavigationStack {
            List {
                Section("Preferences") {
                    Picker("Measurement Units", selection: $units) {
                        Text("Metric (km/h, m)").tag("Metric (km/h, m)")
                        Text("Imperial (mph, ft)").tag("Imperial (mph, ft)")
                    }

                    Stepper("Update Rate: \(updateRateHz) Hz", value: $updateRateHz, in: 1...5)
                }

                Section("Free iOS Personal Team Provisioning") {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("7-Day Expiry Notice")
                            .font(.subheadline.bold())
                        Text("Apple limits free Personal Developer accounts to 7 days before provisioning profiles expire. This is an Apple server-side limitation.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    VStack(alignment: .leading, spacing: 6) {
                        Text("Zero-Cost Renewal Workflow")
                            .font(.subheadline.bold())
                        Text("To refresh without losing any settings or app data, keep the bundle ID identical (`com.sunnygupta.RPMLoppNav`) and re-run:")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Text("xtool dev build --sign && xtool install")
                            .font(.system(size: 11, design: .monospaced))
                            .padding(6)
                            .background(Color(.secondarySystemBackground))
                            .clipShape(RoundedRectangle(cornerRadius: 6))
                    }
                }

                Section("App Information") {
                    HStack {
                        Text("App Version")
                        Spacer()
                        Text("1.0.0").foregroundStyle(.secondary)
                    }
                    HStack {
                        Text("Protocol Version")
                        Spacer()
                        Text("v1.0.0").foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle("Settings")
        }
    }
}
