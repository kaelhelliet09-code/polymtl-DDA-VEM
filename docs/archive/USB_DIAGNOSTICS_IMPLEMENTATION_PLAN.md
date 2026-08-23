# Historical USB, DMA Diagnostics, and Timestamp Plan

> This document records an earlier implementation plan. It is retained only
> for design history and is not the current firmware contract. The root
> `README.md`, user-code API documentation, tests, and compiled behavior are
> authoritative.

This plan reconstructs the interrupted Codex Plan Mode result from the persisted
root session and its three completed subsystem reports. It is based on the
current `usb` branch, the checked-in firmware, and the existing uncommitted
changes. It does not require an RTOS, dynamic allocation, memory-to-memory DMA,
or a second packet class. DFU and boot-mode changes are intentionally deferred.

## Decisions fixed by this plan

- Keep the existing `UsbPaquet` as the canonical packet container.
- Preserve the existing packet numeric values. Add no DFU packet type in this
  phase.
- Encode every USB `uint16_t` little-endian; decode INA226 register bytes
  MSB-first before placing their values in a USB packet.
- Use request-driven, one-snapshot-ahead acquisition. A measurement request
  supplies the selection mask, the device prepares and sends that snapshot,
  then immediately prepares one replacement snapshot via DMA.
- The host sends no acquisition frequency or timing configuration.
- Keep ADC DMA in normal mode and use I2C1 RX DMA only for INA226 register
  reads.
- Use TIM2 as a shared 1 MHz, 32-bit free-running timestamp counter. Remove
  TIM3 from the diagnostic design.
- Treat PA15 `USB_VSENSE` as active-high VBUS, subject to the hardware
  verification item in section 12. Do not treat normal USB suspend as cable
  removal.
- Preserve all CubeMX-owned paths and keep generated-file edits inside
  recognized `USER CODE` sections.

## 1. Current repository state

### Working tree and build

The repository is on branch `usb`. The pre-existing uncommitted work must be
preserved:

- deleted `Core/Inc/Utils/MeasBuffer.h`;
- modified `DDA.ioc`, currently adding I2C1 RX and TX DMA;
- formatting-only modification in `Core/Inc/main.h`.

The obsolete `MeasBuffer.h` is not included by current code. Its deletion is
consistent with retaining `UsbPaquet` as the only measurement packet
abstraction.

The root `CMakeLists.txt`:

- recursively discovers every `Core/Src/**/*.cpp`;
- explicitly and redundantly lists the current `USB.h` and `USB.cpp`;
- manually lists the Cube-generated CDC files and ST USB middleware sources;
- receives `Core/Inc` as an include root through the `stm32cubemx` target.

`STM32G0B1xx_FLASH.ld` currently exposes 128 KiB of flash at `0x08000000` and
144 KiB of RAM at `0x20000000`. No flash page, `.noinit` area, EEPROM
emulation, or other persistent configuration is currently reserved.

### Current application flow

`Core/Src/main.c` initializes GPIO, DMA, ADC, DAC, I2C, SPI, TIM2, USB PCD, and
TIM3. Its `USER CODE BEGIN 2` block then calls:

1. `main_cpp()`;
2. `MX_USB_Device_Init()`.

`Core/Src/App/main_cpp.cpp` initializes the coil controller, starts the
nonblocking diagnostics initialization, and resets the application USB state.
Every foreground pass calls:

1. `dda::diagnostics.process()`;
2. `UsbCommunication_Process()`.

This foreground service model is appropriate and should be retained.

### Current packet and USB implementation

`Core/Inc/Utils/USBpaquet.h` defines the canonical `UsbPaquet`:

- `MaximumDataWords = 8`;
- packet type, one options byte, and eight `uint16_t` words;
- measurement word count as the population count of the options mask;
- no raw-object transmission.

Existing packet type values are:

| Symbol | Value |
|---|---:|
| `MEASUREMENT` | `0x00` |
| `COMMAND` / `CONTROL` | `0x01` |
| current forward value | `0x01` |
| current reverse value | `0x02` |
| current stop value | `0x03` |
| `MEASUREMENT_SETUP` | `0x10` |
| `MEASUREMENT_REQUEST` | `0x11` |
| `MEASUREMENT_STOP` | `0x12` |

The current measurement-only `UsbPaquet::dataWordCount()` rule remains
appropriate because requests carry no data words and the host sends no setup
frequency.

`Core/Src/Interfaces/External/USB.cpp` already provides:

- a 128-byte fixed RX ring with 127 usable bytes;
- partial-packet retention and multiple-packet accumulation;
- explicit serialization to `[type][options][little-endian words]`;
- a maximum 18-byte application TX buffer;
- `Empty`, `Ready`, and `InFlight` TX states;
- retry on `USBD_BUSY`;
- immutable TX-buffer ownership until CDC transmit completion;
- foreground command processing.

The generated CDC receive and transmit-complete callbacks are wired through
`UsbCommunicationBridge.h`. The middleware retains the application TX pointer,
so the existing immutable-buffer rule is necessary and correct.

Current gaps and mismatches are:

- `dda::UsbCommand` duplicates values already owned by the canonical protocol;
- coil actions are currently interpreted as packet types instead of
  `[COMMAND][action]`;
- setup frequency is stored separately by USB but should be removed from the
  active protocol;
- measurement request starts a fresh acquisition, but the current design does
  not prepare the following snapshot after sending the response;
- the parser/transport and command owners are combined in one file-static
  implementation;
