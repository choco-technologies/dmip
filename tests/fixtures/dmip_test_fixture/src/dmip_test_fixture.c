/**
 * @file dmip_test_fixture.c
 * @brief Implementation of dmip_test_fixture.h - see its doc comment for
 *        why this fixture is a real Library-type module rather than
 *        living directly in dmip_test.c
 */
#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmip_test_fixture.h"
#include <string.h>

static uint16_t g_claimed_protocols[DMIP_MAX_PROTOCOL_NUMBERS];
static size_t   g_claimed_count = 0;

static bool             g_called = false;
static dmip_family_t    g_last_family;
static dmnetif_iface_t  g_last_iface;
static uint8_t          g_last_packet[DMIP_TEST_FIXTURE_MAX_PACKET_LEN];
static size_t           g_last_packet_len = 0;

/**
 * @brief Implementation of dmip_test_fixture_claim() - see its own doc
 *        comment in dmip_test_fixture.h
 */
dmod_dmip_test_fixture_api_declaration(1.0, void, _claim, ( const uint16_t* protocols, size_t count ))
{
    if (count > DMIP_MAX_PROTOCOL_NUMBERS)
        count = DMIP_MAX_PROTOCOL_NUMBERS;
    if (count > 0 && protocols != NULL)
        memcpy(g_claimed_protocols, protocols, count * sizeof(uint16_t));
    g_claimed_count = count;
}

/**
 * @brief Implementation of dmip_test_fixture_claim_one()
 */
dmod_dmip_test_fixture_api_declaration(1.0, void, _claim_one, ( uint16_t protocol ))
{
    dmip_test_fixture_claim(&protocol, 1);
}

/**
 * @brief Implementation of dmip_test_fixture_claim_nothing()
 */
dmod_dmip_test_fixture_api_declaration(1.0, void, _claim_nothing, ( void ))
{
    dmip_test_fixture_claim(NULL, 0);
}

/**
 * @brief Implementation of dmip_test_fixture_reset_call_record()
 */
dmod_dmip_test_fixture_api_declaration(1.0, void, _reset_call_record, ( void ))
{
    g_called = false;
    g_last_packet_len = 0;
}

/**
 * @brief Implementation of dmip_test_fixture_get_last_call()
 */
dmod_dmip_test_fixture_api_declaration(1.0, bool, _get_last_call, ( dmip_family_t* out_family, dmnetif_iface_t* out_iface, uint8_t* out_packet, size_t* out_packet_len ))
{
    if (!g_called)
        return false;

    if (out_family != NULL)
        *out_family = g_last_family;
    if (out_iface != NULL)
        *out_iface = g_last_iface;
    if (out_packet != NULL && out_packet_len != NULL)
    {
        memcpy(out_packet, g_last_packet, g_last_packet_len);
        *out_packet_len = g_last_packet_len;
    }
    return true;
}

/**
 * @brief Implementation of dmip's dmip_protocol_receive DIF (see dmip.h) -
 *        records the call for dmip_test_fixture_get_last_call() to report
 */
dmod_dmip_dif_api_declaration(1.0, dmip_test_fixture, void, _protocol_receive, ( dmip_family_t family, dmnetif_iface_t iface, const uint8_t* packet, size_t packet_len ))
{
    g_called = true;
    g_last_family = family;
    g_last_iface = iface;
    g_last_packet_len = (packet_len < DMIP_TEST_FIXTURE_MAX_PACKET_LEN) ? packet_len : DMIP_TEST_FIXTURE_MAX_PACKET_LEN;
    memcpy(g_last_packet, packet, g_last_packet_len);
}

/**
 * @brief Implementation of dmip's dmip_protocol_numbers DIF (see dmip.h) -
 *        reports whatever dmip_test_fixture_claim()/_one()/_nothing()
 *        last set
 */
dmod_dmip_dif_api_declaration(1.0, dmip_test_fixture, size_t, _protocol_numbers, ( uint16_t* out_protocols, size_t max_protocols ))
{
    size_t count = (g_claimed_count < max_protocols) ? g_claimed_count : max_protocols;
    memcpy(out_protocols, g_claimed_protocols, count * sizeof(uint16_t));
    return count;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    g_claimed_count = 0;
    g_called = false;
    g_last_packet_len = 0;
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
