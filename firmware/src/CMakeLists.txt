function(make_firmware board board_def)
    pico_sdk_init()
    add_executable(${board}
        main.c slider.c air.c rgb.c save.c config.c commands.c
        cli.c lzfx.c vl53l0x.c mpr121.c usb_descriptors.c)
    target_compile_definitions(${board} PUBLIC ${board_def})
    pico_enable_stdio_usb(${board} 1)
    pico_enable_stdio_uart(${board} 0)

    pico_generate_pio_header(${board} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
    
    target_compile_options(${board} PRIVATE -Wall -Werror -Wfatal-errors -O3)
    target_include_directories(${board} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    
    target_link_libraries(${board} PRIVATE
        aic
        pico_multicore pico_stdlib hardware_pio hardware_pwm hardware_flash
        hardware_adc hardware_i2c hardware_watchdog
        tinyusb_device tinyusb_board)

    pico_add_extra_outputs(${board})

    add_custom_command(TARGET ${board} POST_BUILD
                       COMMAND cp ${board}.uf2 ${CMAKE_CURRENT_LIST_DIR}/..)
endfunction()

make_firmware(chu_pico BOARD_CHU_PICO)