- DFU and boot-mode support do not exist and remain outside this phase;
- every USB suspend is incorrectly treated as disconnect;
- no TX-completion timeout exists;
- no host protocol tests exist.

### Current diagnostics and peripheral behavior

`Core/Inc/Modules/Diagnostics.h` and
`Core/Src/Modules/Diagnostics.cpp` define `dda::Diagnostics` and the global
`dda::diagnostics`.

The selection masks currently consume all eight bits:

| Option | Mask | Source |
|---|---:|---|
| `POWER_BOARD` | `0x80` | INA226 power register |
| `VOLTAGE_RAIL` | `0x40` | INA226 bus-voltage register |
| `CURRENT_SHUNT` | `0x20` | INA226 current register |
| `CURRENT_H1` | `0x10` | ADC |
| `CURRENT_H2` | `0x08` | ADC |
| `CURRENT_H3` | `0x04` | ADC |
| `CURRENT_H4` | `0x02` | ADC |
| `TIME_STAMP` | `0x01` | firmware timestamp |

Current acquisition is one-shot. `startAcquisition()` starts selected INA226
reads and one four-channel ADC scan, while `process()` assembles a packet after
all requested sources complete. `takeCompleted()` consumes the one stored
packet, preventing repeated requests from returning the same coherent
snapshot.

The ADC is already configured appropriately for per-snapshot DMA:

- ADC clock: 8 MHz;
- 12-bit, right aligned;
- software trigger;
- continuous conversion disabled;
- selected fixed-sequencer channels: 3, 8, 15, 18;
- DMA1 Channel 2, normal mode, halfword alignment, memory increment, medium
  priority;
- `DMA1_Channel2_3_IRQn` priority 0.

The fixed ADC scan order and logical current mapping are:

| DMA index | Channel/pin | Logical value |
|---:|---|---|
| 0 | ADC_IN3 / PA3 | H1 |
| 1 | ADC_IN8 / PB0 | H3 |
| 2 | ADC_IN15 / PB11 | H4 |
| 3 | ADC_IN18 / PC5 | H2 |

INA226 access currently uses interrupt-driven, two-byte register transactions.
INA bytes are already decoded MSB-first. Selected registers are read
sequentially because the INA226 register pointer does not auto-increment across
power, bus-voltage, and current registers. A six-byte contiguous DMA read is
therefore not valid.

The dirty `.ioc` contains both I2C1 RX DMA2 Channel 1 and I2C1 TX DMA2 Channel
2, but generated C code has not been refreshed and the handwritten I2C driver
still calls the `_IT` APIs.

TIM2 is the only 32-bit general-purpose timer. It is configured for PC6/PC7
input capture on channels 3 and 4 with a 16 MHz counter, but no code starts it.
TIM3 is an unused 16-bit base timer. The APB timer clock is 16 MHz.

## 2. Proposed architecture and readability

### Component ownership

`UsbPaquet`

- Remains the only packet container.
- Owns packet type, options, measurement word count, and up to eight words.
- Does not know about CDC, DMA, coils, or boot behavior.

`UsbProtocol`

- Owns exact wire lengths, little-endian encoding/decoding, selection ordering,
  status values, and the fixed stream accumulator.
- Converts complete request bytes into the existing `UsbPaquet`.
- Has no HAL dependency so it can be host tested.

`UsbService`

- Owns the CDC RX/TX transport state and one immutable 18-byte TX buffer.
- Pulls complete requests from `UsbProtocol`.
- Dispatches to the coil controller or diagnostics.
- Queues at most one response and retains later request bytes until TX becomes
  available.
- Never performs a blocking wait in a callback or the main loop.

`Diagnostics`

- Owns the requested selection, ADC/I2C acquisition state, timeouts, staging
  buffers, one prepared snapshot, and one pending measurement request.
- Acquires on demand and prepares exactly one replacement snapshot after each
  response.
- Does not know about USB serialization.

`Timestamp`

- Owns starting and reading TIM2.
- Expresses internal timestamps and durations in 32-bit microseconds.
- Provides wrap-safe unsigned elapsed-time calculation.
- Does not depend on USB.

Generated C callback bridges

- Copy USB RX bytes or set small completion/disconnect flags.
- Set ADC and I2C completion/error flags after filtering the HAL instance.
- Never serialize, publish a packet, start another transfer, or transmit USB
  from interrupt context.

### Foreground sequence

Initialization:

1. `HAL_Init()`.
2. Configure system clock and CubeMX peripherals.
3. Start TIM2 through `dda::timestamp.start()`.
4. Initialize coils, diagnostics, and `dda::usbService`.
5. Start USB CDC with `MX_USB_Device_Init()`.

Every main-loop pass:

1. `dda::diagnostics.process()` consumes completion/DMA flags, advances INA
   transactions, handles timeouts, and completes requested/prefetched snapshots.
2. `dda::usbService.process()` handles disconnect state, RX parsing,
   dispatch, TX retry/completion, and pending measurement/stop responses.

This keeps state transitions short and explicit. No general event framework,
inheritance hierarchy, dynamic allocation, or template timer abstraction is
needed.

### State machines

Acquisition state:

- `Idle`
- `AcquiringForRequest`
- `Prefetching`
- `SnapshotReady`
- `Cancelling`
- `Faulted`

TX state:

- `Empty`
- `Ready`
- `InFlight`

The implementation should use scoped enums and one required/ready source
bitmask rather than many unrelated booleans.

