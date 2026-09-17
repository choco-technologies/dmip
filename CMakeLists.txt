cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED DMOD_MODULE_VERSION)
    set(DMOD_MODULE_VERSION "0.1" CACHE STRING "DMOD module version")
endif()

# If a local dmod checkout is provided, use it instead of fetching from git
if(DEFINED DMOD_DIR AND NOT "${DMOD_DIR}" STREQUAL "")
    set(FETCHCONTENT_SOURCE_DIR_DMOD "${DMOD_DIR}" CACHE PATH "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    dmod
    GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
    GIT_TAG        develop
)

set(DMOD_MODE "DMOD_MODULE" CACHE STRING "DMOD build mode")
set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")

FetchContent_MakeAvailable(dmod)

project(dmip VERSION ${DMOD_MODULE_VERSION}
    DESCRIPTION "dmip DMOD library module"
    LANGUAGES C CXX)

set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")
set(DMOD_SCRIPTS_DIR ${DMOD_DIR}/scripts CACHE PATH "DMOD scripts directory")
include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# Name of the module
set(DMOD_MODULE_NAME        dmip)

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Path to the .dmr file - enables release package creation in build/packages/
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/dmip.dmr)

# List of MAL interfaces implemented by this module (leave empty if none)
set(DMOD_MAL_IMPLS          )

# List of DIF interfaces implemented by this module (leave empty if none) -
# dmip implements dmnetbridge's packet_received DIF (see src/dmip.c).
set(DMOD_DIF_IMPLS          dmnetbridge)

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    src/dmip.c
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# dmnetif: header-only - dmip.h needs dmnetif_iface_t for the optional
#          out_iface parameter on dmip_v4_receive()/_v6_receive()/
#          _receive(), even though dmip.c itself never calls a dmnetif_*
#          function directly anymore. A separately published module,
#          fetched via dmod_link_modules().
# dmroute: header-only - dmip.h needs dmroute_addr_t (re-exported as
#          dmip_addr_t) directly, even though dmip.c itself no longer
#          calls any dmroute_* function (that moved to dmnetbridge - see
#          src/dmip.c's top comment)
# dmlist:  backs the fragment reassembly table
# dmosi:   mutexes guarding reassembly/identification state, plus
#          dmosi_get_tick_count() for reassembly timeouts - see
#          src/dmip.c's "Reassembly" section
dmod_link_modules(${DMOD_MODULE_NAME}
    dmnetbridge
    dmnetif
    dmroute
    dmlist
    dmosi
)

add_subdirectory(tests)
add_subdirectory(tools/lsproto)
