#pragma once

#include "Platform/Stm32/System/InterruptGuard.h"
#include "Service/RequestManager/RequestManager.h"

#include <cstdint>

namespace dda {

class GpioPin;
enum class Driver : uint8_t;

enum class PowerStageFault : uint32_t {
  None = 0U,
  DriverH1 = 1UL << 0U,
  DriverH2 = 1UL << 1U,
  DriverH3 = 1UL << 2U,
  DriverH4 = 1UL << 3U,
  PowerAlert = 1UL << 4U,
  SafeStateFailure = 1UL << 6U,
};

enum class SystemState : uint8_t {
  Initializing = 0U,
  Safe,
  Ready,
  Faulted,
};

enum class SafetyCommand : uint8_t {
  ReportFault = 1U,
  GetState = 2U,
  GetFaultMask = 3U,
};

/** Foreground observations used by SafetyManager recovery policy. */
struct SafetySnapshot {
  bool powerStageOutputsSafe{false};
  bool faultedDriversDisabled{false};
  bool faultInputsReleased{false};
};

/** Owns board-wide safety policy, fault memory, recovery, and reporting. */
class SafetyManager {
public:
  /** Scoped interrupt mask available to components participating in safety. */
  using CriticalSection = InterruptGuard;

  SafetyManager(RequestManager &requestManager,
                GpioPin &powerAlertPin) noexcept;

  [[nodiscard]] CriticalSection enterCriticalSection() const noexcept;

  void resetForInitialization() noexcept;
  void setPowerAlertConfigured(bool configured) noexcept;
  void setSystemReady(bool ready) noexcept;

  SystemState getState() const noexcept;
  bool isPowerStageReady() const noexcept;
  bool canEnergize(Driver driver) const noexcept;
  bool powerAlertConfigured() const noexcept;
  bool powerAlertReleased() const noexcept;
  bool powerAlertFaultObserved() const noexcept;

  void handleDriverFault(Driver driver) noexcept;
  void handlePowerFault() noexcept;
  void handleSafeStateFailure() noexcept;

  bool safeStateRequired() const noexcept;
  void reportSafeStateResult(bool success) noexcept;
  void process(const SafetySnapshot &snapshot) noexcept;

  bool prepareManualFaultClear(uint32_t &faultEpoch) noexcept;
  bool takeAutomaticFaultClearRequest(uint32_t &faultEpoch) noexcept;
  bool completeFaultClear(uint32_t expectedFaultEpoch, bool hardwareCleared,
                          bool automatic) noexcept;

  uint32_t faultMask() const noexcept;
  uint32_t faultEpoch() const noexcept;
  bool hasAnyFault() const noexcept;
  bool hasGlobalFault() const noexcept;
  bool hasDriverFault(Driver driver) const noexcept;

  /** Handles only communication-oriented safety requests. */
  void processRequest(Request &request) noexcept;

private:
  bool recordFault(uint32_t mask) noexcept;
  void resetFaultRecoveryTracking() noexcept;
  void updatePowerAlertCycle(bool safetyEventPending, uint32_t faults,
                             uint32_t nowMilliseconds) noexcept;
  void updateSafeStatePolicy(const SafetySnapshot &snapshot) noexcept;
  void updateFaultReleaseValidation(const SafetySnapshot &snapshot,
                                    uint32_t nowMilliseconds) noexcept;
  void updateAutomaticClearRequest(bool isolatedPowerAlert) noexcept;
  void queueFaultReport() noexcept;

  RequestManager &_requestManager;
  GpioPin &_powerAlertPin;

  volatile uint32_t _faultMask;
  volatile uint32_t _faultEpoch;
  volatile bool _faultReportPending;
  volatile SystemState _state;
  bool _systemReady;
  bool _powerAlertConfigured;
  bool _faultReleaseTiming;
  volatile bool _safeStateRequired;
  bool _faultSafeStateComplete;
  bool _powerAlertCycleActive;
  bool _powerAlertRequiresAcknowledgement;
  bool _powerAlertRepeatWindowActive;
  uint32_t _faultReleasedSinceMilliseconds;
  uint32_t _faultReleaseEpoch;
  uint32_t _powerAlertRepeatWindowStartedMilliseconds;
  bool _automaticClearPending;
  uint32_t _automaticClearEpoch;
};

} // namespace dda
