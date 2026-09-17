# ==============================================================================
#               lsproto Tool Module Configuration
#
# A small companion application module for dmip: lists every IP protocol
# number currently registered via dmip_register_protocol() (using
# dmip_for_each_protocol(), see include/dmip.h in the repo root).
# Declared here as a second module in this same CMake project - same
# "nested module, no second project()/FetchContent" pattern dmuart/dmfmc
# use for their own <module>_port via add_subdirectory(src/port), just an
# application module instead of a library one. Named without the "dm"
# prefix - reserved for modules, not dmell-run tools (same convention
# tools/ip uses in the parent dmnet repo).
# ==============================================================================

# Name of the module
set(DMOD_MODULE_NAME        lsproto)

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Priority for the thread of the module (should be integer)
set(DMOD_PRIORITY           0)

# Path to the .dmr file - enables release package creation in build/packages/
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/lsproto.dmr)

#
#   dmod_add_executable - create an executable module
#   it has the same signature as add_executable
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    src/lsproto.c
)

# dmip_if: the Built-in API this tool calls (dmip_for_each_protocol()) - a
#          local target, not fetched via dmod_link_modules(), since dmip is
#          built in this same repo/CMake project (see the top-level
#          CMakeLists.txt's own target_include_directories(dmip_if ...)).
target_link_libraries(${DMOD_MODULE_NAME} dmip_if)

# dmnetif: dmip.h needs dmnetif_iface_t (an optional parameter type on the
#          protocol handler callback type it declares), even though this
#          tool itself never calls a dmnetif_* function.
# dmroute: dmip.h needs dmroute_addr_t (re-exported as dmip_addr_t), even
#          though this tool itself never calls a dmroute_* function.
# Same two dependencies dmip's own tests/CMakeLists.txt fetches for the
# same reason - only what dmip.h itself pulls in, everything dmip.c needs
# beyond that stays private to dmip.
dmod_link_modules(${DMOD_MODULE_NAME}
    dmnetif
    dmroute
)
