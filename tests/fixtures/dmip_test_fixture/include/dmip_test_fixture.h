#ifndef DMIP_TEST_FIXTURE_H
#define DMIP_TEST_FIXTURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmip.h"
#include "dmip_test_fixture_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file dmip_test_fixture.h
 * @brief Test-only fixture module implementing dmip's protocol handler DIF
 *
 * dmip_test.c's "Protocol dispatch" test steps need a *loaded, enabled*
 * module implementing dmip_protocol_receive()/_protocol_numbers() to
 * drive dmip's DIF-based dispatch end-to-end - but dmip_test.c itself is
 * built as an Application-type module (dmod_add_test()), and
 * Dmod_GetNextDifModule() only ever returns modules that are *enabled*
 * (Dmod_IsEnabled()), a state Application modules never reach: they run
 * via Dmod_Run(), which only ever sets Running, and Dmod_Enable() itself
 * outright rejects a non-Library module type (see dmod's own
 * src/system/dmod_system.c). This fixture is a genuine Library-type
 * module instead, so it becomes Enabled - auto-loaded and enabled as one
 * of test_dmip's own required modules, the same way dmip itself already
 * is (see Dmod_Run()'s Dmod_RMod_LoadRequiredModules()/
 * _EnableRequiredModules() calls) - and is therefore actually
 * discoverable.
 *
 * dmip_test.c drives it through this Built-in API rather than reaching
 * into its state directly: claim()/_one()/_nothing() control what its
 * dmip_protocol_numbers() implementation reports, and get_last_call()
 * reports what its dmip_protocol_receive() implementation was last called
 * with.
 */

/**
 * @brief Capacity of the `out_packet` buffer dmip_test_fixture_get_last_call()
 *        expects
 */
#define DMIP_TEST_FIXTURE_MAX_PACKET_LEN 256u

/**
 * @brief Set the list of protocol numbers dmip_protocol_numbers() reports
 *
 * @param protocols DMIP_PROTO_* or DMIP_PROTO_DEFAULT values to claim
 * @param count     Number of entries in `protocols` (0 clears the claim,
 *                   in which case `protocols` may be NULL)
 */
dmod_dmip_test_fixture_api(1.0, void, _claim, ( const uint16_t* protocols, size_t count ));

/**
 * @brief Shorthand for dmip_test_fixture_claim() with a single entry
 */
dmod_dmip_test_fixture_api(1.0, void, _claim_one, ( uint16_t protocol ));

/**
 * @brief Shorthand for dmip_test_fixture_claim(NULL, 0)
 */
dmod_dmip_test_fixture_api(1.0, void, _claim_nothing, ( void ));

/**
 * @brief Clear the record of the last dmip_protocol_receive() call, so
 *        dmip_test_fixture_get_last_call() only reports a call made after
 *        this point
 */
dmod_dmip_test_fixture_api(1.0, void, _reset_call_record, ( void ));

/**
 * @brief Report the most recent dmip_protocol_receive() call since the
 *        last dmip_test_fixture_reset_call_record()
 *
 * @param out_family     Output: family the call was made with
 * @param out_iface      Output: iface the call was made with
 * @param out_packet     Output buffer, at least DMIP_TEST_FIXTURE_MAX_PACKET_LEN
 *                        bytes - the packet is truncated to that length if
 *                        longer
 * @param out_packet_len Output: length actually copied into `out_packet`
 *
 * @return true if dmip_protocol_receive() was called since the last reset
 *         (every output param is then set) - false otherwise (none are
 *         touched)
 */
dmod_dmip_test_fixture_api(1.0, bool, _get_last_call, ( dmip_family_t* out_family, dmnetif_iface_t* out_iface, uint8_t* out_packet, size_t* out_packet_len ));

#ifdef __cplusplus
}
#endif

#endif // DMIP_TEST_FIXTURE_H
