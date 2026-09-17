/**
 * @file lsproto.c
 * @brief lsproto - lists every IP protocol number currently registered
 *        with dmip via dmip_register_protocol()
 */
#include "dmod.h"
#include "dmip.h"

/**
 * @brief Best-effort human-readable name for a DMIP_PROTO_* value
 *
 * Only the well-known numbers dmip.h itself defines - anything else prints
 * as a bare number, same as a real system's /etc/protocols lookup falling
 * back to the raw number when it has no entry for it.
 */
static const char* protocol_name(uint8_t protocol)
{
    switch (protocol)
    {
        case DMIP_PROTO_ICMP:          return "ICMP";
        case DMIP_PROTO_TCP:           return "TCP";
        case DMIP_PROTO_UDP:           return "UDP";
        case DMIP_PROTO_IPV6_FRAGMENT: return "IPv6-Fragment";
        case DMIP_PROTO_ICMPV6:        return "ICMPv6";
        default:                       return "unknown";
    }
}

static void print_protocol(uint8_t protocol, void* user_data)
{
    size_t* count = (size_t*)user_data;
    Dmod_Printf("  %3u  %s\n", (unsigned)protocol, protocol_name(protocol));
    (*count)++;
}

/**
 * @brief Main function of the application
 *
 * @param argc Number of arguments
 * @param argv Array of arguments
 *
 * @return Always 0 - there is nothing here that can fail; an empty
 *         registration table is reported, not an error
 */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    Dmod_Printf("Registered IP protocols:\n");

    size_t count = 0;
    dmip_for_each_protocol(print_protocol, &count);

    if (count == 0)
        Dmod_Printf("  (none)\n");

    return 0;
}
