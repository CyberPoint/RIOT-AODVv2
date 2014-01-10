#include <string.h>
#include <stdio.h>
#ifdef RIOT
#include "net_help.h"
#endif

#include "aodvv2_writer.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

/**
 * Writer to create aodvv2 RFC5444 RREQ and RREP messages.
 * Please note that this is work under construction, specifically:
 * - The messageTLVs will be changed into Address TLVs or Address Block TLVs as
 *   soon as Charlie has answered my questions on the subject
 * - The Packet header data is bullshit and only serves as an example for me 
 **/

static void _cb_rreq_addMessageTLVs(struct rfc5444_writer *wr);
static void _cb_rreq_addAddresses(struct rfc5444_writer *wr);
static void _cb_rreq_addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *message);

static void _cb_rrep_addMessageTLVs(struct rfc5444_writer *wr);
static void _cb_rrep_addAddresses(struct rfc5444_writer *wr);
static void _cb_rrep_addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *message);

static void _cb_addPacketHeader(struct rfc5444_writer *wr, struct rfc5444_writer_target *interface_1);

static int _msg_buffer[128];
static int _msg_addrtlvs[1000];
static int _packet_buffer[128];

static struct rfc5444_writer_message *_rreq_msg;
static struct rfc5444_writer_message *_rrep_msg;

static struct{
    struct netaddr origNode;
    struct netaddr targNode;
} rreq_packet_data;

static struct aodvv2_packet_data rrep_packet_data;

/**
 * Callback to define the packet header for a RFC5444 packet. This is actually
 * quite useless because a RREP/RREQ packet header should have no flags set, but anyway,
 * here's how you do it in case you need to.
 */
static void
_cb_addPacketHeader(struct rfc5444_writer *wr, struct rfc5444_writer_target *interface_1)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    /* set header with sequence number */
    rfc5444_writer_set_pkt_header(wr, interface_1, true);
}

/*
 * message content provider that will add message TLVs,
 * addresses and address block TLVs to all messages of type RREQ.
 */
static struct rfc5444_writer_content_provider _rreq_message_content_provider = {
    .msg_type = RFC5444_MSGTYPE_RREQ,
    .addAddresses = _cb_rreq_addAddresses,
};

/* declaration of all address TLVs added to the RREQ message */
static struct rfc5444_writer_tlvtype _rreq_addrtlvs[] = {
    [RFC5444_MSGTLV_ORIGNODE_SEQNUM] = { .type = RFC5444_MSGTLV_SEQNUM, .exttype = RFC5444_MSGTLV_ORIGNODE_SEQNUM },
    [RFC5444_MSGTLV_METRIC] = { .type = AODVV2_DEFAULT_METRIC_TYPE },
};

/*
 * message content provider that will add message TLVs,
 * addresses and address block TLVs to all messages of type RREQ.
 */
static struct rfc5444_writer_content_provider _rrep_message_content_provider = {
    .msg_type = RFC5444_MSGTYPE_RREP,
    .addAddresses = _cb_rrep_addAddresses,
};

/* declaration of all address TLVs added to the RREP message */
static struct rfc5444_writer_tlvtype _rrep_addrtlvs[] = {
    [RFC5444_MSGTLV_ORIGNODE_SEQNUM] = { .type = RFC5444_MSGTLV_SEQNUM, .exttype = RFC5444_MSGTLV_ORIGNODE_SEQNUM },
    [RFC5444_MSGTLV_TARGNODE_SEQNUM] = { .type = RFC5444_MSGTLV_SEQNUM, .exttype = RFC5444_MSGTLV_TARGNODE_SEQNUM },
    [RFC5444_MSGTLV_METRIC] = { .type = AODVV2_DEFAULT_METRIC_TYPE },
};

/**
 * Callback to define the message header for a RFC5444 RREQ message
 * @param wr
 * @param message
 */
static void
_cb_rreq_addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *message)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    /* no originator, no hopcount, has hoplimit, no seqno */
    rfc5444_writer_set_msg_header(wr, message, false, false, true, false);
    rfc5444_writer_set_msg_hoplimit(wr, message, AODVV2_MAX_HOPCOUNT);
}

/**
 * Callback to add addresses and address TLVs to a RFC5444 RREQ message
 * @param wr
 */
static void
_cb_rreq_addAddresses(struct rfc5444_writer *wr)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    struct rfc5444_writer_address *origNode_addr, *targNode_addr;

    uint16_t origNode_seqNum = get_seqNum();
    inc_seqNum();
    
    uint8_t origNode_hopCt = 0;

    /* add origNode address (has no address tlv); is mandatory address */
    origNode_addr = rfc5444_writer_add_address(wr, _rreq_message_content_provider.creator, &rreq_packet_data.origNode, true);

    /* add origNode address (has no address tlv); is mandatory address */
    targNode_addr = rfc5444_writer_add_address(wr, _rreq_message_content_provider.creator, &rreq_packet_data.targNode, true);

    /* add SeqNum TLV and metric TLV to origNode */
    // TODO: allow_dup true or false?
    rfc5444_writer_add_addrtlv(wr, origNode_addr, &_rreq_addrtlvs[RFC5444_MSGTLV_ORIGNODE_SEQNUM], &origNode_seqNum, sizeof(origNode_seqNum), false);
    rfc5444_writer_add_addrtlv(wr, origNode_addr, &_rreq_addrtlvs[RFC5444_MSGTLV_METRIC], &origNode_hopCt, sizeof(origNode_hopCt), false);
}

