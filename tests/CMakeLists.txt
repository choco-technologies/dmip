set(DMOD_MODULE_NAME        test_dmip)
set(DMOD_MODULE_VERSION     ${PROJECT_VERSION})
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         1024)
set(DMOD_PRIORITY           0)

# List of DIF interfaces implemented by this module - test_dmip implements
# dmip's own protocol handler DIF as a fixture for the "Protocol dispatch"
# steps in dmip_test.c (see that file's own doc comment).
set(DMOD_DIF_IMPLS          dmip)

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
#          both for the functions under test and for the
#          dmod_dmip_dif_api_declaration()-based fixture implementing
#          dmip's protocol handler DIF (see dmip_test.c).
target_link_libraries(${DMOD_MODULE_NAME} dmip_if )

# dmnetif: the send/receive test steps register dmnetif fixture interfaces
#          by hand (see dmip_test.c's file comment).
# dmroute: routes are seeded by hand - see dmip_test.c's file comment.
dmod_link_modules(${DMOD_MODULE_NAME}
    dmnetbridge
    dmarp
    dmnetif
    dmroute
)
