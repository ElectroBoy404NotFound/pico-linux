add_library(st7735
	st7735.c
)

target_include_directories(st7735 PUBLIC
	.
)

target_link_libraries(st7735 pico_stdlib hardware_spi hardware_dma)