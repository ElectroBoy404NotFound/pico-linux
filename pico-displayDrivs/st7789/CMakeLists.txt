add_library(st7789
	st7789.c
)

target_include_directories(st7789 PUBLIC
	.
)

target_link_libraries(st7789 pico_stdlib hardware_spi hardware_dma)