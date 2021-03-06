#include <stdio.h>

#include "constants.h"
#include "routing.h"
#include "reader.h"
#include "utils.h"
#include "cunit/cunit.h"

#include "common/netaddr.h"

struct netaddr_str nbuf;
char str[6000];


char* node_data_to_string(struct node_data* node_data);
char* packet_data_to_string(struct aodvv2_packet_data* packet_data);

void test_routingtable_get_entry(struct netaddr* addr, uint8_t metricType)
{
   struct aodvv2_routing_entry_t* entry_result = routingtable_get_entry(addr, metricType);
   CHECK_TRUE(entry_result != NULL, "there should be an entry for %s with metrictType %i\n", netaddr_to_string(&nbuf, addr), metricType);
}

void test_routingtable_get_entry_bullshitdata(struct netaddr* addr, uint8_t metricType)
{
    struct aodvv2_routing_entry_t* entry_result = routingtable_get_entry(addr, metricType);
    CHECK_TRUE(entry_result == NULL, "there should be no entry for %s with metrictType %i\n", netaddr_to_string(&nbuf, addr), metricType);
}

void test_routingtable_get_next_hop(struct netaddr* addr, uint8_t metricType, struct netaddr* next_hop_expected)
{
    int addr_equal = -1;
    struct netaddr* next_hop_result = routingtable_get_next_hop(addr, metricType);
    
    CHECK_TRUE(next_hop_result != NULL, "no next hop for %s with metrictType %i\n", netaddr_to_string(&nbuf, addr), metricType);

    if (next_hop_result)
        addr_equal = netaddr_cmp(next_hop_expected, next_hop_result);

    CHECK_TRUE( addr_equal == 0, "unexpected next hop for %s with metrictType %i: \n\t found: %s expected: %s\n", 
                netaddr_to_string(&nbuf, addr), metricType, netaddr_to_string(&nbuf, next_hop_result), 
                netaddr_to_string(&nbuf, next_hop_expected));
}

void test_routingtable_get_next_hop_bullshitdata(struct netaddr* addr, uint8_t metricType)
{
    struct netaddr* next_hop_result = routingtable_get_next_hop(addr, metricType);   
    CHECK_TRUE(next_hop_result == NULL, "there should be no next hop for %s with metricType %i\n", 
                netaddr_to_string(&nbuf, addr), metricType);
}

void test_rreqtable_rreq_not_redundant(struct aodvv2_packet_data* packet_data)
{
    bool data_is_redundant = rreqtable_is_redundant(packet_data);
    CHECK_TRUE(!data_is_redundant, "the following data shouldn't be redundant:\n%s", packet_data_to_string(packet_data));
}

void test_rreqtable_rreq_redundant(struct aodvv2_packet_data* packet_data)
{
    bool data_is_redundant = rreqtable_is_redundant(packet_data);
    CHECK_TRUE(data_is_redundant, "the following data should be redundant:\n%s", packet_data_to_string(packet_data));
}

void test_routingtable(void)
{
    timex_t now, validity_t;
    struct netaddr addr_1, addr_2;

    /* init data */
    netaddr_from_string(&addr_1, "::23");
    netaddr_from_string(&addr_2, "::42");

    vtimer_now(&now);
    validity_t = timex_set(AODVV2_ACTIVE_INTERVAL + AODVV2_MAX_IDLETIME, 0); 

    struct aodvv2_routing_entry_t entry_1 = {
        .addr = addr_1,
        .seqnum = 6,
        .nextHopAddr = addr_2,
        .lastUsed = now,
        .expirationTime = timex_add(now, validity_t),
        .metricType = AODVV2_DEFAULT_METRIC_TYPE,
        .metric = 12,
        .state = ROUTE_STATE_IDLE
    };

    vtimer_now(&now);

    struct aodvv2_routing_entry_t entry_2 = {
        .addr = addr_2,
        .seqnum = 0, // illegal, but what the hell. for testing purposes. ahum.
        .nextHopAddr = addr_2,
        .lastUsed = now,
        .expirationTime = timex_add(now, validity_t),
        .metricType = AODVV2_DEFAULT_METRIC_TYPE,
        .metric = 13,
        .state = ROUTE_STATE_ACTIVE
    };
    
    START_TEST();

    /* prepare routing table */
    routingtable_init();

    print_routingtable();
    printf("Adding first entry: %s ...\n", netaddr_to_string(&nbuf, &addr_2));
    routingtable_add_entry(&entry_1);
    print_routingtable();
    printf("Adding second entry: %s ...\n", netaddr_to_string(&nbuf, &addr_2));
    routingtable_add_entry(&entry_2);
    print_routingtable();
    printf("Deleting first entry: %s ...\n", netaddr_to_string(&nbuf, & addr_2));
    routingtable_delete_entry(&addr_2, entry_1.metricType);
    print_routingtable();   
    
    /* start testing */
    test_routingtable_get_entry(&addr_1, entry_1.metricType);
    test_routingtable_get_entry_bullshitdata(&addr_2, entry_2.metricType); // ask for nonexisting address
    test_routingtable_get_entry_bullshitdata(&now, entry_2.metricType);    // wrong data type for address
    test_routingtable_get_entry_bullshitdata(&addr_1, 2);                  // right address, wrong metricType 

    test_routingtable_get_next_hop(&addr_1, entry_1.metricType, &addr_2);  
    test_routingtable_get_next_hop_bullshitdata(&addr_2, entry_2.metricType); // use nonexisting address
    test_routingtable_get_next_hop_bullshitdata(&now, entry_2.metricType);    // wrong data type for address
    test_routingtable_get_next_hop_bullshitdata(&addr_1, 2);                  // right address, wrong metricType 

    vtimer_usleep((AODVV2_MAX_SEQNUM_LIFETIME) * 1000000); // usleeps needs milliseconds, so there
    test_routingtable_get_entry_bullshitdata(&addr_1, entry_1.metricType);    // entry_1 should be stale & removed by now
    test_routingtable_get_next_hop_bullshitdata(&addr_1, entry_1.metricType); // entry_1 should be stale & removed by now
    routingtable_delete_entry(&addr_1, entry_1.metricType);                        // here's to hoping this blows up when something goes wrong

    END_TEST();
}

