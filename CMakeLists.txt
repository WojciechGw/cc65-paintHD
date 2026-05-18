cmake_minimum_required(VERSION 3.20)

project(MY-RP6502-PROJECT C ASM)

add_subdirectory(tools)

#add_executable(paintHD)
#rp6502_asset(paintHD 0x10000 assets/paintHDwallpaper.bin)
#rp6502_asset(paintHD 0x1EC00 assets/fonticons.bin)
#rp6502_asset(paintHD paintHDhelp.bmp assets/paintHDhelp.bmp)
#rp6502_executable(paintHD DATA 0x200 RESET 0x200)
#target_sources(paintHD PRIVATE
#    src/paintHD.c
#    src/bmp.c
#)
#target_link_options(paintHD PRIVATE "-Wl" "-D__STACKSIZE__=0x0200") # default 0x0800 wastes RAM; 0x0200 verified sufficient

add_executable(paintHD)
rp6502_asset(paintHD 0x10000 assets/paintHDwallpaper.bin)
rp6502_asset(paintHD 0x1EC00 assets/fonticons.bin)
rp6502_asset(paintHD paintHDsplash.bin assets/paintHDsplash.bin)
rp6502_executable(paintHD DATA 0x200 RESET 0x200)
target_sources(paintHD PRIVATE
    src/paintHDloader.c
    src/bmp.c
)

add_executable(paintHDeditor)
rp6502_asset(paintHDeditor 0x1EC00 assets/fonticons.bin)
rp6502_asset(paintHDeditor paintHDhelp.bmp assets/paintHDhelp.bmp)
rp6502_executable(paintHDeditor DATA 0x200 RESET 0x200)
target_sources(paintHDeditor PRIVATE
    src/paintHDeditor.c
    src/bmp.c
)
target_link_options(paintHDeditor PRIVATE "-Wl" "-D__STACKSIZE__=0x0200") # default 0x0800 wastes RAM; 0x0200 verified sufficient

add_executable(paintHDzoom)
rp6502_asset(paintHDzoom 0x1EC00 assets/fonticons.bin)
rp6502_executable(paintHDzoom DATA 0x200 RESET 0x200)
target_sources(paintHDzoom PRIVATE
    src/paintHDzoom.c
    src/bmp.c
)
target_link_options(paintHDzoom PRIVATE "-Wl" "-D__STACKSIZE__=0x0200")
