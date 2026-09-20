# dmip_test_fixture: a real Library-type module implementing dmip's
# protocol handler DIF for the "Protocol dispatch" steps in dmip_test.c to
# drive - test_dmip itself (an Application-type module, below) cannot
# serve that role, since Dmod_GetNextDifModule() only ever returns
# *enabled* modules and an Application module never reaches that state
# (see fixtures/dmip_test_fixture/include/dmip_test_fixture.h's own doc
# comment). Declared before test_dmip so its target exists for
# target_link_libraries() below.
add_subdirectory(fixtures/dmip_test_fixture)

set(DMOD_MODULE_NAME        test_dmip)
set(DMOD_MODULE_VERSION     ${PROJECT_VERSION})
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         1024)
set(DMOD_PRIORITY           0)

dmod_add_test(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    dmip_test.c
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# dmarp: the send/receive test steps seed dmarp cache entries by hand (see
#        dmip_test.c's file comment).
# dmnetbridge: feed_packet() drives dmip's packet_received DIF
#              implementation directly via Dmod_GetNextDifModule()/
#              Dmod_GetDifFunction(), the same way
#              dmnetbridge_handle_netif_rx() itself does.
# dmip_if (headers only - dmip itself is a sibling build product, found by
#          dmod_loader in the same DMOD_DMF_DIR, not fetched here): needed
#          for the functions under test.
# dmip_test_fixture_if (headers only - also a sibling build product): the
#          "Protocol dispatch" steps drive it through its Built-in API
#          (dmip_test_fixture_claim()/_get_last_call()/...) rather than
#          implementing the DIF in this module itself - see above.
target_link_libraries(${DMOD_MODULE_NAME} dmip_if dmip_test_fixture_if )

# dmnetif: the send/receive test steps register dmnetif fixture interfaces
#          by hand (see dmip_test.c's file comment).
# dmroute: routes are seeded by hand - see dmip_test.c's file comment.
dmod_link_modules(${DMOD_MODULE_NAME}
    dmnetbridge
    dmarp
    dmnetif
    dmroute
)
