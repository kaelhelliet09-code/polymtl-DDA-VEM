#include "Service/RequestManager/RequestManager.h"
#include "Platform/Stm32/System/InterruptGuard.h"

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

void Request::accept(uint32_t timestamp) noexcept {
  initialDestination = destination;
  initialOptions = options;
  createdAt = timestamp;
}

void Request::markOutgoing(uint32_t timestamp) noexcept {
  state = RequestState::Outgoing;
  reachedOutgoing = true;
  outgoingAt = timestamp;
}

void Request::complete(Service completedBy) noexcept {
  const uint32_t timestamp = TIM2->CNT;
  if (requiresAnswer && (source != Service::None)) {
    destination = source;
    source = completedBy;
    requiresAnswer = false;
    markOutgoing(timestamp);
    return;
  }
  state = RequestState::Done;
  doneAt = timestamp;
}

RequestSnapshot::RequestSnapshot(const Request &request) noexcept
    : createdAt(request.createdAt), outgoingAt(request.outgoingAt),
      doneAt(request.doneAt), service(request.initialDestination),
      command(request.command), options(request.initialOptions),
      flags(request.reachedOutgoing ? HasOutgoingTimestamp : 0U) {}

RequestManager::RequestManager() noexcept
    : _requests{}, _snapshots{}, _handlers{}, _nextInsert(0U),
      _requestCount(0U), _snapshotCount(0U), _launchActive(false),
      _sensorBlock(true), _debugMode(false) {}

bool RequestManager::queueRequest(const Request &request) noexcept {
  InterruptGuard interruptGuard;
  if (blocksSensorNotification(request)) {
    return false;
  }
  Request queuedRequest = request;
  if (blocksHostRequest(request)) {
    queuedRequest.destination = Service::UsbControl;
    queuedRequest.source = request.destination;
    queuedRequest.options =
        static_cast<uint8_t>(RequestStatus::BlockedDuringLaunch);
    queuedRequest.requiresAnswer = false;
    queuedRequest.state = RequestState::Outgoing;
  }
  return queueUnchecked(queuedRequest);
}

bool RequestManager::queueUnchecked(const Request &request) noexcept {
  if (_requestCount >= Capacity) {
    return false;
  }

  for (uint8_t offset = 0U; offset < Capacity; ++offset) {
    const uint8_t index =
        static_cast<uint8_t>((_nextInsert + offset) % Capacity);
    if (_requests[index].state != RequestState::Idle) {
      continue;
    }

    _requests[index] = request;
    const uint32_t acceptedAt = TIM2->CNT;
    _requests[index].accept(acceptedAt);
    if (_requests[index].state == RequestState::Idle) {
      _requests[index].state = RequestState::Incoming;
    } else if (_requests[index].state == RequestState::Outgoing) {
      _requests[index].markOutgoing(acceptedAt);
    }
    _nextInsert = static_cast<uint8_t>((index + 1U) % Capacity);
    ++_requestCount;
    return true;
  }
  return false;
}

bool RequestManager::hasCapacity() const noexcept {
  InterruptGuard interruptGuard;
  return _requestCount < Capacity;
}

void RequestManager::dispatchToServices() noexcept {
  for (Request &request : _requests) {
    if ((request.state != RequestState::Incoming) &&
        (request.state != RequestState::Outgoing)) {
      continue;
    }

    const uint8_t destination = static_cast<uint8_t>(request.destination);
    if ((destination >= static_cast<uint8_t>(Service::Count)) ||
        (_handlers[destination].process == nullptr)) {
      continue;
    }
    _handlers[destination].process(_handlers[destination].context, request);
  }
}

void RequestManager::clearCompleted() noexcept {
  InterruptGuard interruptGuard;
  for (uint8_t index = 0U; index < Capacity; ++index) {
    Request &request = _requests[index];
    if (request.state == RequestState::Done) {
      if (_snapshotCount < SnapshotCapacity) {
        _snapshots[_snapshotCount++] = RequestSnapshot(request);
      }
      request = {};
      --_requestCount;
    }
  }
}

void RequestManager::clearSnapshots() noexcept {
  InterruptGuard interruptGuard;
  _snapshotCount = 0U;
}

void RequestManager::discardRequestsInvolving(Service service) noexcept {
  InterruptGuard interruptGuard;
  for (Request &request : _requests) {
    if ((request.state == RequestState::Idle) ||
        ((request.source != service) && (request.destination != service))) {
      continue;
    }
    request = {};
    --_requestCount;
  }
}

void RequestManager::process() noexcept {
  dispatchToServices();
  clearCompleted();
}

void RequestManager::setLaunchActive(bool active) noexcept {
  _launchActive = active;
  _sensorBlock = !active;
}

bool RequestManager::isLaunchActive() const noexcept { return _launchActive; }

void RequestManager::unlockSensor() noexcept { _sensorBlock = false; }

bool RequestManager::isSensorBlocked() const noexcept { return _sensorBlock; }

void RequestManager::setDebugMode(bool enabled) noexcept {
  _debugMode = enabled;
}

bool RequestManager::isCompetitionMode() const noexcept {
  return !_debugMode;
}

bool RequestManager::blocksSensorNotification(
    const Request &request) const noexcept {
  return _sensorBlock && (request.source == Service::SensorControl) &&
         (request.destination == Service::UsbControl);
}

bool RequestManager::blocksHostRequest(const Request &request) const noexcept {
  return _launchActive && !_debugMode &&
         (request.source == Service::UsbControl) &&
         ((request.destination == Service::CoilControl) ||
          (request.destination == Service::SensorControl));
}

} // namespace dda
