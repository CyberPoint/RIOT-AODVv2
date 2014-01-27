#include <stdio.h>

#include "ipv6.h"

#include "common/netaddr.h"

#include "include/aodvv2.h"
#include "seqnum.h"

#ifndef UTILS_H_
#define UTILS_H_

#define AODVV2_MAX_CLIENTS 16 // shabby, I know
#define AODVV2_RREQ_BUF 3   // should be enough for now...

struct aodvv2_rreq_entry {
    struct netaddr origNode;
    struct netaddr targNode;
    uint8_t metricType;
    uint8_t metric;
    uint16_t seqNum;
    timex_t timestamp;
};

/* Section 5.3.: Client Table functionality */
// TODO: make naming more clear? (-> reference to client table)
void init_clienttable(void);
void add_client(struct netaddr* addr);
bool is_client(struct netaddr* addr);
void delete_client(struct netaddr* addr);

/* Sections 5.7. and 7.6.: RREQ table functionality */
void init_rreqtable(void);
bool rreq_is_redundant(struct aodvv2_packet_data* packet_data);
void rreqtable_add(struct aodvv2_packet_data* packet_data);

#endif /* UTILS_H_ */
