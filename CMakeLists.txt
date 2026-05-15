cmake_minimum_required(VERSION 3.20)

project(MY-RP6502-PROJECT C ASM)

add_subdirectory(tools)

add_executable(paintHD)
rp6502_asset(paintHD 0x10000 assets/paintHDwallpaper.bin)
rp6502_asset(paintHD 0x1EC00 assets/fonticons.bin)
rp6502_asset(paintHD paintHDhelp.bmp assets/paintHDhelp.bmp)
rp6502_executable(paintHD DATA 0x200 RESET 0x200)
target_sources(paintHD PRIVATE
    src/paintHD.c
    src/bmp.c
)

add_executable(paintHDloader)
rp6502_asset(paintHDloader 0x10000 assets/paintHDtools.bin)
rp6502_asset(paintHDloader 0x1EC00 assets/fonticons.bin)
rp6502_executable(paintHDloader DATA 0x200 RESET 0x200)
target_sources(paintHDloader PRIVATE
    src/paintHDloader.c
    src/bmp.c
)
