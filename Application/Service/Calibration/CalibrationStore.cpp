// Implements the two-page transactional calibration journal with commit-last
// writes, incompatible-format lockout, and identical-snapshot suppression.
#include "Service/Calibration/CalibrationRecordCodec.h"
#include "Service/Calibration/CalibrationStore.h"

#include <cstddef>
#include <cstring>

extern "C" {
#include "stm32g0xx_hal.h"

extern uint8_t __calibration_page_a_start__;
extern uint8_t __calibration_page_b_start__;
}

namespace {

using FlashSnapshot = dda::calibration_record::Snapshot;

static_assert(sizeof(FlashSnapshot) <= FLASH_PAGE_SIZE);

enum class PageState : uint8_t {
  Erased,
  Valid,
  Corrupt,
  Incompatible,
};

uintptr_t pageAAddress() noexcept {
  return reinterpret_cast<uintptr_t>(&__calibration_page_a_start__);
}

uintptr_t pageBAddress() noexcept {
  return reinterpret_cast<uintptr_t>(&__calibration_page_b_start__);
}

bool pageIsErased(uintptr_t address) noexcept {
  const volatile uint64_t *const words =
      reinterpret_cast<const volatile uint64_t *>(address);
  for (uint32_t index = 0U; index < (FLASH_PAGE_SIZE / sizeof(uint64_t));
       ++index) {
    if (words[index] != UINT64_MAX) {
      return false;
    }
  }
  return true;
}

PageState classifyPage(uintptr_t address, FlashSnapshot &snapshot) noexcept {
  if (pageIsErased(address)) {
    return PageState::Erased;
  }

  std::memcpy(&snapshot, reinterpret_cast<const void *>(address),
              sizeof(snapshot));
  if (snapshot.magic != dda::calibration_record::Magic) {
    return PageState::Corrupt;
  }
  // Inspect the stable header before version-specific tail fields. A future
  // format may move its commit marker, and downgraded firmware must never
  // classify such a page as disposable corruption.
  if (snapshot.formatVersion > dda::calibration_record::FormatVersion) {
    return PageState::Incompatible;
  }
  if (snapshot.formatVersion != dda::calibration_record::FormatVersion ||
      snapshot.snapshotSize != sizeof(FlashSnapshot)) {
    return PageState::Corrupt;
  }
  if (snapshot.commitMarker != dda::calibration_record::CommitMarker) {
    return PageState::Corrupt;
  }

  const uint32_t expectedCrc = dda::calibration_record::calculateCrc32(
      reinterpret_cast<const uint8_t *>(&snapshot),
      offsetof(FlashSnapshot, crc32));
  return expectedCrc == snapshot.crc32 ? PageState::Valid : PageState::Corrupt;
}

void flashBankAndPage(uintptr_t address, uint32_t &bank,
                      uint32_t &page) noexcept {
#if defined(FLASH_DBANK_SUPPORT)
  if ((FLASH_BANK_NB == 2U) && (address >= (FLASH_BASE + FLASH_BANK_SIZE))) {
    bank = FLASH_BANK_2;
    page = static_cast<uint32_t>((address - (FLASH_BASE + FLASH_BANK_SIZE)) /
                                 FLASH_PAGE_SIZE);
    return;
  }
#endif
  bank = FLASH_BANK_1;
  page = static_cast<uint32_t>((address - FLASH_BASE) / FLASH_PAGE_SIZE);
}

HAL_StatusTypeDef erasePage(uintptr_t address) noexcept {
  uint32_t bank = FLASH_BANK_1;
  uint32_t page = 0U;
  flashBankAndPage(address, bank, page);

  FLASH_EraseInitTypeDef erase{};
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks = bank;
  erase.Page = page;
  erase.NbPages = 1U;
  uint32_t pageError = 0xFFFFFFFFU;
  const HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &pageError);
  return (status == HAL_OK && pageError == 0xFFFFFFFFU) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef programBytes(uintptr_t address, const uint8_t *data,
                               size_t length) noexcept {
  for (size_t offset = 0U; offset < length; offset += sizeof(uint64_t)) {
    uint64_t word = 0U;
    std::memcpy(&word, data + offset, sizeof(word));
    const HAL_StatusTypeDef status =
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + offset, word);
    if (status != HAL_OK) {
      return status;
    }
  }
  return HAL_OK;
}

} // namespace

