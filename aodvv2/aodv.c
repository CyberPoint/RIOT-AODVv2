#include "aodv.h"
#include "include/aodvv2.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

#define UDP_BUFFER_SIZE     (128) // TODO öhm.
#define RCV_MSG_Q_SIZE      (64)

static void _init_addresses(void);
static void _init_sock_snd(void);
static void _aodv_receiver_thread(void);
static void _write_packet(struct rfc5444_writer *wr __attribute__ ((unused)),
        struct rfc5444_writer_target *iface __attribute__((unused)),
        void *buffer, size_t length);

char addr_str[IPV6_MAX_ADDR_STR_LEN];
char aodv_rcv_stack_buf[KERNEL_CONF_STACKSIZE_MAIN];

static int _metric_type;
static int _sock_snd;
static struct autobuf _hexbuf;
static sockaddr6_t sa_wp;
static ipv6_addr_t _v6_addr_local, _v6_addr_mcast;
static struct netaddr na_local; // the same as _v6_addr_local, but to save us constant calls to ipv6_addr_t_to_netaddr()...
static struct writer_target* wt;
static struct netaddr_str nbuf;

void aodv_init(void)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    aodv_set_metric_type(AODVV2_DEFAULT_METRIC_TYPE);
    _init_addresses();
    _init_sock_snd();

    /* init ALL the things! \o, */
    seqNum_init();
    routingtable_init();
    clienttable_init();

    /* every node is its own client. */
    clienttable_add_client(&na_local);
    rreqtable_init(); 

    /* init reader and writer */
    reader_init();
    writer_init(_write_packet);

    /* start listening */
    int aodv_receiver_thread_pid = thread_create(aodv_rcv_stack_buf, KERNEL_CONF_STACKSIZE_MAIN, PRIORITY_MAIN, CREATE_STACKTEST, _aodv_receiver_thread, "_aodv_receiver_thread");
    printf("[aodvv2] listening on port %d (thread pid: %d)\n", HTONS(MANET_PORT), aodv_receiver_thread_pid);

    /* register aodv for routing */
    ipv6_iface_set_routing_provider(aodv_get_next_hop);

    /*testtest*/
    writer_send_rreq(&na_local, &na_mcast, &na_mcast);

    struct unreachable_node unreachable_nodes[2];
    unreachable_nodes[0].addr = na_local;
    unreachable_nodes[0].seqnum = 13;
    unreachable_nodes[1].addr = na_mcast; 
    unreachable_nodes[1].seqnum = 23;
    // set the hoplimit to 3 to reduce debugging noise
    writer_send_rerr(unreachable_nodes, 2, 3, &na_mcast);
}

/* 
 * Change or set the metric type.
 * If metric_type does not match any known metric types, no changes will be made.
 */
void aodv_set_metric_type(int metric_type)
{
    if (metric_type != AODVV2_DEFAULT_METRIC_TYPE)
        return;
    _metric_type = metric_type;
}

/* 
 * init the multicast address all RREQs are sent to 
 * and the local address (source address) of this node
 */
static void _init_addresses(void)
{
    /* init multicast address: set to to a link-local all nodes multicast address */
    ipv6_addr_set_all_nodes_addr(&_v6_addr_mcast);
    DEBUG("[aodvv2] my multicast address is: %s\n", ipv6_addr_to_str(addr_str, &_v6_addr_mcast));

    /* get best IP for sending */
    ipv6_iface_get_best_src_addr(&_v6_addr_local, &_v6_addr_mcast);
    DEBUG("[aodvv2] my src address is:       %s\n", ipv6_addr_to_str(addr_str, &_v6_addr_local));

    /* store src & multicast address as netaddr as well for easy interaction 
    with oonf based stuff */
    ipv6_addr_t_to_netaddr(&_v6_addr_local, &na_local);
    ipv6_addr_t_to_netaddr(&_v6_addr_mcast, &na_mcast);

    /* init sockaddr that write_packet will use to send data */
    sa_wp.sin6_family = AF_INET6;
    sa_wp.sin6_port = HTONS(MANET_PORT);
}

/* Init everything needed for socket communication */
static void _init_sock_snd(void)
{
    _sock_snd = destiny_socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    if(-1 == _sock_snd) {
        DEBUG("[aodvv2] Error Creating Socket!");
        return;
    }
}

