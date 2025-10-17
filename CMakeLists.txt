cmake_minimum_required(VERSION 3.10)

project(rosettax87)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rosettax87 loader/main.cpp loader/macho_loader.cpp loader/offset_finder.cpp)

# We need to sign the binary with those entitlements to allow debugging without root
set(ENTITLEMENTS_FILE "${CMAKE_SOURCE_DIR}/entitlements.plist")
add_custom_command(TARGET rosettax87 POST_BUILD
    COMMAND codesign -s - --entitlements "${ENTITLEMENTS_FILE}" --force "$<TARGET_FILE:rosettax87>"
    COMMENT "Signing rosettax87 with entitlements"
)

add_executable(libRuntimeRosettax87
    rosettaRuntime/main.cpp
    rosettaRuntime/X87Float80.cpp
    rosettaRuntime/X87StackRegister.cpp
    rosettaRuntime/X87State.cpp
    rosettaRuntime/X87.cpp
    rosettaRuntime/Export.cpp
    rosettaRuntime/Log.cpp
    rosettaRuntime/SIMDGuard.cpp
)

target_include_directories(libRuntimeRosettax87 PRIVATE
    rosettaRuntime
)

# Add ExternalProject module
include(ExternalProject)

# Create directories before CMake references them
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/deps/lib)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/deps/include)

set(OPTIMIZATION_FLAGS
    "-fvisibility=hidden"
    "-O3"
    "-funroll-loops"
    "-march=native"
    "-mtune=native"
    "-fomit-frame-pointer"
    "-mcpu=apple-m1"
    "-fno-builtin-sin"
    "-ftree-vectorize"
    "-fvectorize"
    "-finline-functions"
    "-fno-stack-protector"
    "-fno-exceptions"
    "-fno-unwind-tables"
    "-fno-asynchronous-unwind-tables"
)

string(REPLACE ";" " " OPTIMIZATION_FLAGS_STRING "${OPTIMIZATION_FLAGS}")

# Build OpenLibm as a separate project with static library
ExternalProject_Add(
    openlibm_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/openlibm
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/deps
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_C_FLAGS=${OPTIMIZATION_FLAGS_STRING}
    # Skip running the default install step that has errors
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/openlibm_build-prefix/src/openlibm_build-build/libopenlibm.a
)

# Add a custom step to copy the headers and static library manually
ExternalProject_Add_Step(openlibm_build custom_install
    COMMENT "Installing OpenLibm files"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/openlibm/include
        ${CMAKE_BINARY_DIR}/deps/include
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/openlibm_build-prefix/src/openlibm_build-build/libopenlibm.a
        ${CMAKE_BINARY_DIR}/deps/lib/
    DEPENDEES build
)

# Create an imported target for the static library
add_library(openlibm STATIC IMPORTED)
add_dependencies(openlibm openlibm_build)
set_target_properties(openlibm PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/deps/lib/libopenlibm.a
)
target_include_directories(openlibm INTERFACE ${CMAKE_BINARY_DIR}/deps/include)

target_link_options(libRuntimeRosettax87 PRIVATE
    "-Wl,-no_compact_unwind"
    "-Wl,-no_pie"
    "-static"
    "-nostdlib"
    "-Wl,-segaddr,__TEXT,0x0"
    "-Wl,-segaddr,__DATA,0x10000"
    "-Wl,-pagezero_size,0x0"
    "-Wl,-headerpad,0"
    "-Wl,-e,_start"
    "-Wl,-segalign,10"
)

target_compile_options(libRuntimeRosettax87 PRIVATE
    ${OPTIMIZATION_FLAGS}
    "-flto"
)

target_link_libraries(libRuntimeRosettax87 PRIVATE
    openlibm
)
