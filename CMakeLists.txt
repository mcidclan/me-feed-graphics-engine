cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "$ENV{PSPDEV}/psp/share/pspdev.cmake" CACHE PATH "toolchain file")

project(me-feed-graphics-engine CXX ASM)

# Add kernel prx
add_subdirectory(kernel)

# Add custom command and dep to generate imports asm
set(GENERATED_ASM_FILE ${CMAKE_BINARY_DIR}/kcall.S)
add_custom_command(
    OUTPUT ${GENERATED_ASM_FILE}
    COMMAND psp-build-exports -s ${CMAKE_SOURCE_DIR}/kernel/src/exports.exp > ${GENERATED_ASM_FILE}
    DEPENDS kcall_all
    COMMENT "Generating kcall.S from exports.exp"
)

# Main executable
add_executable(megu
    ${CMAKE_BINARY_DIR}/kcall.S
    main.cpp
)

# Set flags for all build types
target_compile_options(megu PRIVATE -Ofast -G0  -Wall -fno-pic -std=c++11 -fno-rtti -fno-exceptions)

# Includes
target_include_directories(megu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/kernel/src")

# Libs
target_link_libraries(megu psppower pspgu pspge pspdisplay pspctrl pspdebug)

create_pbp_file(
  TARGET megu
  ICON_PATH NULL
  BACKGROUND_PATH NULL
  PREVIEW_PATH NULL
  TITLE "Me Gu Stuff"
)