/**
 * Callback to define the message header for a RFC5444 RREQ message
 * @param wr
 * @param message
 */
static void
_cb_rrep_addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *message)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    /* no originator, no hopcount, has hoplimit, no seqno */
    rfc5444_writer_set_msg_header(wr, message, false, false, true, false);
    rfc5444_writer_set_msg_hoplimit(wr, message, AODVV2_MAX_HOPCOUNT);
}

/**
 * Callback to add addresses and address TLVs to a RFC5444 RREQ message
 * @param wr
 */
static void
_cb_rrep_addAddresses(struct rfc5444_writer *wr)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    struct rfc5444_writer_address *origNode_addr, *targNode_addr;
    struct netaddr na_origNode, na_targNode;

    uint16_t origNode_seqNum = rrep_packet_data.origNode.seqNum;
    
    uint16_t targNode_seqNum = get_seqNum();
    inc_seqNum();

    uint8_t targNode_hopCt = rrep_packet_data.targNode.metric;

    /* add origNode address (has no address tlv); is mandatory address */
    origNode_addr = rfc5444_writer_add_address(wr, _rrep_message_content_provider.creator, &rrep_packet_data.origNode.addr, true);

    /* add origNode address (has no address tlv); is mandatory address */
    targNode_addr = rfc5444_writer_add_address(wr, _rrep_message_content_provider.creator, &rrep_packet_data.targNode.addr, true);

    /* add OrigNode and TargNode SeqNum TLVs */
    // TODO: allow_dup true or false?
    rfc5444_writer_add_addrtlv(wr, origNode_addr, &_rrep_addrtlvs[RFC5444_MSGTLV_ORIGNODE_SEQNUM], &origNode_seqNum, sizeof(origNode_seqNum), false);
    rfc5444_writer_add_addrtlv(wr, targNode_addr, &_rrep_addrtlvs[RFC5444_MSGTLV_TARGNODE_SEQNUM], &targNode_seqNum, sizeof(targNode_seqNum), false);

    /* Add Metric TLV to targNode Address */
    rfc5444_writer_add_addrtlv(wr, targNode_addr, &_rrep_addrtlvs[RFC5444_MSGTLV_METRIC], &targNode_hopCt, sizeof(targNode_hopCt), false);
}

void writer_init(write_packet_func_ptr ptr)
{
    DEBUG("[aodvv2] %s()\n", __func__);

    /* define interface for generating rfc5444 packets */
    interface_1.packet_buffer = _packet_buffer;
    interface_1.packet_size = sizeof(_packet_buffer);
    interface_1.addPacketHeader = _cb_addPacketHeader;
    /* set function to send binary packet content */
    interface_1.sendPacket = ptr;
    
    /* define the rfc5444 writer */
    writer.msg_buffer = _msg_buffer;
    writer.msg_size = sizeof(_msg_buffer);
    writer.addrtlv_buffer = _msg_addrtlvs;
    writer.addrtlv_size = sizeof(_msg_addrtlvs);

    /* initialize writer */
    rfc5444_writer_init(&writer);

    /* register a target (for sending messages to) in writer */
    rfc5444_writer_register_target(&writer, &interface_1);

    /* register a message content providers for RREQ and RREP */
    rfc5444_writer_register_msgcontentprovider(&writer, &_rreq_message_content_provider, _rreq_addrtlvs, ARRAYSIZE(_rreq_addrtlvs));
    rfc5444_writer_register_msgcontentprovider(&writer, &_rrep_message_content_provider, _rrep_addrtlvs, ARRAYSIZE(_rrep_addrtlvs));

    /* register rreq and rrep messages with 16 byte (ipv6) addresses.
       AddPacketHeader & addMessageHeader callbacks are triggered here. */
    _rreq_msg = rfc5444_writer_register_message(&writer, RFC5444_MSGTYPE_RREQ, false, RFC5444_MAX_ADDRLEN);
    _rrep_msg = rfc5444_writer_register_message(&writer, RFC5444_MSGTYPE_RREP, false, RFC5444_MAX_ADDRLEN);

    _rreq_msg->addMessageHeader = _cb_rreq_addMessageHeader;
    _rrep_msg->addMessageHeader = _cb_rrep_addMessageHeader;
}

void writer_send_rreq(struct netaddr* na_origNode, struct netaddr* na_targNode)
{
    DEBUG("[RREQ]\n");

    // TODO: geht das auch eleganter?
    if (na_origNode == NULL || na_targNode == NULL)
        return;

    // TODO das ist doch unschön so. (memcpy?!)
    rreq_packet_data.origNode = *na_origNode;
    rreq_packet_data.targNode = *na_targNode;

    rfc5444_writer_create_message_alltarget(&writer, RFC5444_MSGTYPE_RREQ);
    //memcpy(&rreq_packet_data.origNode, na_origNode, sizeof(struct netaddr));
    //memcpy(&rreq_packet_data.targNode, na_targNode, sizeof(struct netaddr));
    rfc5444_writer_flush(&writer, &interface_1, false);
}

void writer_send_rrep(struct aodvv2_packet_data* packet_data)
{
    DEBUG("[RREP]\n");

    if (packet_data == NULL)
        return;

    memcpy(&rrep_packet_data, packet_data, sizeof(struct aodvv2_packet_data));

    rfc5444_writer_create_message_alltarget(&writer, RFC5444_MSGTYPE_RREP);
    rfc5444_writer_flush(&writer, &interface_1, false);
}

void writer_cleanup(void)
{
    DEBUG("[aodvv2] %s()\n", __func__);
}