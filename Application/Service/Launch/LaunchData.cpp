#include "Service/Launch/LaunchData.hpp"

#include <cstring>

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "LaunchData wire serialization requires a little-endian target"
#endif

namespace {

uint8_t boundedCount(uint8_t count) noexcept {
  return count < dda::SensorEvents::Capacity ? count
                                             : dda::SensorEvents::Capacity;
}

uint32_t boundedCurrentSamples(uint32_t count) noexcept {
  constexpr uint32_t capacity = dda::CurrentBufferSize / dda::BridgeCount;
  return count < capacity ? count : capacity;
}

uint32_t boundedPowerSamples(uint32_t count) noexcept {
  return count < dda::PowerBufferSize ? count : dda::PowerBufferSize;
}

uint16_t boundedSnapshots(const dda::RequestSnapshot *snapshots,
                          uint16_t count) noexcept {
  if (snapshots == nullptr) {
    return 0U;
  }
  return count < dda::RequestManager::SnapshotCapacity
             ? count
             : dda::RequestManager::SnapshotCapacity;
}

class SegmentWriter {
public:
  SegmentWriter(uint32_t offset, uint8_t *output, uint8_t capacity) noexcept
      : _offset(offset), _output(output), _capacity(capacity) {}

  void append(const void *data, uint32_t length) noexcept {
    const uint32_t segmentStart = _streamPosition;
    _streamPosition += length;
    if ((_written >= _capacity) || (_offset >= _streamPosition)) {
      return;
    }

    const uint32_t sourceOffset =
        _offset > segmentStart ? _offset - segmentStart : 0U;
    const uint32_t available = length - sourceOffset;
    const uint32_t remaining =
        static_cast<uint32_t>(_capacity - _written);
    const uint32_t copyLength = available < remaining ? available : remaining;
    std::memcpy(_output + _written,
                static_cast<const uint8_t *>(data) + sourceOffset, copyLength);
    _written = static_cast<uint8_t>(_written + copyLength);
  }

  void appendUint32(uint32_t value) noexcept {
    const uint8_t bytes[] = {
        static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8U),
        static_cast<uint8_t>(value >> 16U), static_cast<uint8_t>(value >> 24U)};
    append(bytes, sizeof(bytes));
  }

  void appendUint16(uint16_t value) noexcept {
    const uint8_t bytes[] = {static_cast<uint8_t>(value),
                             static_cast<uint8_t>(value >> 8U)};
    append(bytes, sizeof(bytes));
  }

  uint8_t written() const noexcept { return _written; }

private:
  uint32_t _offset;
  uint8_t *_output;
  uint8_t _capacity;
  uint32_t _streamPosition{0U};
  uint8_t _written{0U};
};

} // namespace

namespace dda {

uint32_t LaunchData::serializedSize() const noexcept {
  uint32_t size = 0U;
  for (const SensorEvents &events : sensorEvents) {
    size += 2U;
    size += 4U * boundedCount(events.risingEdgeCount);
    size += 4U * boundedCount(events.fallingEdgeCount);
  }
  size += 4U + (BridgeCount * boundedCurrentSamples(currentSampleCount));
  size += 8U + (2U * boundedPowerSamples(powerSampleCount));
  const uint16_t snapshots =
      boundedSnapshots(requestSnapshots, snapshotCount);
  return size + 12U + 2U + (16U * snapshots);
}

uint8_t LaunchData::serialize(uint32_t offset, uint8_t *output,
                              uint8_t capacity) const noexcept {
  if ((output == nullptr) || (capacity == 0U) ||
      (offset >= serializedSize())) {
    return 0U;
  }

  SegmentWriter writer(offset, output, capacity);
  for (const SensorEvents &events : sensorEvents) {
    const uint8_t risingCount = boundedCount(events.risingEdgeCount);
    const uint8_t fallingCount = boundedCount(events.fallingEdgeCount);
    writer.append(&risingCount, sizeof(risingCount));
    writer.append(&fallingCount, sizeof(fallingCount));
    writer.append(events.risingEdgeTimestamps, 4U * risingCount);
    writer.append(events.fallingEdgeTimestamps, 4U * fallingCount);
  }

  const uint32_t currentSamples = boundedCurrentSamples(currentSampleCount);
  writer.appendUint32(currentSamples);
  writer.append(currentData, BridgeCount * currentSamples);

  const uint32_t powerSamples = boundedPowerSamples(powerSampleCount);
  writer.appendUint32(powerSamples);
  writer.appendUint32(missedPowerSampleCount);
  writer.append(powerData, 2U * powerSamples);
  writer.appendUint32(launchStart);
  writer.appendUint32(launchEnd);
  writer.appendUint32(velocityTickDelta);

  const uint16_t snapshots =
      boundedSnapshots(requestSnapshots, snapshotCount);
  writer.appendUint16(snapshots);
  for (uint16_t index = 0U; index < snapshots; ++index) {
    const RequestSnapshot &snapshot = requestSnapshots[index];
    const uint8_t service = static_cast<uint8_t>(snapshot.service);
    writer.append(&service, sizeof(service));
    writer.append(&snapshot.command, sizeof(snapshot.command));
    writer.append(&snapshot.options, sizeof(snapshot.options));
    writer.append(&snapshot.flags, sizeof(snapshot.flags));
    writer.appendUint32(snapshot.createdAt);
    writer.appendUint32(snapshot.outgoingAt);
    writer.appendUint32(snapshot.doneAt);
  }
  return writer.written();
}

} // namespace dda
