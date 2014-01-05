/* beware, for these are dummy values */
#define AODVV2_MAX_HOPCOUNT 20
#define AODVV2_MAX_ROUTING_ENTRIES 255
#define AODVV2_DEFAULT_METRIC_TYPE 3
#define AODVV2_ACTIVE_INTERVAL 5 // seconds
#define AODVV2_MAX_IDLETIME 10  // seconds // TODO: find proper value

/* RFC5498 */
#define MANET_PORT  269

// I'M SO SORRY
#define MY_IP "::42"

enum msg_type {
    RFC5444_MSGTYPE_RREQ,
    RFC5444_MSGTYPE_RREP,
};

enum tlv_type {
    RFC5444_MSGTLV_SEQNUM,    
    RFC5444_MSGTLV_ORIGNODE_SEQNUM,
    RFC5444_MSGTLV_TARGNODE_SEQNUM,
    RFC5444_MSGTLV_METRIC,
};