## 3. Protocol definition

### Types and actions

Retain the existing `type` enum values without adding a DFU type.
`MEASUREMENT_SETUP (0x10)` remains reserved for compatibility but is not
accepted by this phase.

Coil action values remain numerically unchanged but become options of
`COMMAND`, not packet types:

```cpp
enum class CoilAction : uint8_t {
  Forward = 0x01U,
  Reverse = 0x02U,
  Stop = 0x03U,
};
```

After migrating call sites, remove the duplicate `dda::UsbCommand`. If source
compatibility is temporarily required, keep aliases for one migration commit
only; do not retain two canonical enums.

Status remains:

```cpp
enum class UsbStatus : uint8_t {
  Succeeded = 0x00U,
  Failed = 0x01U,
};
```

### Host-to-device packets

| Operation | Bytes | Length |
|---|---|---:|
| Coil forward | `[0x01][0x01]` | 2 |
| Coil reverse | `[0x01][0x02]` | 2 |
| Coil stop | `[0x01][0x03]` | 2 |
| Measurement request | `[0x11][selection]` | 2 |
| Measurement stop | `[0x12][0x00]` | 2 |

Every multi-byte USB value is little-endian.

Measurement request accepts:

- nonzero selection;
- no bits outside `Diagnostics::SupportedOptionsMask`.

### Device-to-host packets

Successful measurement request:

```text
[0x00][selection][word_0_lo][word_0_hi] ... [word_n_lo][word_n_hi]
```

Length is `2 + 2 * popcount(selection)`, with a maximum of 18 bytes.

Status response:

```text
[request_type][status]
```

The status table is:

| Request | Success | Failure |
|---|---|---|
| command | `[0x01][0x00]` | `[0x01][0x01]` |
| measurement request | measurement packet | `[0x11][0x01]` |
| stop | `[0x12][0x00]` | `[0x12][0x01]` |

`MEASUREMENT_REQUEST` returns a measurement packet on success, not a separate
success status.

### Measurement word ordering

Iterate masks from bit 7 to bit 0. Include a word only when that bit is
selected:

| Order | Mask | Word |
|---:|---:|---|
| 0 | `0x80` | raw INA226 power |
| 1 | `0x40` | raw INA226 bus voltage |
| 2 | `0x20` | raw INA226 current |
| 3 | `0x10` | ADC H1 |
| 4 | `0x08` | ADC H2 |
| 5 | `0x04` | ADC H3 |
| 6 | `0x02` | ADC H4 |
| 7 | `0x01` | acquisition-start timestamp in milliseconds modulo 65536 |

INA226 two-byte reads are decoded MSB-first into host-native `uint16_t` values.
The resulting words are then serialized USB little-endian like every other
word.

### `UsbPaquet` use

Keep the existing word-count rule:

- `MEASUREMENT`: population count of options;
- all command/status/request types: zero words.

Keep the eight-word fixed array. Do not expose or transmit object memory.

### Stream behavior

- The CDC callback appends every received byte to the fixed 128-byte
  accumulator and immediately rearms the generated RX buffer.
- A partial known packet remains buffered.
- Multiple packets in one CDC reception are processed in wire order.
- Foreground processes packets until the accumulator is incomplete or the
  single TX response slot is unavailable.
- A second request received while TX is `Ready` or `InFlight` remains in the
  accumulator. It is not dropped and cannot overwrite the TX buffer.
- An unknown leading type byte is discarded one byte at a time to resynchronize.
- A known packet with an invalid action, reserved options byte, or selection
  receives `Failed`.
- On accumulator overflow, foreground clears the accumulator and records a
  protocol-overflow diagnostic. No response is sent because packet boundaries
  are no longer trustworthy.
- If a recognized partial packet remains incomplete for 100 ms, discard its
  leading byte and resume type scanning. This prevents a truncated packet from
  blocking the stream indefinitely.

Adding a future diagnostic operation should require only:

1. a new `type` numeric value;
2. its request length/word-count rule;
3. one dispatch case calling its owner;
4. serialization tests.

The accumulator, CDC state, and measurement coordinator must not need redesign.

## 4. Acquisition and concurrency design

### Request and one-snapshot-ahead flow

The host controls when it wants a response, but it does not send a frequency,
period, deadline, or any other request-timing information. Each
`MEASUREMENT_REQUEST` is exactly `[0x11][selection]`.

For each valid request:

1. Validate that `selection` is nonzero and contains only supported bits.
2. If a complete prepared snapshot exists for the same selection, serialize it
   into the independent USB TX buffer, consume it, and begin preparing exactly
   one replacement snapshot immediately.
3. If a snapshot for the same selection is already being prepared, hold the
   request as the single pending measurement request. Send that snapshot when
   acquisition completes, then immediately begin preparing one replacement.
4. If no snapshot exists and no acquisition is active, hold the request and
   begin its acquisition.
5. If the selection differs from a prepared snapshot, discard the obsolete
   snapshot and acquire the newly requested selection.
6. If the selection differs while acquisition is active, cancel the obsolete
   acquisition safely, then start the newly requested selection. USB request
   ordering and TX backpressure allow only one pending measurement response at
   a time.

The first request therefore waits nonblockingly for hardware completion; the
absence of a prepared snapshot is not an error. After the requested snapshot is
copied into USB-owned TX storage, acquisition of the next snapshot begins
immediately and may overlap USB transmission. When that prefetch completes, it
becomes `SnapshotReady` and acquisition stops. The device stays exactly one
snapshot ahead; it does not acquire continuously while the host is idle.

