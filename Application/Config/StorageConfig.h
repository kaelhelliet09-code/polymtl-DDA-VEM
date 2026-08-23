/**
 * @file StorageConfig.h
 * @brief Application and calibration Flash layout.
 *
 * @warning These constants must match `linker/STM32G0B1_DDA.ld`. Changing
 * only this header can make calibration erases overlap the application.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace dda::config {

inline constexpr uintptr_t FlashStartAddress = 0x08000000UL;
///< First byte of STM32G0B1 internal Flash.
inline constexpr size_t PhysicalFlashSizeBytes = 128U * 1024U;
///< Total physical Flash capacity of the selected MCU.
inline constexpr size_t ApplicationFlashSizeBytes = 124U * 1024U;
///< Linkable firmware region, excluding both calibration pages.
inline constexpr size_t CalibrationPageSizeBytes = 2U * 1024U;
///< Physical erase-page size and size of each A/B calibration region.
inline constexpr uintptr_t ApplicationFlashEndAddress =
    FlashStartAddress + ApplicationFlashSizeBytes;
///< First address outside application Flash.
///< This is also the first byte of calibration page A.
inline constexpr uintptr_t CalibrationPageAAddress =
    ApplicationFlashEndAddress;
///< Calibration page A start.
///< Derived; do not enter a separate hard-coded address here.
inline constexpr uintptr_t CalibrationPageBAddress =
    CalibrationPageAAddress + CalibrationPageSizeBytes;
///< Calibration page B start.
///< Derived as the page immediately following calibration page A.
inline constexpr uintptr_t PhysicalFlashEndAddress =
    FlashStartAddress + PhysicalFlashSizeBytes;
///< First address outside physical Flash.
///< All application and calibration regions must end below this address.

static_assert((CalibrationPageAAddress % CalibrationPageSizeBytes) == 0U &&
                  (CalibrationPageBAddress % CalibrationPageSizeBytes) == 0U,
              "Calibration pages must be 2 KiB aligned");
static_assert(CalibrationPageAAddress == ApplicationFlashEndAddress &&
                  CalibrationPageBAddress ==
                      CalibrationPageAAddress + CalibrationPageSizeBytes,
              "Application and calibration regions must be adjacent");
static_assert(CalibrationPageBAddress + CalibrationPageSizeBytes ==
                  PhysicalFlashEndAddress,
              "The second calibration page must end at physical Flash end");

} // namespace dda::config