void test_rreq_table(void)
{

    timex_t now, validity_t;
    struct netaddr address, next_hop;

    /* init data */
    netaddr_from_string(&address, "::42");
    netaddr_from_string(&next_hop, "::23");

    vtimer_now(&now);
    validity_t = timex_set(AODVV2_ACTIVE_INTERVAL + AODVV2_MAX_IDLETIME, 0); 

    struct aodvv2_packet_data entry_1 = {
        .hoplimit = AODVV2_MAX_HOPCOUNT,
        .sender = address,
        .metricType = AODVV2_DEFAULT_METRIC_TYPE,
        .origNode = {
            .addr = address,
            .metric = 12,
            .seqnum = 13,
        },
        .targNode = {
            .addr = next_hop,
            .metric = 12,
            .seqnum = 0,
        },
        .timestamp = now,
    };

    vtimer_now(&now);

    START_TEST();
    rreqtable_init();

    /* start testing */
    printf(".\n");
    test_rreqtable_rreq_not_redundant(&entry_1);    
    printf(".\n");
    test_rreqtable_rreq_redundant(&entry_1);    // the RREQ Table should already know entry_1
    entry_1.origNode.metric = 1;    
    printf(".\n");
    test_rreqtable_rreq_not_redundant(&entry_1);
    entry_1.origNode.seqnum = 1;
    printf(".\n");
    test_rreqtable_rreq_redundant(&entry_1);
    entry_1.origNode.seqnum = 14;               // seqnum isn now bigger than the "newest" entry (i.e. original entry_1 with seqnum 13)
    printf(".\n");
    test_rreqtable_rreq_not_redundant(&entry_1);
    printf(".\n");
    test_rreqtable_rreq_redundant(&entry_1);

    vtimer_usleep((AODVV2_MAX_IDLETIME+1) * 1000000); // usleeps needs milliseconds, so there
    printf(".\n");
    test_rreqtable_rreq_not_redundant(&entry_1);      // entry_1 should be stale & removed by now

    // TODO: wie überprüfe ich ob das bogus data ist? geht nicht, oder?
    //test_rreqtable_rreq_redundant(&next_hop);   // feed the rreqtable bogus data

    END_TEST();
}

void test_tables_main(void)
{
    BEGIN_TESTING(NULL);

    test_routingtable();
    test_rreq_table();

    FINISH_TESTING();
}

char* node_data_to_string(struct node_data* node_data)
{
    sprintf(str, "\t\taddr: %s\n\
\t\tmetric: %d\n\
\t\tseqnum: %d\n", netaddr_to_string(&nbuf, &node_data->addr), node_data->metric,
                             node_data->seqnum);
    return str;
}

char* packet_data_to_string(struct aodvv2_packet_data* packet_data)
{
    /*
    sprintf(str,"packet data :\n \
\thoplimit: %i\n\
\tsender: %s\n\
\tmetricType: %i\n\
\torigNode: \n%s\n\
\ttargNode: \n%s\n\
\ttimestamp: %i.%i\n", packet_data->hoplimit, 
                                       netaddr_to_string(&nbuf, &packet_data->sender),
                                       packet_data->metricType, 
                                       node_data_to_string(&packet_data->origNode),
                                       "down for maintenance",
                                       packet_data->timestamp.seconds,
                                       packet_data->timestamp.microseconds);
    return str;
    */
    return  "";
}
