# ==============================================================================
#     dmip_test_fixture - test-only Library module implementing dmip's
#     protocol handler DIF, so dmip_test.c's "Protocol dispatch" steps have
#     a genuinely loaded+enabled DIF implementor to exercise (see
#     include/dmip_test_fixture.h's own doc comment for why test_dmip
#     itself - an Application-type module - can't serve this role: only a
#     Library-type module can ever be Dmod_Enable()'d, the state
#     Dmod_GetNextDifModule() requires).
#     Same "nested module, no second project()/FetchContent" pattern
#     tools/lsproto uses.
# ==============================================================================

set(DMOD_MODULE_NAME        dmip_test_fixture)

set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         1024)

# List of MAL interfaces implemented by this module (leave empty if none)
set(DMOD_MAL_IMPLS          )

# List of DIF interfaces implemented by this module - implements dmip's
# protocol handler DIF (see src/dmip_test_fixture.c).
set(DMOD_DIF_IMPLS          dmip)

dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    src/dmip_test_fixture.c
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# dmip_if: dmip_protocol_receive()/_protocol_numbers() DIF declarations,
#          and the dmip_family_t/DMIP_MAX_PROTOCOL_NUMBERS types this
#          fixture uses - a local target, not fetched via
#          dmod_link_modules(), since dmip is built in this same
#          repo/CMake project (same reasoning tools/lsproto's own
#          CMakeLists.txt gives for the same target).
target_link_libraries(${DMOD_MODULE_NAME}_if INTERFACE dmip_if)

# dmnetif: dmip.h needs dmnetif_iface_t (the iface parameter
#          dmip_protocol_receive() passes through).
# dmroute: dmip.h needs dmroute_addr_t (re-exported as dmip_addr_t), even
#          though this fixture itself never calls a dmroute_* function.
# Same two dependencies dmip's own tests/CMakeLists.txt and
# tools/lsproto/CMakeLists.txt fetch for the same reason.
dmod_link_modules(${DMOD_MODULE_NAME}
    dmnetif
    dmroute
)
