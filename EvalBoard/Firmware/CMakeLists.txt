cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

project(DDA_EvalBoard LANGUAGES C ASM)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(${PROJECT_NAME}
    Core/Src/main.c
    Core/Src/eval_board.c
    Core/Src/stm32g0xx_hal_msp.c
    Core/Src/stm32g0xx_it.c
    USB_DEVICE/App/usb_device.c
    USB_DEVICE/App/usbd_cdc_if.c
    USB_DEVICE/App/usbd_desc.c
    ${REPO_ROOT}/USB_DEVICE/Target/usbd_conf.c
    ${REPO_ROOT}/Core/Src/system_stm32g0xx.c
    ${REPO_ROOT}/Core/Src/syscalls.c
    ${REPO_ROOT}/Core/Src/sysmem.c
    ${REPO_ROOT}/startup_stm32g0b1xx.s
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_cortex.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash_ex.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pcd.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pcd_ex.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr_ex.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_tim.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_tim_ex.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_rcc.c
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_usb.c
    ${REPO_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    ${REPO_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    ${REPO_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    ${REPO_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    USB_DEVICE/App
    USB_DEVICE/Target
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Inc
    ${REPO_ROOT}/Drivers/STM32G0xx_HAL_Driver/Inc/Legacy
    ${REPO_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${REPO_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    ${REPO_ROOT}/Drivers/CMSIS/Device/ST/STM32G0xx/Include
    ${REPO_ROOT}/Drivers/CMSIS/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32G0B1xx
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -ffunction-sections
    -fdata-sections
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T${REPO_ROOT}/linker/STM32G0B1_DDA.ld
    --specs=nano.specs
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

target_link_libraries(${PROJECT_NAME} m)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating Eval Board firmware images"
)