A later request with the same selection can send the prepared snapshot
immediately and starts the next prefetch. A changed selection invalidates the
old pipeline and makes the new request wait for a matching acquisition.

### Acquisition start

For each requested or prefetched acquisition:

1. Derive `requiredSources`:
   - INA required if any of `0xE0` is selected;
   - ADC required if any of `0x1E` is selected;
   - timestamp requires no asynchronous source.
2. Clear `readySources` and per-acquisition error flags.
3. Capture `snapshotTimestampUs = timestamp.now()` immediately before starting
   either hardware source.
4. Start the selected INA226 register sequence using
   `HAL_I2C_Mem_Read_DMA()`.
5. Start one four-channel `HAL_ADC_Start_DMA()` scan if any ADC current is
   selected.
6. If neither source is required, publish the timestamp-only snapshot
   immediately.

ADC and I2C proceed independently. Do not change ADC CHSELR per selection; a
four-halfword scan is simpler and takes only about 7 microseconds nominally.
Do not use circular ADC DMA or memory-to-memory DMA.

### Completion, response, and prefetch

- `HAL_ADC_ConvCpltCallback()` filters `hadc1` and sets an ADC-complete flag.
- `HAL_ADC_ErrorCallback()` filters `hadc1` and sets an ADC-error flag.
- Existing I2C completion callbacks continue to advance the INA226 state
  machine; after all selected registers are decoded, foreground marks INA ready.
- Foreground consumes volatile ISR flags in a short critical section to avoid
  read/modify/write races.
- When `readySources == requiredSources`, foreground copies selected values
  into a local eight-word array in exact protocol order and constructs one
  `UsbPaquet(type::MEASUREMENT, selection, words)`.
- If a request is pending, foreground gives the completed packet to
  `UsbService`. `UsbService` serializes it into its independent TX buffer before
  `Diagnostics` reuses ADC/INA staging storage.
- As soon as the response is accepted into USB-owned storage, `Diagnostics`
  starts exactly one replacement prefetch with the same selection.
- If no request is pending, the completed prefetch becomes the sole prepared
  `SnapshotReady` packet and acquisition becomes idle.

Only foreground constructs, publishes, and serializes packets. One persistent
prepared `UsbPaquet`, one selection value, and a ready flag are sufficient;
diagnostic double buffering is unnecessary because the USB service owns a
separate serialized TX buffer.

### Stop and selection changes during DMA

`MEASUREMENT_STOP` is idempotent:

1. Clear the pending request and invalidate the prepared snapshot and selection.
2. Stop ADC DMA synchronously.
3. Ask INA226 to cancel the register sequence.
4. Let the current two-byte I2C DMA transaction finish and discard it.
5. If it does not finish within 10 ms, use
   `HAL_I2C_Master_Abort_IT()` and handle `HAL_I2C_AbortCpltCallback()`.
6. Report stop success only when both hardware paths are idle.

A request whose selection differs from the active acquisition uses the same
cancellation path, then starts a requested acquisition for the new selection.
Do not send an incomplete or obsolete-selection snapshot.

### Error recovery

- ADC error: stop DMA and discard the incomplete snapshot. For an acquisition
  serving a pending request, perform at most one clean retry and then return
  `[0x11][0x01]`. For a prefetch, invalidate it and let the next request start a
  fresh acquisition.
- I2C/DMA error: use the same requested-acquisition retry or prefetch discard
  policy and return the INA state machine to ready when recoverable.
- Acquisition timeout: use a 10 ms timestamp deadline. Abort I2C
  nonblockingly. If abort does not complete, mark INA unavailable and stop
  acquisition rather than remaining busy indefinitely.
- A later valid request may retry initialization/recovery.
- Never send or retain a packet after any source error in that acquisition.

## 5. Timestamp design

Add a small `Timestamp` class:

```cpp
namespace dda {

class Timestamp {
public:
  using Tick = uint32_t;

  explicit Timestamp(TIM_HandleTypeDef &timer) noexcept;
  HAL_StatusTypeDef start() noexcept;
  Tick now() const noexcept;  // microseconds

  static constexpr Tick elapsed(Tick start, Tick end) noexcept {
    return end - start;
  }

  static constexpr uint16_t toMeasurementWord(Tick timestampUs) noexcept {
    return static_cast<uint16_t>(timestampUs / 1000U);
  }

private:
  TIM_HandleTypeDef &timer_;
};

extern Timestamp timestamp;

} // namespace dda
```

TIM2 settings:

- input clock: 16 MHz;
- prescaler: 15;
- counter clock: 1 MHz;
- period: `0xFFFFFFFF`;
- resolution: 1 microsecond;
- wrap: 4,294,967,296 microseconds, or 71 minutes 34.967296 seconds;
- no update interrupt and no software overflow extension.

`now()` reads the 32-bit hardware counter directly. This is short and safe from
foreground or an ISR. Unsigned `end - start` is correct across one wrap as long
as the measured interval is less than one full 71.6-minute period.

TIM2 channels 3 and 4 remain available for PC6/PC7 velocity input capture and
their capture values become microseconds. TIM2 ownership must be centralized:
future velocity code may enable/disable capture channels but must not stop the
base timer owned by `Timestamp`.

The current USB timestamp convention remains milliseconds modulo 65536 through
`toMeasurementWord()`. Internal operation durations remain 32-bit
microseconds. A future action-duration diagnostic records:

