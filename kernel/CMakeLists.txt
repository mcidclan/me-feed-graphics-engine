cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "$ENV{PSPDEV}/psp/share/pspdev.cmake" CACHE PATH "toolchain file")

project(kcall)

set(SRC_FILES)


# This is simplified copy of AddPrxModule.cmake from pspsdk, without psp-fixup-imports, as there's none
set(GENERATED_C_FILE ${CMAKE_BINARY_DIR}/exports.c)

add_custom_command(
    OUTPUT ${GENERATED_C_FILE}
    COMMAND psp-build-exports -b ${CMAKE_CURRENT_SOURCE_DIR}/src/exports.exp > ${GENERATED_C_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/exports.exp
    COMMENT "Generating export.c from exports.exp"
)

list(APPEND SRC_FILES ${GENERATED_C_FILE})
list(APPEND SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)

add_executable(kcall ${SRC_FILES})
target_link_options(kcall PRIVATE -Wl,-q,-T${PSPDEV}/psp/sdk/lib/linkfile.prx -nostartfiles -Wl,-zmax-page-size=128)

# Generate the .prx module from executable target
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/kcall.prx
  POST_BUILD
  COMMAND "${PSPDEV}/bin/psp-prxgen" "$<TARGET_FILE:kcall>" "${CMAKE_BINARY_DIR}/kcall.prx"
  COMMENT "Calling prxgen for target kcall"
)

# This is required to add ependency to main app
add_custom_target(kcall_all
  ALL
  DEPENDS ${CMAKE_BINARY_DIR}/kcall.prx
)