namespace dda {

CalibrationStore::CalibrationStore() noexcept
    : _sensorData{}, _activePageAddress(0U), _activeSequence(0U),
      _flashErrorFlags(0U),
      _initializationResult(CalibrationStoreResult::NotInitialized),
      _validSensorMask(0U), _readable(false) {
  resetData();
}

CalibrationStoreResult CalibrationStore::initialize() noexcept {
  resetData();
  _activePageAddress = 0U;
  _activeSequence = 0U;
  _flashErrorFlags = 0U;
  _validSensorMask = 0U;
  _readable = false;

  FlashSnapshot pageA{};
  FlashSnapshot pageB{};
  const PageState stateA = classifyPage(pageAAddress(), pageA);
  const PageState stateB = classifyPage(pageBAddress(), pageB);

  const bool validA = stateA == PageState::Valid;
  const bool validB = stateB == PageState::Valid;
  const bool incompatiblePage =
      stateA == PageState::Incompatible || stateB == PageState::Incompatible;
  if (!validA && !validB) {
    if (incompatiblePage) {
      // Future-format evidence always wins over "corrupt" classification:
      // overwriting either page could destroy the only newer valid snapshot.
      _initializationResult = CalibrationStoreResult::IncompatibleFormat;
    } else if (stateA == PageState::Erased && stateB == PageState::Erased) {
      _initializationResult = CalibrationStoreResult::StorageEmpty;
      _readable = true;
    } else {
      _initializationResult = CalibrationStoreResult::CorruptStorage;
      _readable = true;
    }
    return _initializationResult;
  }

  const FlashSnapshot *selected = nullptr;
  if (validA && validB) {
    if (calibration_record::sequenceIsNewer(pageB.sequence, pageA.sequence)) {
      selected = &pageB;
      _activePageAddress = pageBAddress();
    } else {
      selected = &pageA;
      _activePageAddress = pageAAddress();
    }
  } else if (validA) {
    selected = &pageA;
    _activePageAddress = pageAAddress();
  } else {
    selected = &pageB;
    _activePageAddress = pageBAddress();
  }

  _activeSequence = selected->sequence;
  _validSensorMask = calibration_record::decode(*selected, _sensorData);
  _readable = true;
  if (incompatiblePage) {
    // A downgraded firmware image must never alternate into and erase a page
    // that belongs to a newer storage format. Current-format data remains
    // readable, but writes require an explicit future migration/reset path.
    _initializationResult = CalibrationStoreResult::IncompatibleFormat;
    return _initializationResult;
  }

  const bool invalidOtherPage =
      (validA && stateB != PageState::Valid && stateB != PageState::Erased) ||
      (validB && stateA != PageState::Valid && stateA != PageState::Erased);
  _initializationResult = invalidOtherPage
                              ? CalibrationStoreResult::RecoveredFromCorruption
                              : CalibrationStoreResult::Ok;
  return _initializationResult;
}

CalibrationStoreResult
CalibrationStore::readSensor(SensorId sensor,
                             SensorCalibrationData &data) const noexcept {
  if (_initializationResult == CalibrationStoreResult::NotInitialized) {
    return CalibrationStoreResult::NotInitialized;
  }
  if (!_readable) {
    return CalibrationStoreResult::IncompatibleFormat;
  }
  if (!isValidSensor(sensor)) {
    return CalibrationStoreResult::InvalidSensor;
  }
  const uint8_t sensorMask =
      static_cast<uint8_t>(1U << static_cast<uint8_t>(sensor));
  if ((_validSensorMask & sensorMask) == 0U) {
    return CalibrationStoreResult::CalibrationNotFound;
  }
  data = _sensorData[static_cast<uint8_t>(sensor)];
  return CalibrationStoreResult::Ok;
}

CalibrationStoreResult
CalibrationStore::saveSensor(SensorId sensor,
                             const SensorCalibrationData &data) noexcept {
  if (_initializationResult == CalibrationStoreResult::NotInitialized) {
    return CalibrationStoreResult::NotInitialized;
  }
  if (_initializationResult == CalibrationStoreResult::IncompatibleFormat) {
    return CalibrationStoreResult::IncompatibleFormat;
  }
  if (!isValidSensor(sensor)) {
    return CalibrationStoreResult::InvalidSensor;
  }
  std::array<SensorCalibrationData, SensorCount> candidate = _sensorData;
  const uint8_t sensorIndex = static_cast<uint8_t>(sensor);
  const uint8_t sensorMask = static_cast<uint8_t>(1U << sensorIndex);
  const SensorCalibrationData &existing = candidate[sensorIndex];
  if ((_validSensorMask & sensorMask) != 0U &&
      existing.currentLedCode == data.currentLedCode &&
      existing.voltageTripCode == data.voltageTripCode) {
    return CalibrationStoreResult::Ok;
  }
  candidate[sensorIndex] = data;
  const uint8_t candidateMask =
      static_cast<uint8_t>(_validSensorMask | sensorMask);

  const CalibrationStoreResult result = writeSnapshot(candidate, candidateMask);
  if (result == CalibrationStoreResult::Ok) {
    _sensorData = candidate;
    _validSensorMask = candidateMask;
  }
  return result;
}

CalibrationStoreResult CalibrationStore::initializationResult() const noexcept {
  return _initializationResult;
}

uint32_t CalibrationStore::flashErrorFlags() const noexcept {
  return _flashErrorFlags;
}

bool CalibrationStore::isValidSensor(SensorId sensor) noexcept {
  return static_cast<uint8_t>(sensor) < SensorCount;
}

void CalibrationStore::resetData() noexcept {
  _sensorData = {};
  _validSensorMask = 0U;
}

CalibrationStoreResult CalibrationStore::writeSnapshot(
    const std::array<SensorCalibrationData, SensorCount> &candidate,
    uint8_t validSensorMask) noexcept {
  _flashErrorFlags = 0U;
  uint32_t nextSequence = _activeSequence + 1U;
  if (nextSequence == 0U) {
    nextSequence = 1U;
  }

  const uintptr_t targetAddress =
      _activePageAddress == pageAAddress() ? pageBAddress() : pageAAddress();
  const FlashSnapshot snapshot =
      calibration_record::encode(candidate, validSensorMask, nextSequence);
  CalibrationStoreResult result = CalibrationStoreResult::Ok;

  if (HAL_FLASH_Unlock() != HAL_OK) {
    _flashErrorFlags = HAL_FLASH_GetError();
    return CalibrationStoreResult::FlashUnlockFailed;
  }

  if (erasePage(targetAddress) != HAL_OK || !pageIsErased(targetAddress)) {
    _flashErrorFlags = HAL_FLASH_GetError();
    result = CalibrationStoreResult::FlashEraseFailed;
  }

  constexpr size_t bodySize = offsetof(FlashSnapshot, commitMarker);
  if (result == CalibrationStoreResult::Ok &&
      programBytes(targetAddress, reinterpret_cast<const uint8_t *>(&snapshot),
                   bodySize) != HAL_OK) {
    _flashErrorFlags = HAL_FLASH_GetError();
    result = CalibrationStoreResult::FlashProgramFailed;
  }

  if (result == CalibrationStoreResult::Ok &&
      std::memcmp(reinterpret_cast<const void *>(targetAddress), &snapshot,
                  bodySize) != 0) {
    result = CalibrationStoreResult::FlashVerificationFailed;
  }

  if (result == CalibrationStoreResult::Ok &&
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                        targetAddress + offsetof(FlashSnapshot, commitMarker),
                        snapshot.commitMarker) != HAL_OK) {
    _flashErrorFlags = HAL_FLASH_GetError();
    result = CalibrationStoreResult::FlashProgramFailed;
  }

  if (result == CalibrationStoreResult::Ok &&
      std::memcmp(reinterpret_cast<const void *>(targetAddress), &snapshot,
                  sizeof(snapshot)) != 0) {
    result = CalibrationStoreResult::FlashVerificationFailed;
  }

  if (HAL_FLASH_Lock() != HAL_OK) {
    _flashErrorFlags = HAL_FLASH_GetError();
    if (result == CalibrationStoreResult::Ok) {
      result = CalibrationStoreResult::FlashLockFailed;
    }
  }

  if (result == CalibrationStoreResult::Ok) {
    _activePageAddress = targetAddress;
    _activeSequence = nextSequence;
  }
  return result;
}

} // namespace dda