```cpp
const auto start = timestamp.now();
// operation
const auto durationUs = Timestamp::elapsed(start, timestamp.now());
```

It can later place an explicitly scaled value into a `UsbPaquet` without
changing the request-driven acquisition pipeline.

## 6. DFU scope

DFU is deferred and must not be implemented in this phase.

- Add no DFU packet type or command handler.
- Add no `BootMode` module, boot marker, flash erase/write logic, reset
  sequence, or ROM-bootloader jump.
- Do not reserve a flash page or modify `STM32G0B1xx_FLASH.ld`.
- Do not change BOOT option bytes.
- Do not add a DFU USB class, second `.ioc`, second firmware target, or
  CDC/DFU composite device.
- Keep normal startup and USB CDC behavior unchanged apart from the CDC,
  diagnostics, and disconnect corrections described elsewhere in this plan.

When DFU becomes a separate approved feature, it should receive its own
repository inspection, protocol decision, persistence requirement, safety
review, and on-target recovery test plan.

## 7. CubeMX and peripheral changes

### `DDA.ioc`

ADC1:

- Keep channels 3, 8, 15, and 18.
- Keep fixed scan, software trigger, sequence EOC, normal DMA, and medium DMA
  priority.
- Keep DMA1 Channel 2, halfword alignment, memory increment.
- Do not enable continuous conversion, circular DMA, or timer-triggered ADC.

I2C1:

- Keep 100 kHz timing `0x00503D58`.
- Keep the I2C1 event/error IRQ at priority 1.
- Keep/add I2C1 RX DMA on DMA2 Channel 1:
  - peripheral-to-memory;
  - byte alignment;
  - memory increment;
  - normal mode;
  - medium priority.
- Remove the dirty I2C1 TX DMA assignment. INA initialization writes are rare
  and remain interrupt-driven.

DMA/NVIC:

- Enable DMA2 clock through regenerated code.
- Enable `DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn` at priority 0.
- Keep `DMA1_Channel2_3_IRQn` at priority 0.
- Keep USB and I2C at priority 1.
- Keep SysTick at priority 3.

TIM2:

- Keep input capture on CH3/PC6 and CH4/PC7.
- Remove stale/conflicting PWM CH3 metadata if CubeMX still exposes it.
- Set prescaler 15 and ARR `0xFFFFFFFF`.
- No TIM2 update interrupt.

TIM3:

- Remove or disable TIM3 in `DDA.ioc`; diagnostic acquisition is not
  timer-scheduled.
- Remove `TIM3_TIM4_IRQn` from the diagnostics design.
- Keep ADC software-triggered.

USB:

- Keep Full-Speed CDC, HSI48/CRS, PA11/PA12, and interrupt priority 1.
- USB uses PMA/interrupts, not a general-purpose DMA channel.
- Keep PA15 `USB_VSENSE` as GPIO input and poll it from foreground. No new EXTI
  is required.
- Keep the normal CDC build.

### Expected generated changes

CubeMX regeneration should update:

- `Core/Src/main.c` TIM2 values, TIM3 removal, and DMA2 initialization;
- `Core/Src/stm32g0xx_hal_msp.c` I2C RX DMA allocation/linking;
- `Core/Src/stm32g0xx_it.c` combined DMA2 handler and removal of TIM3 handling;
- `Core/Inc/stm32g0xx_it.h` declarations;
- related generated handle declarations.

Handwritten behavior belongs in:

- `USER CODE` includes/calls in `main.c`;
- the existing generated USB callback `USER CODE` blocks;
- independent C++ files under `Core/Src`.

After changing `.ioc`, regenerate once and verify that
`MX_USB_DRD_FS_PCD_Init()` remains generated and called before
`MX_USB_Device_Init()`.

## 8. File and folder reorganization

### Current relevant tree

```text
Core/
  Inc/
    App/main_cpp.h
    Interfaces/
      External/
        INA226.h
        USB.h
        UsbCommunicationBridge.h
      Internal/
        ADC.h
        I2C.h
    Modules/
      CoilController.h
      Diagnostics.h
    Utils/
      USBpaquet.h
  Src/
    App/main_cpp.cpp
    Interfaces/
      External/
        INA226.cpp
        USB.cpp
      Internal/
        ADC.cpp
        I2C.cpp
    Modules/
      CoilController.cpp
      Diagnostics.cpp
USB_DEVICE/                 # CubeMX generated
Drivers/                    # STM32 generated/vendor
Middlewares/                # ST USB middleware
cmake/stm32cubemx/          # CubeMX generated build input
```

Problems:

- USB protocol, transport, parsing, dispatch, and application ownership are
  concentrated in one `USB.cpp`.
- `USBpaquet.h` sits in generic `Utils` despite defining the public protocol.
- a trivial `USB` compatibility class hides file-static service state;
- platform-wide timestamp responsibility has no home;
- generated and handwritten USB files are not visually distinguished by
  includes/naming.

### Target tree

```text
Core/
  Inc/
    App/
    Interfaces/
      External/INA226.h
      Internal/I2C.h
    Modules/
      CoilController.h
      Diagnostics.h
    Platform/
      Timestamp.h
    Usb/
      UsbCommunicationBridge.h
      UsbPaquet.h
      UsbProtocol.h
      UsbService.h
  Src/
    App/
    Interfaces/
      External/INA226.cpp
      Internal/I2C.cpp
    Modules/
      CoilController.cpp
      Diagnostics.cpp
    Platform/
      Timestamp.cpp
    Usb/
      UsbProtocol.cpp
      UsbService.cpp
```

