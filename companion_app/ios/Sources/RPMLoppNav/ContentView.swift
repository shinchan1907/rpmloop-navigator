import SwiftUI

public struct ContentView: View {
    @StateObject private var bleManager = BLEManager()
    @StateObject private var navEngine: NavigationEngine

    public init() {
        let ble = BLEManager()
        _bleManager = StateObject(wrappedValue: ble)
        _navEngine = StateObject(wrappedValue: NavigationEngine(bleManager: ble))
    }

    public var body: some View {
        TabView {
            NavDashboardView(navEngine: navEngine, bleManager: bleManager)
                .tabItem {
                    Label("Navigation", systemImage: "location.fill")
                }

            ConnectionView(bleManager: bleManager)
                .tabItem {
                    Label("Connection", systemImage: "antenna.radiowaves.left.and.right")
                }

            SimulationView(navEngine: navEngine)
                .tabItem {
                    Label("Simulation", systemImage: "play.circle.fill")
                }

            DiagnosticsView(bleManager: bleManager)
                .tabItem {
                    Label("Diagnostics", systemImage: "waveform.path.ecg")
                }

            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gearshape.fill")
                }
        }
    }
}