/* receive RREQs and RREPs and handle them */
static void _aodv_receiver_thread(void)
{
    DEBUG("[aodvv2] %s()\n", __func__);
    uint32_t fromlen;
    int32_t rcv_size;
    char buf_rcv[UDP_BUFFER_SIZE];
    char addr_str_rec[IPV6_MAX_ADDR_STR_LEN];
    msg_t msg_q[RCV_MSG_Q_SIZE];
    
    msg_init_queue(msg_q, RCV_MSG_Q_SIZE);

    sockaddr6_t sa_rcv = { .sin6_family = AF_INET6,
                           .sin6_port = HTONS(MANET_PORT) };

    int sock_rcv = destiny_socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    
    if (-1 == destiny_socket_bind(sock_rcv, &sa_rcv, sizeof(sa_rcv))) {
        DEBUG("Error: bind to recieve socket failed!\n");
        destiny_socket_close(sock_rcv);
    }

    DEBUG("[aodvv2] ready to receive data\n");
    for(;;) {
        rcv_size = destiny_socket_recvfrom(sock_rcv, (void *)buf_rcv, UDP_BUFFER_SIZE, 0, 
                                          &sa_rcv, &fromlen);

        if(rcv_size < 0) {
            DEBUG("[aodvv2] ERROR receiving data!\n");
        }
        DEBUG("[aodvv2] UDP packet received from %s\n", ipv6_addr_to_str(addr_str_rec, &sa_rcv.sin6_addr));
        
        struct netaddr _sender;
        ipv6_addr_t_to_netaddr(&sa_rcv.sin6_addr, &_sender);
        reader_handle_packet((void*) buf_rcv, rcv_size, &_sender);
    }

    destiny_socket_close(sock_rcv);    
}

/**
 * This function is set as ipv6_iface_routing_provider and will be called by 
 * RIOT's ipv6_sendto() to determine the next hop it should send a packet to dest to.
 * @param dest 
 * @return ipv6_addr_t* of the next hop towards dest if there is any, NULL if there is no next hop (yet)
 */
static ipv6_addr_t* aodv_get_next_hop(ipv6_addr_t* dest)
{
    DEBUG("[aodvv2] getting next hop for %s\n", ipv6_addr_to_str(addr_str, dest));

    struct netaddr _tmp_dest;
    ipv6_addr_t_to_netaddr(dest, &_tmp_dest);
    timex_t now;

    /*
    ipv6_addr_t* next_hop = (ipv6_addr_t*) routingtable_get_next_hop(&_tmp_dest, _metric_type);
    if (next_hop){
        DEBUG("\t found dest in routing table: %s\n", netaddr_to_string(&nbuf, next_hop));
        return next_hop;
    }*/

    struct aodvv2_routing_entry_t* rt_entry = routingtable_get_entry(&_tmp_dest, _metric_type);
    if (rt_entry) {
        if (rt_entry->state == ROUTE_STATE_BROKEN ||
            rt_entry->state == ROUTE_STATE_EXPIRED ||
            rt_entry->broken = true) {
            DEBUG("\tRouting table entry found, but invalid. Sending RERR.\n");
            // TODO send rerr
            struct unreachable_node unreachable_nodes[1];
            unreachable_nodes[0].addr = _tmp_dest;
            unreachable_nodes[0].seqnum = rt_entry->seqNum;
            writer_send_rerr(unreachable_nodes, 1, AODVV2_MAX_HOPCOUNT, &na_mcast);
            return NULL;
        }

        DEBUG("\t found dest in routing table: %s\n", netaddr_to_string(&nbuf, &rt_entry->nextHopAddress));
        vtimer_now(&now);
        rt_entry->lastUsed = now;

        return &rt_entry->nextHopAddress;
    } 

    /* no route found => start route discovery */
    writer_send_rreq(&na_local, &_tmp_dest, &na_mcast);

    return NULL;
}

/**
 * Handle the output of the RFC5444 packet creation process
 * @param wr
 * @param iface
 * @param buffer
 * @param length
 */
static void _write_packet(struct rfc5444_writer *wr __attribute__ ((unused)),
        struct rfc5444_writer_target *iface __attribute__((unused)),
        void *buffer, size_t length)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    /* generate hexdump and human readable representation of packet
       and print to console */
    abuf_hexdump(&_hexbuf, "\t", buffer, length);
    rfc5444_print_direct(&_hexbuf, buffer, length);
    DEBUG("%s", abuf_getptr(&_hexbuf));
    abuf_clear(&_hexbuf);
    
    /* fetch the address the packet is supposed to be sent to (i.e. to a 
       specific node or the multicast address) from the writer_target struct
       iface* is stored in. This is a bit hacky, but it does the trick. */
    wt = container_of(iface, struct writer_target, interface);
    netaddr_to_ipv6_addr_t(&wt->target_address, &sa_wp.sin6_addr);

    /* When originating a RREQ, add it to our RREQ table/update its predecessor */
    if (ipv6_addr_is_equal(&sa_wp.sin6_addr, &_v6_addr_mcast)
        && netaddr_cmp(&wt->_packet_data.origNode.addr, &na_local) == 0) {
        DEBUG("[aodvv2] originating RREQ; updating RREQ table...\n");
        rreqtable_is_redundant(&wt->_packet_data);
    }

    int bytes_sent = destiny_socket_sendto(_sock_snd, buffer, length, 
                                            0, &sa_wp, sizeof sa_wp);

    DEBUG("[aodvv2] %d bytes sent.\n", bytes_sent);
}