Host-side tests and tools are maintained outside this firmware repository in
`C:\Users\kaelh\Desktop\C++\HOST`.

### Old-to-new mapping

| Old path | New path |
|---|---|
| `Core/Inc/Interfaces/External/USB.h` | `Core/Inc/Usb/UsbService.h` |
| `Core/Src/Interfaces/External/USB.cpp` | split into `Core/Src/Usb/UsbService.cpp` and `Core/Src/Usb/UsbProtocol.cpp` |
| `Core/Inc/Interfaces/External/UsbCommunicationBridge.h` | `Core/Inc/Usb/UsbCommunicationBridge.h` |
| `Core/Inc/Utils/USBpaquet.h` | `Core/Inc/Usb/UsbPaquet.h` |
| deleted `Core/Inc/Utils/MeasBuffer.h` | no replacement |

New files:

- `Core/Inc/Usb/UsbProtocol.h`
- `Core/Inc/Platform/Timestamp.h`
- `Core/Src/Platform/Timestamp.cpp`
- `C:\Users\kaelh\Desktop\C++\HOST\CMakeLists.txt`
- `C:\Users\kaelh\Desktop\C++\HOST\UsbProtocolTests.cpp`
- `C:\Users\kaelh\Desktop\C++\HOST\BoardTestHost.cpp`

Files intentionally left in place:

- all `USB_DEVICE/**`, `Drivers/**`, `Middlewares/**`;
- `Core/Src/main.c`, MSP, interrupt, startup, and HAL configuration files;
- `Core/Inc/Modules/Diagnostics.h` and its source;
- INA226 and I2C drivers;
- unrelated application, coil, UI, and hardware-interface files;
- `cmake/stm32cubemx/CMakeLists.txt`.

### Header visibility and build changes

Public application headers:

- `Usb/UsbPaquet.h`
- `Usb/UsbProtocol.h`
- `Usb/UsbService.h`
- `Platform/Timestamp.h`

`UsbCommunicationBridge.h` is a narrow C ABI used only by generated C callback
files. Parser storage, TX storage, and transient state should remain private
members or `.cpp`-private constants.

Update generated `USER CODE` includes to:

```cpp
#include "Usb/UsbCommunicationBridge.h"
```

Update handwritten includes to `Usb/...` and `Platform/...`.

Remove the two explicit old USB paths from `add_executable()` in the root
`CMakeLists.txt`; recursive C++ discovery will find the moved/new sources. Keep
the explicit generated CDC/middleware source list. Do not add handwritten
sources to `cmake/stm32cubemx/CMakeLists.txt`.

Perform moves as a standalone buildable step before behavior changes so rename
errors are isolated from protocol and DMA changes.

## 9. File-by-file implementation plan

`Core/Inc/Usb/UsbPaquet.h`

- Move the existing canonical class.
- Introduce `CoilAction` with values 1, 2, and 3.
- Keep measurement word count derived from the request selection mask.
- Preserve fixed storage and existing numeric measurement values.
- Remove/deprecate semantically incorrect coil packet-type aliases after
  migrating call sites.

`Core/Inc/Usb/UsbProtocol.h` and `Core/Src/Usb/UsbProtocol.cpp`

- Move `UsbStatus`, maximum serialized length, request-length logic, and the
  fixed accumulator here.
- Implement explicit little-endian serialize and complete-request decode.
- Codify descending measurement-bit ordering.
- Expose no HAL or USB middleware types.
- Replace the duplicated `UsbCommand`.

`Core/Inc/Usb/UsbService.h` and `Core/Src/Usb/UsbService.cpp`

- Replace the trivial `USB` wrapper and anonymous file-static ownership with
  one `dda::UsbService` and `extern UsbService usbService`.
- Keep one 18-byte TX buffer and `Empty/Ready/InFlight`.
- Keep retry-on-`USBD_BUSY` and transmit-complete ownership.
- Add a 250 ms TX watchdog without reusing an in-flight buffer prematurely.
- Process complete requests in order until TX backpressure requires a pause.
- Dispatch command/request/stop to their actual owners; reject reserved
  `MEASUREMENT_SETUP`.
- Pass the request selection mask to `Diagnostics`, retain at most one pending
  measurement response, and accept a completed packet into USB-owned TX storage
  before allowing the replacement prefetch to start.
- Remove obsolete `rejectMeasurementRequestPrefixBytes`.
- Poll active-high `USB_VSENSE`; on loss, clear transport state, stop
  diagnostics, invalidate snapshots, and force coils off.

`Core/Inc/Usb/UsbCommunicationBridge.h`

- Preserve the current C ABI names.
- Generated RX, TX-complete, and disconnect callbacks delegate to
  `dda::usbService`.
- Keep callbacks to byte copies and flags only.

`USB_DEVICE/App/usbd_cdc_if.c`

- Change only the bridge include path in its `USER CODE` block.
- Retain receive forwarding, immediate RX rearm, transmit completion, and
  deinitialization forwarding.

`USB_DEVICE/Target/usbd_conf.c`

- Change only the bridge include path as needed.
- Remove `UsbCommunication_OnDisconnect()` from
  `HAL_PCD_SuspendCallback()`; suspend is not cable removal.
- Retain real device-disconnect/deinitialization forwarding.

