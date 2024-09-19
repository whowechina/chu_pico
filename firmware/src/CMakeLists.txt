pico_sdk_init()

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Werror -Wfatal-errors -O3)
link_libraries(pico_multicore pico_stdlib hardware_i2c hardware_spi
               hardware_pio hardware_adc hardware_flash hardware_watchdog
               tinyusb_device tinyusb_board)

function(make_firmware board board_def)
    add_executable(${board}
        main.c slider.c air.c rgb.c save.c config.c commands.c
        cli.c lzfx.c vl53l0x.c mpr121.c usb_descriptors.c)
    target_compile_definitions(${board} PUBLIC ${board_def})
    pico_enable_stdio_usb(${board} 1)

    pico_generate_pio_header(${board} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
    
    target_link_libraries(${board} PRIVATE aic)

    pico_add_extra_outputs(${board})

    add_custom_command(TARGET ${board} PRE_BUILD
    COMMAND touch ${CMAKE_CURRENT_SOURCE_DIR}/cli.c)

    add_custom_command(TARGET ${board} POST_BUILD
                       COMMAND cp ${board}.uf2 ${CMAKE_CURRENT_LIST_DIR}/..)
endfunction()

make_firmware(chu_pico BOARD_CHU_PICO)
