/*
 * Cobbled-together routing table.
 * This is neither efficient nor elegant, but until RIOT gets their own native
 * RT, this will have to do.
 */

#include <string.h>

#include "routing.h" 
#include "include/aodvv2.h"
#include "common/netaddr.h"

static struct aodvv2_routing_entry_t routing_table[AODVV2_MAX_ROUTING_ENTRIES];

void init_routingtable(void)
{   
    for (uint8_t i = 0; i < AODVV2_MAX_ROUTING_ENTRIES; i++) {
        memset(&routing_table[i], 0, sizeof(routing_table[i]));
    }
}

/* returns NULL if addr is not in routing table */
struct netaddr* get_next_hop(struct netaddr* addr)
{
    struct aodvv2_routing_entry_t* entry = get_routing_entry(addr);
    if (!entry)
        return NULL;
    return(&entry->nextHopAddress);
}


void add_routing_entry(struct aodvv2_routing_entry_t* entry)
{
    /* only update if we don't already know the address
     * TODO: does this always make sense?
     */
    if (!(get_routing_entry(&(entry->address)))){ // na ob das so stimmt...
        /*find free spot in RT and place rt_entry there */
        for (uint8_t i = 0; i< AODVV2_MAX_ROUTING_ENTRIES; i++){
            if (routing_table[i].address._type == AF_UNSPEC) {
                /* TODO: sanity check? */
                routing_table[i] = *entry; 
                return;
            }
        }
    }
}

/*
 * retrieve pointer to a routing table entry. To edit, simply follow the pointer.
 */
struct aodvv2_routing_entry_t* get_routing_entry(struct netaddr* addr)
{   
    for (uint8_t i = 0; i < AODVV2_MAX_ROUTING_ENTRIES; i++) {
        if (ipv6_addr_is_equal(&routing_table[i].address, addr)) {
            return &routing_table[i];
        }
    }
    return NULL;
}

void delete_routing_entry(struct netaddr* addr)
{
    for (uint8_t i = 0; i < AODVV2_MAX_ROUTING_ENTRIES; i++) {
        if (ipv6_addr_is_equal(&routing_table[i].address, addr)) {
            memset(&routing_table[i], 0, sizeof(routing_table[i]));
            return;
        }
    }
}

void print_rt_entry(struct aodvv2_routing_entry_t* rt_entry, int index)
{   
    struct netaddr_str nbuf;

    printf("routing table entry at %i:\n", index );
    printf("\t address: %s\n", netaddr_to_string(&nbuf, &(rt_entry->address))); 
    printf("\t prefixLength: %i\n", rt_entry->prefixLength);
    printf("\t seqNum: %i\n", rt_entry->seqNum);
    printf("\t nextHopAddress: %s\n", netaddr_to_string(&nbuf, &(rt_entry->nextHopAddress)));
    printf("\t lastUsed: %i\n", rt_entry->lastUsed);
    printf("\t expirationTime: %i\n", rt_entry->expirationTime);
    printf("\t broken: %d\n", rt_entry->broken);
    printf("\t metricType: %i\n", rt_entry->metricType);
    printf("\t metric: %i\n", rt_entry->metric);
    printf("\t state: \n", rt_entry->state);
}

void print_rt(void)
{   
    struct aodvv2_routing_entry_t curr_entry;

    printf("===== BEGIN ROUTING TABLE ===================\n");
    for(int i = 0; i < AODVV2_MAX_ROUTING_ENTRIES; i++) {
        if (routing_table[i].seqNum) {
            print_rt_entry(&routing_table[i], i); // fuck it, I'm tired.
        }
    }
    printf("===== END ROUTING TABLE =====================\n");
}