`Core/Inc/Modules/Diagnostics.h` and
`Core/Src/Modules/Diagnostics.cpp`

- Replace one-shot/take semantics with the request-driven, one-snapshot-ahead
  pipeline.
- Own the active/prepared selection, one prepared packet, one pending request,
  and explicit `Idle`, `AcquiringForRequest`, `Prefetching`, `SnapshotReady`,
  `Cancelling`, and `Faulted` states.
- Add required/ready source masks, timeout state, changed-selection
  cancellation, and safe asynchronous stop.
- Capture a device timestamp at every acquisition start.
- Keep ADC mapping and packet word order explicit.
- Keep global HAL ADC callbacks here, filtered by `hadc1`.
- Do not add a TIM3 callback or any periodic scheduling.

`Core/Inc/Interfaces/Internal/I2C.h` and
`Core/Src/Interfaces/Internal/I2C.cpp`

- Add `readRegisterDma()` using `HAL_I2C_Mem_Read_DMA()`.
- Keep initialization/register writes on `_IT`.
- Reuse the existing MSB-first two-byte decode and completion callbacks.
- Add abort request/completion handling and expose idle/failure state.
- Keep the one-active-transaction restriction explicit.

`Core/Inc/Interfaces/External/INA226.h` and its source

- Change the measurement read path to the I2C RX DMA method.
- Keep power, voltage, and current as separate register transactions.
- Preserve nonblocking initialization and cancellation semantics.
- Ensure aborted/cancelled reads discard partial data and return to a defined
  state.

`Core/Inc/Platform/Timestamp.h` and
`Core/Src/Platform/Timestamp.cpp`

- Implement the interface in section 5 around `htim2`.
- Start the base counter once.
- Provide only explicit microsecond reads, elapsed calculation, and protocol
  conversion.

`Core/Src/main.c`

- Retain `main_cpp()` and `MX_USB_Device_Init()` order after generated
  peripheral initialization.
- Accept CubeMX-generated TIM2/DMA changes and TIM3 removal.

`Core/Src/App/main_cpp.cpp`

- Include the new service/timestamp paths.
- Start `dda::timestamp`.
- Initialize `dda::usbService`.
- Continue calling diagnostics before USB service every loop.

`DDA.ioc`

- Apply section 7 exactly and regenerate.

`Core/Src/stm32g0xx_hal_msp.c`,
`Core/Src/stm32g0xx_it.c`, and related generated headers

- Accept regenerated DMA2 RX setup and removal of TIM3 interrupt setup.
- Ensure the combined DMA handler calls the correct HAL DMA function exactly
  once.

`CMakeLists.txt`

- Remove explicit old handwritten USB paths.
- Retain recursive `Core/Src` discovery and generated USB source lists.

`C:\Users\kaelh\Desktop\C++\HOST\CMakeLists.txt`,
`UsbProtocolTests.cpp`, and `BoardTestHost.cpp`

- Build the HAL-independent packet, codec, and accumulator code natively
  without adding a third-party test framework.
- Cover exact byte vectors, fragmentation, combined packets, malformed input,
  overflow, and response lengths.
- Exercise the board-test handshake and request-driven measurement path through
  the Windows USB CDC serial port.

## 10. Error handling

| Condition | Required behavior |
|---|---|
| empty selection | reject measurement request; do not disturb a valid prepared snapshot |
| unsupported selection bit | reject measurement request; do not start hardware |
| reserved `MEASUREMENT_SETUP` received | return failure/unsupported status; do not change acquisition state |
| first request with no prepared snapshot | hold one response pending, acquire the requested selection, then send it |
| request matching a prepared snapshot | send it immediately, consume it, and start one replacement prefetch |
| request matching an active prefetch | hold one response pending, send that acquisition when complete, then start one replacement prefetch |
| request with a changed selection | discard/cancel obsolete prepared or active work, acquire the new selection, then respond |
| request while USB busy | leave it buffered; process after TX completion |
| stop while ADC active | stop ADC DMA, discard the incomplete snapshot |
| stop while I2C DMA active | request cancel, finish/discard current word, abort after 10 ms |
| partial USB packet | retain up to 100 ms, then discard leading byte and resynchronize |
| multiple USB packets | process in order until TX backpressure |
| unknown USB byte | discard one byte; do not invent an unverifiable response |
| RX accumulator overflow | clear stream and record protocol overflow |
| `USBD_BUSY` | preserve exact TX bytes and retry later |
| no TX-complete callback | retain buffer and recover CDC/disconnect state after 250 ms without reusing in-flight storage |
| USB suspend | do not treat as disconnect |
| PA15 VBUS loss | stop diagnostics and coils, invalidate snapshot, clear pending traffic |
| ADC/DMA error | discard snapshot and stop/clear DMA; retry a pending request once, but discard a failed prefetch |
| I2C/DMA error | discard snapshot; retry a pending request once, but discard a failed prefetch; restore state or fault if unrecoverable |
| acquisition timeout | abort I2C; never remain permanently busy |
| timer wrap | unsigned 32-bit subtraction; valid for intervals below 71.6 minutes |

## 11. Verification plan

### Buildable migration gates

1. Build the untouched dirty baseline and record `.text`, `.data`, `.bss`, and
   map output.
2. Commit the existing `MeasBuffer.h` deletion separately or explicitly include
   it in the first migration commit.
3. Move handwritten USB files and update includes/CMake; build with no behavior
   change.
