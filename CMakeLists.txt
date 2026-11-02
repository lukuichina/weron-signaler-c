project(weron-signaler-c C)
cmake_minimum_required(VERSION 3.7)

# Version
set(WERON_VERSION "1.0.0")

# Source files
set(WERON_SOURCES
    main.c
    relay.c
)

# Include directories
include_directories(${PROJECT_SOURCE_DIR})

# Add version definitions
add_definitions(-DWERON_VERSION=\"${WERON_VERSION}\")

# --- Platform-specific configuration ---
if(DEFINED WIN32)
    # Windows: use bcrypt for crypto, link system libs
    set(CMAKE_RC_FLAGS "/nologo /I \"${CMAKE_CURRENT_SOURCE_DIR}\"")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# --- Build information ---
if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT DEFINED CMAKE_BUILD_TYPE)

set(CMAKE_INSTALL_DO_STRIP ON)

# --- Build the executable ---
if(DEFINED WIN32)
    add_executable(weron-signaler-c.exe ${WERON_SOURCES})
    set_target_properties(weron-signaler-c.exe PROPERTIES
        OUTPUT_NAME weron-signaler-c.exe
    )
    set_property(TARGET weron-signaler-c.exe PROPERTY C_STANDARD 99)
    
    # Windows-specific: static libgcc, dynamic CRT
    set_target_properties(weron-signaler-c.exe PROPERTIES
        LINK_FLAGS "-static-libgcc"
    )
    
    # Link libraries for Windows
    target_link_libraries(weron-signaler-c.exe bcrypt ws2_32 iphlpapi ole32)
else()
    add_executable(weron-signaler-c ${WERON_SOURCES})
    set_property(TARGET weron-signaler-c PROPERTY C_STANDARD 99)
    
    # Static linking for Linux/Unix targets
    set_target_properties(weron-signaler-c PROPERTIES
        LINK_FLAGS "-static"
    )
    
    # Link libraries for Linux/Unix
    target_link_libraries(weron-signaler-c pthread dl)
endif()

# Dead code elimination
if(NOT DEFINED WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()

# Installation
install(TARGETS weron-signaler-c weron-signaler-c.exe
    RUNTIME DESTINATION sbin
    OPTIONAL
)