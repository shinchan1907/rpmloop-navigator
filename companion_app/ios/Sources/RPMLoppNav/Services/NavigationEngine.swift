import Foundation
import CoreLocation
import Combine

public struct SyntheticStep: Sendable {
    public var maneuver: ManeuverType
    public var road: String
    public var instruction: String
    public var distanceM: UInt32
    public var speedMps: Float
    public var heading: Float
}

@MainActor
public class NavigationEngine: ObservableObject {
    @Published public var isNavigating = false
    @Published public var isPaused = false
    @Published public var isSimulated = true
    @Published public var currentPayload = NavPayload()
    @Published public var simulationSpeed: Double = 1.0
    @Published public var currentStepIndex = 0
    @Published public var totalSteps = 0

    private var bleManager: BLEManager
    private var sequenceNumber: UInt32 = 10
    private var sessionId = UUID().uuidString.prefix(8)
    private var timer: Timer?

    // Bundled Synthetic Route (Coastal Highway Loop)
    private let syntheticRoute: [SyntheticStep] = [
        SyntheticStep(maneuver: .straight, road: "Grand Avenue", instruction: "Proceed straight on Grand Ave", distanceM: 600, speedMps: 12.0, heading: 0),
        SyntheticStep(maneuver: .turnRight, road: "Ocean Highway", instruction: "In 250m, turn right onto Ocean Hwy", distanceM: 250, speedMps: 13.5, heading: 90),
        SyntheticStep(maneuver: .straight, road: "Ocean Highway", instruction: "Continue on Ocean Hwy for 1.2 km", distanceM: 1200, speedMps: 18.0, heading: 90),
        SyntheticStep(maneuver: .slightLeft, road: "Cliffside Drive", instruction: "Keep left onto Cliffside Dr", distanceM: 400, speedMps: 14.0, heading: 60),
        SyntheticStep(maneuver: .roundabout, road: "Marina Circle", instruction: "Enter roundabout, take 2nd exit", distanceM: 300, speedMps: 8.5, heading: 120),
        SyntheticStep(maneuver: .destination, road: "Harbor Point Lookout", instruction: "You have arrived at your destination", distanceM: 0, speedMps: 0.0, heading: 180)
    ]

    public init(bleManager: BLEManager) {
        self.bleManager = bleManager
        self.totalSteps = syntheticRoute.count
    }

    public func startNavigation() {
        isNavigating = true
        isPaused = false
        currentStepIndex = 0
        sequenceNumber = 10
        bleManager.sendRawHello(sessionId: String(sessionId))

        updateAndTransmitCurrentStep()

        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 1.0 / simulationSpeed, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.tick()
            }
        }
    }

    public func pauseNavigation() {
        isPaused = true
        var p = currentPayload
        p.instr = "Navigation Paused"
        let packet = NavPacket(seq: sequenceNumber, sid: String(sessionId), rid: "coastal_loop", type: "NAV_PAUSE", payload: p)
        sequenceNumber += 1
        bleManager.sendPacket(packet)
    }

    public func resumeNavigation() {
        isPaused = false
        var p = currentPayload
        let packet = NavPacket(seq: sequenceNumber, sid: String(sessionId), rid: "coastal_loop", type: "NAV_RESUME", payload: p)
        sequenceNumber += 1
        bleManager.sendPacket(packet)
    }

    public func stopNavigation() {
        isNavigating = false
        isPaused = false
        timer?.invalidate()
        timer = nil

        var p = currentPayload
        p.instr = "Navigation Ended"
        let packet = NavPacket(seq: sequenceNumber, sid: String(sessionId), rid: "coastal_loop", type: "NAV_STOP", payload: p)
        sequenceNumber += 1
        bleManager.sendPacket(packet)
    }

    public func setSimulationSpeed(_ speed: Double) {
        simulationSpeed = speed
        if isNavigating && !isPaused {
            timer?.invalidate()
            timer = Timer.scheduledTimer(withTimeInterval: 1.0 / simulationSpeed, repeats: true) { [weak self] _ in
                Task { @MainActor in
                    self?.tick()
                }
            }
        }
    }

    // Fault Injections
    public func injectGpsLoss() {
        currentPayload.pos_valid = false
        currentPayload.gps_acc_m = 99.0
        currentPayload.instr = "GPS Signal Lost — Searching..."
        transmitPayload()
    }

    public func injectOffRoute() {
        currentPayload.off_route = true
        currentPayload.instr = "Off Route! Rerouting..."
        transmitPayload()
    }

    private func tick() {
        guard isNavigating, !isPaused else { return }

        // Decrement distance to turn
        if currentPayload.dist_m > 30 {
            currentPayload.dist_m -= 30
            if currentPayload.rem_dist_m > 30 { currentPayload.rem_dist_m -= 30 }
            if currentPayload.rem_time_s > 2 { currentPayload.rem_time_s -= 2 }
        } else {
            // Next maneuver
            if currentStepIndex + 1 < syntheticRoute.count {
                currentStepIndex += 1
                loadStep(syntheticRoute[currentStepIndex])
            } else {
                // Completed
                stopNavigation()
                return
            }
        }

        currentPayload.progress_pct = UInt8(min(100, (currentStepIndex * 100) / max(1, syntheticRoute.count)))
        transmitPayload()
    }

    private func loadStep(_ step: SyntheticStep) {
        currentPayload.maneuver = step.maneuver
        currentPayload.road = step.road
        currentPayload.instr = step.instruction
        currentPayload.dist_m = step.distanceM
        currentPayload.spd_mps = step.speedMps
        currentPayload.head_deg = step.heading
        currentPayload.rem_dist_m = max(currentPayload.rem_dist_m, step.distanceM * 2)
        currentPayload.rem_time_s = max(currentPayload.rem_time_s, (step.distanceM / 10))
        currentPayload.is_sim = true
        currentPayload.pos_valid = true
        currentPayload.off_route = false
    }

    private func updateAndTransmitCurrentStep() {
        loadStep(syntheticRoute[currentStepIndex])
        transmitPayload()
    }

    private func transmitPayload() {
        let packet = NavPacket(
            seq: sequenceNumber,
            sid: String(sessionId),
            rid: "coastal_loop",
            type: "NAV_UPDATE",
            payload: currentPayload
        )
        sequenceNumber += 1
        bleManager.sendPacket(packet)
    }
}