4. Apply `.ioc` changes, regenerate once, inspect the generated diff, and build.
5. Add the `Timestamp` skeleton; build.
6. Add protocol/codec tests and run them natively.
7. Add request-driven acquisition and one-snapshot-ahead prefetch; build after
   ADC-only, INA-only, and combined stages.

### Host protocol tests

- Serialize selection masks containing each individual bit and all eight bits.
- Verify exact little-endian byte vectors and 2-18 byte lengths.
- Verify every valid request selection and rejection of zero or unsupported
  selection bits.
- Verify reserved `MEASUREMENT_SETUP` is rejected without altering diagnostic
  state.
- Feed every packet one byte at a time.
- Feed several packets in one append.
- Feed a packet split at every possible boundary.
- Feed unknown bytes before valid packets.
- Hold a recognized partial packet past its timeout and verify resynchronization.
- Fill/overflow the ring and verify deterministic reset.
- Verify measurement word order `0x80` through `0x01`.
- Verify that non-measurement packet types never read beyond their defined
  payload.

### On-target acquisition tests

- A timestamp-only request responds with the acquisition-start timestamp and
  prepares one replacement.
- An ADC-only request verifies H1/H2/H3/H4 remapping against injected voltages.
- An INA-only request verifies raw power, bus-voltage, and current values.
- A combined request proves ADC and I2C run independently and responds only
  when both finish.
- Verify the first request waits for acquisition rather than failing.
- Verify each response starts exactly one replacement prefetch.
- Verify the next same-selection request uses the prepared snapshot and starts
  the next prefetch.
- Leave the host idle and confirm acquisition stops after one snapshot is
  prepared.
- Change selection while a snapshot is ready and while ADC or each INA register
  read is active; confirm obsolete data is never sent.
- Stop during ADC DMA and each INA register read.
- Inject ADC and I2C errors and suppress callbacks to exercise timeouts.
- Confirm an incomplete/error acquisition is never sent or retained as ready.

### Timestamp tests

- Compare TIM2 counter rate with a known external interval.
- Verify 1 microsecond resolution and the 71.6-minute wrap calculation.
- Force start/end values around `0xFFFFFFFF` and verify elapsed subtraction.
- Confirm PC6/PC7 capture still works with 1 microsecond tick resolution.
- Measure a known foreground operation and a callback-to-foreground delay.
- Verify the USB timestamp word remains milliseconds modulo 65536.

### USB transport tests

- Request before any sample and verify the response is sent after acquisition
  completes.
- Back-to-back requests while TX is `Ready` and `InFlight`.
- Force repeated `USBD_BUSY`.
- Verify the TX buffer is unchanged until transmit completion.
- Suppress transmit completion to exercise the watchdog.
- Send partial/combined/overflow traffic through real CDC endpoints.
- Suspend and resume without stopping acquisition.
- Disconnect VBUS and verify diagnostics and coils stop.
- Reconnect and verify clean parser/TX state.

## 12. Open questions

These questions do not block implementation; each has a fixed working default.

1. **Is PA15 electrically an active-high VBUS indication?**
   - Default: yes; poll it and stop diagnostics/coils on low.
   - Verification: check the schematic and compare the pin level with cable
     insertion/removal before enabling the safety reaction in production.

2. **Is 1 microsecond sufficient for PC6/PC7 velocity capture?**
   - Default: yes; share TIM2 at 1 MHz.
   - Verification: calculate the maximum expected edge rate and compare
     velocity error against requirements. If insufficient, keep TIM2 at 16 MHz
     and express `Timestamp` in 62.5 ns ticks without changing its ownership.

3. **Does the host expect raw INA226/ADC units or engineering units?**
   - Default: retain existing raw 16-bit values and document scaling on the
     host. Converting units is outside this change.

## Ordered implementation checklist

1. Preserve and document the current dirty worktree.
2. Build the baseline and save size/map results.
3. Move the four handwritten USB headers/sources into `Core/Inc/Usb` and
   `Core/Src/Usb`; update includes and CMake; rebuild.
4. Split pure protocol/accumulator code from CDC service code; add host tests.
5. Extend the existing `UsbPaquet` for selection-derived measurement payloads
   and remove the duplicate command enum.
6. Implement the exact packet table, coil option semantics, validation, byte
   order, and parser timeout.
7. Configure TIM2 at 1 MHz and add `Timestamp`; verify PC6/PC7 capture impact.
8. Remove/disable TIM3 and its interrupt; no request-timing information is sent
   by the host.
9. Keep ADC normal DMA and verify its four-channel logical mapping.
10. Remove I2C TX DMA from `.ioc`, retain I2C RX DMA2 Channel 1, regenerate,
     and build.
11. Change INA226 measurement reads from `_IT` to sequential RX DMA and add
    abort/error handling.
12. Refactor `Diagnostics` to request-driven acquisition, required/ready masks,
    acquisition-start timestamps, one prepared snapshot, one pending request,
    changed-selection cancellation, and timeouts.
13. Make measurement requests send a matching prepared snapshot or wait for a
    new acquisition, then immediately start one replacement prefetch; implement
    stop transitions and USB backpressure.
14. Correct suspend/disconnect behavior and validate active-high PA15 VBUS.
15. Run all host, on-target DMA, timestamp, USB, disconnect, and recovery tests.
16. Perform a final CubeMX regeneration and confirm all `USER CODE` bridges,
    USB PCD initialization, and build paths survive.
