cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
# include(example_auto_set_url.cmake)
project(VGA_project C CXX ASM)
# Initialize the SDK
pico_sdk_init()

add_executable(pico-composite8)

pico_generate_pio_header(pico-composite8 ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio)

target_sources(pico-composite8 PRIVATE cvideo.c)

target_link_libraries(pico-composite8 
	PRIVATE 
	pico_stdlib 
	pico_multicore
	pico_mem_ops 
	hardware_pio 
	hardware_dma 
	hardware_irq
	)

# for printing:
#    # enable usb output, disable uart output
#    pico_enable_stdio_usb(pico-composite8 1)
#    pico_enable_stdio_uart(pico-composite8 0)

pico_add_extra_outputs(pico-composite8)

# add url via pico_set_program_url
# example_auto_set_url(pico-composite8)

# generate .hex file and .pio.h file for the RP2040 datasheet (to make sure
# the datasheet always shows the output of the latest pioasm version)
#add_custom_target(pico-composite8_datasheet DEPENDS
#		Pioasm
#		${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.hex
#		${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.pio.h
#		)
#add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.hex
#        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio
#        COMMAND Pioasm -o hex ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.hex
#        )
#add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.pio.h
#        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio
#        COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.pio.h
#        )
#
#add_dependencies(pico-composite8 pico-composite8_datasheet)
