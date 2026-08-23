add_library(dda_usb_protocol STATIC
    Service/Usb/UsbProtocol.cpp
)

target_include_directories(dda_usb_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(dda_calibration_codec STATIC
    Service/Calibration/CalibrationRecordCodec.cpp
)

target_include_directories(dda_calibration_codec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(DDA_BUILD_HOST_TESTS_ONLY)
    dda_enable_strict_warnings(dda_usb_protocol AS_ERRORS CONVERSIONS)
    dda_enable_strict_warnings(dda_calibration_codec AS_ERRORS CONVERSIONS)
    return()
endif()

dda_enable_strict_warnings(dda_usb_protocol CONVERSIONS)
dda_enable_strict_warnings(dda_calibration_codec CONVERSIONS)

add_library(dda_platform_stm32 OBJECT
    Platform/Stm32/Analog/AdcSampler.cpp
    Platform/Stm32/Analog/InternalDacChannel.cpp
    Platform/Stm32/Gpio/GpioPin.cpp
    Platform/Stm32/I2c/I2cDevice.cpp
    Platform/Stm32/System/InterruptEvents.cpp
    Platform/Stm32/System/Timestamp.cpp
    Platform/Stm32/Usb/UsbCdcBridge.cpp
    Platform/Stm32/Usb/UsbTransport.cpp
)

add_library(dda_application OBJECT
    App/BoardApplication.cpp
    Drivers/Dac088s085.cpp
    Drivers/Drv8874.cpp
    Drivers/Ina226.cpp
    Service/Calibration/CalibrationStore.cpp
    Service/Launch/LaunchData.cpp
    Service/Launch/LaunchManager.cpp
    Service/Power/CoilController.cpp
    Service/Safety/SafetyManager.cpp
    Service/Sensor/SensorController.cpp
    Drivers/Sensors/VelocitySensor.cpp
    Service/UI/Display.cpp
    Service/UI/StatusLed.cpp
    Service/UI/UserInput.cpp
    Service/RequestManager/RequestManager.cpp
    Service/Usb/USBcontroller.cpp
)

function(dda_configure_embedded_application_target target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # HAL, CMSIS, middleware, and CubeMX headers are external to the
    # user-owned warning policy.
    target_include_directories(${target} SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/USB_DEVICE/App
        ${CMAKE_SOURCE_DIR}/USB_DEVICE/Target
        ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
        ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    )

    target_link_libraries(${target} PRIVATE stm32cubemx)
    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Release>:DDA_RELEASE_BUILD=1>
    )
    dda_enable_strict_warnings(${target} CONVERSIONS)
endfunction()

dda_configure_embedded_application_target(dda_platform_stm32)
dda_configure_embedded_application_target(dda_application)

target_link_libraries(dda_platform_stm32 PRIVATE
    dda_usb_protocol
)

target_link_libraries(dda_application PRIVATE
    dda_calibration_codec
    dda_platform_stm32
    dda_usb_protocol
)
