
enum tlv_type {
    RFC5444_MSGTLV_SEQNUM, /* could be omitted & replaced with ORIGNODE_SEQNUM */
    RFC5444_MSGTLV_ORIGNODE_SEQNUM,
    RFC5444_MSGTLV_TARGNODE_SEQNUM,
    RFC5444_MSGTLV_METRIC,
};

enum msg_type {
    RFC5444_MSGTYPE_RREQ,
    RFC5444_MSGTYPE_RREP,
};