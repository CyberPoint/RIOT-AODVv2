
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef RIOT
#include "destiny/socket.h"
#include "inet_ntop.h"
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "common/netaddr.h"
#include "rfc5444/rfc5444_reader.h"
#include "rfc5444/rfc5444_print.h"

#include "constants.h"

static enum rfc5444_result _rfc5444_to_json_cb_print_pkt_start(
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _rfc5444_to_json_cb_print_pkt_tlv(
    struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _rfc5444_to_json_cb_print_pkt_end(
    struct rfc5444_reader_tlvblock_context *context, bool);
static enum rfc5444_result _rfc5444_to_json_cb_print_msg_start(
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _rfc5444_to_json_cb_print_msg_tlv(
    struct rfc5444_reader_tlvblock_entry *tlv, struct rfc5444_reader_tlvblock_context *context) ;
static enum rfc5444_result _rfc5444_to_json_cb_print_msg_end(
    struct rfc5444_reader_tlvblock_context *context, bool);
static enum rfc5444_result _rfc5444_to_json_cb_print_addr_start(
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _rfc5444_to_json_cb_print_addr_tlv(
    struct rfc5444_reader_tlvblock_entry *tlv, struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _rfc5444_to_json_cb_print_addr_end(
    struct rfc5444_reader_tlvblock_context *context, bool);

static int addr_index = 0; // super duper unsauber, but what the hell.

/**
 * Add a printer for a rfc5444 reader
 * @param session pointer to initialized rfc5444 printer session
 * @param reader pointer to initialized reader
 */
void
rfc5444_to_json_print_add(struct rfc5444_print_session *session,
    struct rfc5444_reader *reader) {
  /* memorize reader */
  session->_reader = reader;

  session->_pkt.start_callback = _rfc5444_to_json_cb_print_pkt_start;
  session->_pkt.tlv_callback = _rfc5444_to_json_cb_print_pkt_tlv;
  session->_pkt.end_callback = _rfc5444_to_json_cb_print_pkt_end;
  rfc5444_reader_add_packet_consumer(reader, &session->_pkt, NULL, 0);

  session->_msg.default_msg_consumer = true;
  session->_msg.start_callback = _rfc5444_to_json_cb_print_msg_start;
  session->_msg.tlv_callback = _rfc5444_to_json_cb_print_msg_tlv;
  session->_msg.end_callback = _rfc5444_to_json_cb_print_msg_end;
  rfc5444_reader_add_message_consumer(reader, &session->_msg, NULL, 0);

  session->_addr.default_msg_consumer = true;
  session->_addr.addrblock_consumer = true;
  session->_addr.start_callback = _rfc5444_to_json_cb_print_addr_start;
  session->_addr.tlv_callback = _rfc5444_to_json_cb_print_addr_tlv;
  session->_addr.end_callback = _rfc5444_to_json_cb_print_addr_end;
  rfc5444_reader_add_message_consumer(reader, &session->_addr, NULL, 0);
}

/**
 * Remove printer from rfc5444 reader
 * @param session pointer to initialized rfc5444 printer session
 */
void
rfc5444_to_json_rfc5444_print_remove(struct rfc5444_print_session *session) {
  rfc5444_reader_remove_message_consumer(session->_reader, &session->_addr);
  rfc5444_reader_remove_message_consumer(session->_reader, &session->_msg);
  rfc5444_reader_remove_packet_consumer(session->_reader, &session->_pkt);
}

/**
 * This function converts a rfc5444 buffer into a human readable
 * form and print it into an buffer. To do this it allocates its own
 * rfc5444 reader, hooks in the printer macros, parse the packet and
 * cleans up the reader again.
 *
 * @param out pointer to output buffer
 * @param buffer pointer to packet to be printed
 * @param length length of packet in bytes
 * @return return code of reader, see rfc5444_result enum
 */
enum rfc5444_result
rfc5444_to_json(struct autobuf *out, void *buffer, size_t length) {
  struct rfc5444_reader reader;
  struct rfc5444_print_session session;
  enum rfc5444_result result;

  memset(&reader, 0, sizeof(reader));
  memset(&session, 0, sizeof(session));

  session.output = out;

  rfc5444_reader_init(&reader);
  rfc5444_to_json_print_add(&session, &reader);

  result = rfc5444_reader_handle_packet(&reader, buffer, length);

  rfc5444_to_json_rfc5444_print_remove(&session);
  rfc5444_reader_cleanup(&reader);

  return result;
}

/**
 * Clear output buffer and print start of packet
 * @param c
 * @param context
 * @return
 */
static enum rfc5444_result
_rfc5444_to_json_cb_print_pkt_start(struct rfc5444_reader_tlvblock_context *context) {
  assert (context->type == RFC5444_CONTEXT_PACKET);

  return RFC5444_OKAY;
}

/**
 * Print packet TLVs
 * @param c
 * @param tlv
 * @param context
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_pkt_tlv(struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *context) {

  assert (context->type == RFC5444_CONTEXT_PACKET);

  return RFC5444_OKAY;
}

/**
 * Print end of packet and call print callback if necessary
 * @param c
 * @param context
 * @param dropped
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_pkt_end(struct rfc5444_reader_tlvblock_context *context,
    bool dropped __attribute__ ((unused))) {

  return RFC5444_OKAY;
}

/**
 * Print start of message
 * @param c
 * @param context
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_msg_start(struct rfc5444_reader_tlvblock_context *context) {
  struct rfc5444_print_session *session;
  struct netaddr_str buf;

  assert (context->type == RFC5444_CONTEXT_MESSAGE);

  session = container_of(context->consumer, struct rfc5444_print_session, _msg);

  abuf_appendf(session->output, "{\"msg-type\": %u, ", context->msg_type);

  if (context->has_hoplimit) {
    abuf_appendf(session->output, "\"msg-hop-limit\": %u, ", context->hoplimit);
  }

  return RFC5444_OKAY;
}

/**
 * Print message TLV
 * @param c
 * @param tlv
 * @param context
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_msg_tlv(struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *context) {

  assert (context->type == RFC5444_CONTEXT_MESSAGE);

  return RFC5444_OKAY;
}

/**
 * Print end of message
 * @param c
 * @param context
 * @param dropped
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_msg_end(struct rfc5444_reader_tlvblock_context *context,
    bool dropped __attribute__ ((unused))) {
  assert (context->type == RFC5444_CONTEXT_MESSAGE);

  return RFC5444_OKAY;
}


/**
 * Print start of address
 * @param c
 * @param context
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_addr_start(struct rfc5444_reader_tlvblock_context *context) {
  struct rfc5444_print_session *session;
  struct netaddr_str buf;

  assert (context->type == RFC5444_CONTEXT_ADDRESS);

  session = container_of(context->consumer, struct rfc5444_print_session, _addr);

  // CAUTION: super scruffy, relies on address order!! (sorry.)
  if (addr_index == 0) {
    abuf_puts(session->output, "\"origNode\": {");
    addr_index++;
  } else {
    abuf_puts(session->output, "\"targNode\":{");
  }

  abuf_appendf(session->output, "\"address\": \"%s\",",
      netaddr_to_string(&buf, &context->addr));
  return RFC5444_OKAY;
}


/**
 * Print address tlv
 * @param c
 * @param tlv
 * @param context
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_addr_tlv(struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *context) {
  struct rfc5444_print_session *session;

  assert (context->type == RFC5444_CONTEXT_ADDRESS);

  session = container_of(context->consumer, struct rfc5444_print_session, _addr);

  abuf_puts(session->output, "");
  if (tlv->type == RFC5444_MSGTLV_ORIGSEQNUM || tlv->type == RFC5444_MSGTLV_TARGSEQNUM ){
    abuf_appendf(session->output, "\"seqnum\": %u,", tlv->single_value);
  } else if (tlv->type = RFC5444_MSGTLV_METRIC) {
    abuf_appendf(session->output, "\"metric\": %u}", tlv->single_value);
  }

  return RFC5444_OKAY;
}

/**
 * Print end of address
 * @param c
 * @param context
 * @param dropped
 * @return
 */
enum rfc5444_result
_rfc5444_to_json_cb_print_addr_end(struct rfc5444_reader_tlvblock_context *context,
    bool dropped __attribute__ ((unused))) {
  struct rfc5444_print_session *session;

  assert (context->type == RFC5444_CONTEXT_ADDRESS);

  session = container_of(context->consumer, struct rfc5444_print_session, _addr);

  abuf_puts(session->output, "}\n");
  return RFC5444_OKAY;
}
