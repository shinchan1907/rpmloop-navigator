import SwiftUI

public struct SimulationView: View {
    @ObservedObject var navEngine: NavigationEngine

    public var body: some View {
        NavigationStack {
            List {
                Section("Route Selection") {
                    HStack {
                        VStack(alignment: .leading) {
                            Text("Coastal Highway Loop")
                                .font(.headline)
                            Text("Synthetic Route (Offline, No API key needed)")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Image(systemName: "checkmark.circle.fill")
                            .foregroundStyle(.blue)
                    }

                    Button(action: {}) {
                        Label("Import Custom GPX Route", systemImage: "square.and.arrow.down")
                    }
                }

                Section("Replay Speed") {
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text("Simulation Rate")
                            Spacer()
                            Text("\(String(format: "%.1f", navEngine.simulationSpeed))x")
                                .monospacedDigit()
                                .bold()
                        }
                        Slider(value: $navEngine.simulationSpeed, in: 0.5...5.0, step: 0.5)
                            .onChange(of: navEngine.simulationSpeed) { _, newVal in
                                navEngine.setSimulationSpeed(newVal)
                            }
                    }
                }

                Section("Fault Injection & Edge Cases") {
                    Button(action: { navEngine.injectGpsLoss() }) {
                        Label("Simulate GPS Signal Loss", systemImage: "location.slash.fill")
                            .foregroundStyle(.red)
                    }

                    Button(action: { navEngine.injectOffRoute() }) {
                        Label("Simulate Off-Route Deviation", systemImage: "arrow.triangle.swap")
                            .foregroundStyle(.orange)
                    }
                }

                Section("Notice on Map Providers") {
                    Text("This demo uses synthetic vector waypoints and user GPX data. Google Maps offline downloads are not an exportable map source for this architecture; genuine production basemaps require a licensed vector tile provider.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Route Simulation")
        }
    }
}
