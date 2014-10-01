/**
 * @file stack.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * Parts of this module were derived from GPL licensed PJSIP sample code
 * with the following copyrights.
 *   Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *   Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include "pjsip-simple/evsub.h"
}

#include <arpa/inet.h>

// Common STL includes.
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <string>

#include "constants.h"
#include "eventq.h"
#include "pjutils.h"
#include "log.h"
#include "sas.h"
#include "saslogger.h"
#include "sproutsasevent.h"
#include "stack.h"
#include "utils.h"
#include "zmq_lvc.h"
#include "statistic.h"
#include "custom_headers.h"
#include "utils.h"
#include "accumulator.h"
#include "connection_tracker.h"
#include "quiescing_manager.h"
#include "load_monitor.h"
#include "counter.h"
#include "sprout_ent_definitions.h"

class StackQuiesceHandler;

struct stack_data_struct stack_data;

static std::vector<pj_thread_t*> pjsip_threads;
static std::vector<pj_thread_t*> worker_threads;
static volatile pj_bool_t quit_flag;

// Queue for incoming messages.
struct rx_msg_qe
{
  pjsip_rx_data* rdata;    // received message
  Utils::StopWatch stop_watch;    // stop watch for tracking message latency
};
eventq<struct rx_msg_qe> rx_msg_q;

// Deadlock detection threshold for the message queue (in milliseconds).  This
// is set to roughly twice the expected maximum service time for each message
// (currently four seconds, allowing for four Homestead/Homer interactions
// from a single request, each with a possible 500ms timeout).
static const int MSG_Q_DEADLOCK_TIME = 4000;

static Accumulator* latency_accumulator;
static Accumulator* queue_size_accumulator;
static Counter* requests_counter;
static Counter* overload_counter;

static LoadMonitor *load_monitor = NULL;
static QuiescingManager *quiescing_mgr = NULL;
static StackQuiesceHandler *stack_quiesce_handler = NULL;
static ConnectionTracker *connection_tracker = NULL;

// We register a single module to handle scheduling plus local and
// SAS logging.
static pj_bool_t on_rx_msg(pjsip_rx_data* rdata);
static pj_status_t on_tx_msg(pjsip_tx_data* tdata);

static pjsip_module mod_stack =
{
  NULL, NULL,                           /* prev, next.          */
  pj_str("mod-stack"),                  /* Name.                */
  -1,                                   /* Id                   */
  PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1, /* Priority             */
  NULL,                                 /* load()               */
  NULL,                                 /* start()              */
  NULL,                                 /* stop()               */
  NULL,                                 /* unload()             */
  &on_rx_msg,                           /* on_rx_request()      */
  &on_rx_msg,                           /* on_rx_response()     */
  &on_tx_msg,                           /* on_tx_request()      */
  &on_tx_msg,                           /* on_tx_response()     */
  NULL,                                 /* on_tsx_state()       */
};

const static std::string _known_statnames[] = {
  "client_count",
  "connected_homers",
  "connected_homesteads",
  "connected_sprouts",
  "latency_us",
  "hss_latency_us",
  "hss_digest_latency_us",
  "hss_subscription_latency_us",
  "xdm_latency_us",
  "incoming_requests",
  "rejected_overload",
  "queue_size",
  "hss_user_auth_latency_us",
  "hss_location_latency_us",
  "connected_ralfs",
};

const static std::string SPROUT_ZMQ_PORT = "6666";
const static std::string BONO_ZMQ_PORT = "6669";

const std::string* known_statnames = _known_statnames;
const int num_known_stats = sizeof(_known_statnames) / sizeof(std::string);

/// PJSIP threads are donated to PJSIP to handle receiving at transport level
/// and timers.
static int pjsip_thread(void *p)
{
  pj_time_val delay = {0, 10};

  PJ_UNUSED_ARG(p);

  LOG_DEBUG("PJSIP thread started");

  while (!quit_flag)
  {
    pjsip_endpt_handle_events(stack_data.endpt, &delay);
  }

  LOG_DEBUG("PJSIP thread ended");

  return 0;
}


/// Worker threads handle most SIP message processing.
static int worker_thread(void* p)
{
  // Set up data to always process incoming messages at the first PJSIP
  // module after our module.
  pjsip_process_rdata_param rp;
  pjsip_process_rdata_param_default(&rp);
  rp.start_mod = &mod_stack;
  rp.idx_after_start = 1;

  LOG_DEBUG("Worker thread started");

  struct rx_msg_qe qe = {0};

  while (rx_msg_q.pop(qe))
  {
    pjsip_rx_data* rdata = qe.rdata;
    if (rdata)
    {
      LOG_DEBUG("Worker thread dequeue message %p", rdata);
      pjsip_endpt_process_rx_data(stack_data.endpt, rdata, &rp, NULL);
      LOG_DEBUG("Worker thread completed processing message %p", rdata);
      pjsip_rx_data_free_cloned(rdata);

      unsigned long latency_us;
      if (qe.stop_watch.read(latency_us))
      {
        LOG_DEBUG("Request latency = %ldus", latency_us);
        latency_accumulator->accumulate(latency_us);
        load_monitor->request_complete(latency_us);
      }
      else
      {
        LOG_ERROR("Failed to get done timestamp: %s", strerror(errno));
      }
    }
  }

  LOG_DEBUG("Worker thread ended");

  return 0;
}


static void local_log_rx_msg(pjsip_rx_data* rdata)
{
  LOG_VERBOSE("RX %d bytes %s from %s %s:%d:\n"
              "--start msg--\n\n"
              "%.*s\n"
              "--end msg--",
              rdata->msg_info.len,
              pjsip_rx_data_get_info(rdata),
              rdata->tp_info.transport->type_name,
              rdata->pkt_info.src_name,
              rdata->pkt_info.src_port,
              (int)rdata->msg_info.len,
              rdata->msg_info.msg_buf);
}


static void local_log_tx_msg(pjsip_tx_data* tdata)
{
  LOG_VERBOSE("TX %d bytes %s to %s %s:%d:\n"
              "--start msg--\n\n"
              "%.*s\n"
              "--end msg--",
              (tdata->buf.cur - tdata->buf.start),
              pjsip_tx_data_get_info(tdata),
              tdata->tp_info.transport->type_name,
              tdata->tp_info.dst_name,
              tdata->tp_info.dst_port,
              (int)(tdata->buf.cur - tdata->buf.start),
              tdata->buf.start);
}


static void sas_log_rx_msg(pjsip_rx_data* rdata)
{
  SAS::TrailId trail = 0;

  if (rdata->msg_info.msg->type == PJSIP_RESPONSE_MSG)
  {
    // Message is a response, so try to correlate to an existing UAC
    // transaction using the top-most Via header.
    pj_str_t key;
    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_ROLE_UAC,
                         &rdata->msg_info.cseq->method, rdata);
    pjsip_transaction* tsx = pjsip_tsx_layer_find_tsx(&key, PJ_TRUE);
    if (tsx)
    {
      // Found the UAC transaction, so get the trail if there is one.
      trail = get_trail(tsx);

      // Unlock tsx because it is locked in find_tsx()
      pj_grp_lock_release(tsx->grp_lock);
    }
  }
  else if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD)
  {
    // Message is an ACK, so try to correlate it to the existing UAS
    // transaction using the top-most Via header.
    pj_str_t key;
    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_UAS_ROLE,
                         &rdata->msg_info.cseq->method, rdata);
    pjsip_transaction* tsx = pjsip_tsx_layer_find_tsx(&key, PJ_TRUE);
    if (tsx)
    {
      // Found the UAS transaction, so get the trail if there is one.
      trail = get_trail(tsx);

      // Unlock tsx because it is locked in find_tsx()
      pj_grp_lock_release(tsx->grp_lock);
    }
  }
  else if (rdata->msg_info.msg->line.req.method.id == PJSIP_CANCEL_METHOD)
  {
    // Message is a CANCEL request chasing an INVITE, so we want to try to
    // correlate it to the INVITE trail for the purposes of SAS tracing.
    pj_str_t key;
    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_UAS_ROLE,
                         pjsip_get_invite_method(), rdata);
    pjsip_transaction* tsx = pjsip_tsx_layer_find_tsx(&key, PJ_TRUE);
    if (tsx)
    {
      // Found the INVITE UAS transaction, so get the trail if there is one.
      trail = get_trail(tsx);

      // Unlock tsx because it is locked in find_tsx()
      pj_grp_lock_release(tsx->grp_lock);
    }
  }

  if (trail == 0)
  {
    // The message doesn't correlate to an existing trail, so create a new
    // one.
    trail = SAS::new_trail(1u);
  }

  // Store the trail in the message as it gets passed up the stack.
  set_trail(rdata, trail);

  // Log the message event.
  SAS::Event event(trail, SASEvent::RX_SIP_MSG, 0);
  event.add_static_param(pjsip_transport_get_type_from_flag(rdata->tp_info.transport->flag));
  event.add_static_param(rdata->pkt_info.src_port);
  event.add_var_param(rdata->pkt_info.src_name);
  event.add_var_param(rdata->msg_info.len, rdata->msg_info.msg_buf);
  SAS::report_event(event);
}


static void sas_log_tx_msg(pjsip_tx_data *tdata)
{
  // For outgoing messages always use the trail identified in the module data
  SAS::TrailId trail = get_trail(tdata);

  if (trail != 0)
  {
    // Log the message event.
    SAS::Event event(trail, SASEvent::TX_SIP_MSG, 0);
    event.add_static_param(pjsip_transport_get_type_from_flag(tdata->tp_info.transport->flag));
    event.add_static_param(tdata->tp_info.dst_port);
    event.add_var_param(tdata->tp_info.dst_name);
    event.add_var_param((int)(tdata->buf.cur - tdata->buf.start), tdata->buf.start);
    SAS::report_event(event);
  }
  else
  {
    LOG_ERROR("Transmitting message with no SAS trail identifier\n%.*s",
              (int)(tdata->buf.cur - tdata->buf.start),
              tdata->buf.start);
  }
}


static pj_bool_t on_rx_msg(pjsip_rx_data* rdata)
{
  // Do logging.
  local_log_rx_msg(rdata);
  sas_log_rx_msg(rdata);

  requests_counter->increment();

  // Check whether the request should be processed
  if (!(load_monitor->admit_request()) &&
      (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG) &&
      (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD))
  {
    // Discard non-ACK requests if there are no available tokens.
    // Respond statelessly with a 503 Service Unavailable, including a
    // Retry-After header with a zero length timeout.
    LOG_DEBUG("Rejected request due to overload");

    pjsip_cid_hdr* cid = (pjsip_cid_hdr*)rdata->msg_info.cid;

    SAS::TrailId trail = get_trail(rdata);

    SAS::Marker start_marker(trail, MARKER_ID_START, 1u);
    SAS::report_marker(start_marker);

    SAS::Event event(trail, SASEvent::SIP_OVERLOAD, 0);
    event.add_static_param(load_monitor->get_target_latency());
    event.add_static_param(load_monitor->get_current_latency());
    event.add_static_param(load_monitor->get_rate_limit());
    SAS::report_event(event);

    PJUtils::report_sas_to_from_markers(trail, rdata->msg_info.msg);

    if ((rdata->msg_info.msg->line.req.method.id == PJSIP_REGISTER_METHOD) ||
        ((pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, pjsip_get_subscribe_method())) == 0) ||
        ((pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, pjsip_get_notify_method())) == 0))
    {
      // Omit the Call-ID for these requests, as the same Call-ID can be
      // reused over a long period of time and produce huge SAS trails.
      PJUtils::mark_sas_call_branch_ids(trail, NULL, rdata->msg_info.msg);
    }
    else
    {
      PJUtils::mark_sas_call_branch_ids(trail, cid, rdata->msg_info.msg);
    }

    SAS::Marker end_marker(trail, MARKER_ID_END, 1u);
    SAS::report_marker(end_marker);

    pjsip_retry_after_hdr* retry_after = pjsip_retry_after_hdr_create(rdata->tp_info.pool, 0);
    PJUtils::respond_stateless(stack_data.endpt,
                               rdata,
                               PJSIP_SC_SERVICE_UNAVAILABLE,
                               NULL,
                               (pjsip_hdr*)retry_after,
                               NULL);

    // We no longer terminate TCP connections on overload as the shutdown has
    // to wait for existing transactions to end and therefore it takes too
    // long to get feedback to the downstream node.  We expect downstream nodes
    // to rebalance load if possible triggered by receipt of the 503 responses.

    overload_counter->increment();
    return PJ_TRUE;
  }

  // Check that the worker threads are not all deadlocked.
  if (rx_msg_q.is_deadlocked())
  {
    // The queue has not been serviced for sufficiently long to imply that
    // all the worker threads are deadlock, so exit the process so it will be
    // restarted.
    CL_SPROUT_SIP_DEADLOCK.log();
    LOG_ERROR("Detected worker thread deadlock - exiting");
    abort();
  }

  // Before we start, get a timestamp.  This will track the time from
  // receiving a message to forwarding it on (or rejecting it).
  struct rx_msg_qe qe;
  qe.stop_watch.start();

  // Notify the connection tracker that the transport is active.
  connection_tracker->connection_active(rdata->tp_info.transport);

  // Clone the message and queue it to a scheduler thread.
  pjsip_rx_data* clone_rdata;
  pj_status_t status = pjsip_rx_data_clone(rdata, 0, &clone_rdata);

  if (status != PJ_SUCCESS)
  {
    // Failed to clone the message, so drop it.
    LOG_ERROR("Failed to clone incoming message (%s)", PJUtils::pj_status_to_string(status).c_str());
    return PJ_TRUE;
  }

  // Make sure the trail identifier is passed across.
  set_trail(clone_rdata, get_trail(rdata));

  // @TODO - need to think about back-pressure mechanisms.  For example,
  // should we have a maximum depth of queue and drop messages after that?
  // May be better to hold on to the message until the queue has space - this
  // will force back pressure on the particular TCP connection.  Or should we
  // have a queue per transport and round-robin them?

  LOG_DEBUG("Queuing cloned received message %p for worker threads", clone_rdata);
  qe.rdata = clone_rdata;

  // Track the current queue size
  queue_size_accumulator->accumulate(rx_msg_q.size());
  rx_msg_q.push(qe);

  // return TRUE to flag that we have absorbed the incoming message.
  return PJ_TRUE;
}


static pj_status_t on_tx_msg(pjsip_tx_data* tdata)
{
  // Do logging.
  local_log_tx_msg(tdata);
  sas_log_tx_msg(tdata);

  // Return success so the message gets transmitted.
  return PJ_SUCCESS;
}


static void pjsip_log_handler(int level,
                              const char* data,
                              int len)
{
  switch (level) {
  case 0:
  case 1: level = 0; break;
  case 2: level = 1; break;
  case 3: level = 3; break;
  case 4: level = 4; break;
  case 5:
  case 6:
  default: level = 5; break;
  }

  Log::write(level, "pjsip", 0, data);
}


void init_pjsip_logging(int log_level,
                        pj_bool_t log_to_file,
                        const std::string& directory)
{
  pj_log_set_level(log_level);
  pj_log_set_decor(PJ_LOG_HAS_SENDER);
  pj_log_set_log_func(&pjsip_log_handler);
}


pj_status_t fill_transport_details(int port,
                                   pj_sockaddr *addr,
                                   pj_str_t& host,
                                   pjsip_host_port *published_name)
{
  pj_status_t status;
  unsigned count = 1;
  pj_addrinfo addr_info[count];
  int af = pj_AF_UNSPEC();

  // Use pj_getaddrinfo() to convert the localhost string into an IPv4 or IPv6 address in
  // a pj_sockaddr structure.  The localhost string could be an IP address in string format
  // or a hostname that needs to be resolved.  The localhost string should only contain a
  // single address or hostname.
  // Bono/Sprout needs to bind to the local host, but use the host passed into this
  // function in the route header (which can be the local or public host)
  status = pj_getaddrinfo(af, &stack_data.local_host, &count, addr_info);
  if (status != PJ_SUCCESS)
  {
    LOG_ERROR("Failed to decode IP address %.*s (%s)",
              stack_data.local_host.slen,
              stack_data.local_host.ptr,
              PJUtils::pj_status_to_string(status).c_str());
    return status;
  }

  pj_memcpy(addr, &addr_info[0].ai_addr, sizeof(pj_sockaddr));

  // Set up the port in the appropriate part of the structure.
  if (addr->addr.sa_family == PJ_AF_INET)
  {
    addr->ipv4.sin_port = pj_htons((pj_uint16_t)port);
  }
  else if (addr->addr.sa_family == PJ_AF_INET6)
  {
    addr->ipv6.sin6_port =  pj_htons((pj_uint16_t)port);
  }
  else
  {
    status = PJ_EAFNOTSUP;
  }

  published_name->host = host;
  published_name->port = port;

  return status;
}


pj_status_t create_udp_transport(int port, pj_str_t& host)
{
  pj_status_t status;
  pj_sockaddr addr;
  pjsip_host_port published_name;

  status = fill_transport_details(port, &addr, host, &published_name);
  if (status != PJ_SUCCESS)
  {
    return status;
  }

  // The UDP function call depends on the address type, which should be IPv4
  // or IPv6, otherwise something has gone wrong so don't try to start transport.
  if (addr.addr.sa_family == PJ_AF_INET)
  {
    status = pjsip_udp_transport_start(stack_data.endpt,
                                       &addr.ipv4,
                                       &published_name,
                                       50,
                                       NULL);
  }
  else if (addr.addr.sa_family == PJ_AF_INET6)
  {
    status = pjsip_udp_transport_start6(stack_data.endpt,
                                        &addr.ipv6,
                                        &published_name,
                                        50,
                                        NULL);
  }
  else
  {
    status = PJ_EAFNOTSUP;
  }

  if (status != PJ_SUCCESS)
  {
    CL_SPROUT_SIP_UDP_IFC_START_FAIL.log(port, PJUtils::pj_status_to_string(status).c_str());
    LOG_ERROR("Failed to start UDP transport for port %d (%s)", port, PJUtils::pj_status_to_string(status).c_str());
  }

  return status;
}


pj_status_t create_tcp_listener_transport(int port, pj_str_t& host, pjsip_tpfactory **tcp_factory)
{
  pj_status_t status;
  pj_sockaddr addr;
  pjsip_host_port published_name;
  pjsip_tcp_transport_cfg cfg;

  status = fill_transport_details(port, &addr, host, &published_name);
  if (status != PJ_SUCCESS)
  {
    return status;
  }

  // pjsip_tcp_transport_start2() builds up a configuration structure then calls
  // through to pjsip_tcp_transport_start3().  However it only supports IPv4.
  // Therefore setup the config structure and use pjsip_tcp_transport_start3()
  // instead.

  if (addr.addr.sa_family == PJ_AF_INET)
  {
    pjsip_tcp_transport_cfg_default(&cfg, pj_AF_INET());
  }
  else if (addr.addr.sa_family == PJ_AF_INET6)
  {
    pjsip_tcp_transport_cfg_default(&cfg, pj_AF_INET6());
  }
  else
  {
    status = PJ_EAFNOTSUP;
    CL_SPROUT_SIP_TCP_START_FAIL.log(port,
				     PJUtils::pj_status_to_string(status).c_str());
    LOG_ERROR("Failed to start TCP transport for port %d  (%s)",
              port,
              PJUtils::pj_status_to_string(status).c_str());
    return status;
  }

  pj_sockaddr_cp(&cfg.bind_addr, &addr);
  pj_memcpy(&cfg.addr_name, &published_name, sizeof(published_name));
  cfg.async_cnt = 50;

  status = pjsip_tcp_transport_start3(stack_data.endpt, &cfg, tcp_factory);

  if (status != PJ_SUCCESS)
  {
    CL_SPROUT_SIP_TCP_SERVICE_START_FAIL.log(port,
					     PJUtils::pj_status_to_string(status).c_str());
    LOG_ERROR("Failed to start TCP listener transport for port %d (%s)",
              port,
              PJUtils::pj_status_to_string(status).c_str());
  }

  return status;
}


void destroy_tcp_listener_transport(int port, pjsip_tpfactory *tcp_factory)
{
  LOG_STATUS("Destroyed TCP transport for port %d", port);
  tcp_factory->destroy(tcp_factory);
}


pj_status_t start_transports(int port, pj_str_t& host, pjsip_tpfactory** tcp_factory)
{
  pj_status_t status;

  status = create_udp_transport(port, host);

  if (status != PJ_SUCCESS) {
    return status;
  }

  status = create_tcp_listener_transport(port, host, tcp_factory);

  if (status != PJ_SUCCESS) {
    return status;
  }

  LOG_STATUS("Listening on port %d", port);

  return PJ_SUCCESS;
}


// This class distributes quiescing work within the stack module.  It receives
// requests from the QuiscingManager and ConnectionTracker, and calls the
// relevant methods in the stack module, QuiescingManager and ConnectionManager
// as appropriate.
class StackQuiesceHandler :
  public QuiesceConnectionsInterface,
  public ConnectionsQuiescedInterface
{
public:

  //
  // The following methods are from QuiesceConnectionsInterface.
  //
  void close_untrusted_port()
  {
    // This can only apply to the untrusted P-CSCF port.
    if (stack_data.pcscf_untrusted_tcp_factory != NULL)
    {
      destroy_tcp_listener_transport(stack_data.pcscf_untrusted_port,
                                     stack_data.pcscf_untrusted_tcp_factory);
    }
  }

  void close_trusted_port()
  {
    // This applies to all trusted ports, so the P-CSCF trusted port, or the
    // S-CSCF and I-CSCF ports.
    if (stack_data.pcscf_trusted_tcp_factory != NULL)
    {
      destroy_tcp_listener_transport(stack_data.pcscf_trusted_port,
                                     stack_data.pcscf_trusted_tcp_factory);
    }
    if (stack_data.scscf_tcp_factory != NULL)
    {
      destroy_tcp_listener_transport(stack_data.scscf_port,
                                     stack_data.scscf_tcp_factory);
      CL_SPROUT_S_CSCF_END.log(stack_data.scscf_port);
    }
    if (stack_data.icscf_tcp_factory != NULL)
    {
      destroy_tcp_listener_transport(stack_data.icscf_port,
                                     stack_data.icscf_tcp_factory);
      CL_SPROUT_I_CSCF_END.log(stack_data.icscf_port);
    }
  }

  void open_trusted_port()
  {
    // This applies to all trusted ports, so the P-CSCF trusted port, or the
    // S-CSCF and I-CSCF ports.
    if (stack_data.pcscf_trusted_port != 0)
    {
      create_tcp_listener_transport(stack_data.pcscf_trusted_port,
                                    stack_data.local_host,
                                    &stack_data.pcscf_trusted_tcp_factory);
    }
    if (stack_data.scscf_port != 0)
    {
      create_tcp_listener_transport(stack_data.scscf_port,
                                    stack_data.local_host,
                                    &stack_data.scscf_tcp_factory);
    }
    if (stack_data.icscf_port != 0)
    {
      create_tcp_listener_transport(stack_data.icscf_port,
                                    stack_data.local_host,
                                    &stack_data.icscf_tcp_factory);
    }
  }

  void open_untrusted_port()
  {
    // This can only apply to the untrusted P-CSCF port.
    if (stack_data.pcscf_untrusted_port != 0)
    {
      create_tcp_listener_transport(stack_data.pcscf_untrusted_port,
                                    stack_data.public_host,
                                    &stack_data.pcscf_untrusted_tcp_factory);
    }
  }

  void quiesce()
  {
    connection_tracker->quiesce();
  }

  void unquiesce()
  {
    connection_tracker->unquiesce();
  }

  //
  // The following methods are from ConnectionsQuiescedInterface.
  //
  void connections_quiesced()
  {
    quiescing_mgr->connections_gone();
  }
};


pj_status_t init_pjsip()
{
  pj_status_t status;

  // Must init PJLIB first:
  status = pj_init();
  PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

  // Dump PJLIB config to log file.
  pj_dump_config();

  // Then init PJLIB-UTIL:
  status = pjlib_util_init();
  PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

  // Must create a pool factory before we can allocate any memory.
  pj_caching_pool_init(&stack_data.cp, &pj_pool_factory_default_policy, 0);
  // Create the endpoint.
  status = pjsip_endpt_create(&stack_data.cp.factory, NULL, &stack_data.endpt);
  PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

  // Init transaction layer.
  status = pjsip_tsx_layer_init_module(stack_data.endpt);
  PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

  // Create pool for the application
  stack_data.pool = pj_pool_create(&stack_data.cp.factory,
                                   "sprout-bono",
                                   4000,
                                   4000,
                                   NULL);

  status = register_custom_headers();
  PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

  // Enable deadlock detection on the message queue.
  rx_msg_q.set_deadlock_threshold(MSG_Q_DEADLOCK_TIME);

  return PJ_SUCCESS;
}


pj_status_t init_stack(const std::string& system_name,
                       const std::string& sas_address,
                       int pcscf_trusted_port,
                       int pcscf_untrusted_port,
                       int scscf_port,
                       int icscf_port,
                       const std::string& local_host,
                       const std::string& public_host,
                       const std::string& home_domain,
                       const std::string& additional_home_domains,
                       const std::string& scscf_uri,
                       const std::string& alias_hosts,
                       SIPResolver* sipresolver,
                       int num_pjsip_threads,
                       int num_worker_threads,
                       int record_routing_model,
                       const int default_session_expires,
                       QuiescingManager *quiescing_mgr_arg,
                       LoadMonitor *load_monitor_arg,
                       const std::string& cdf_domain)
{
  pj_status_t status;
  pj_sockaddr pri_addr;
  pj_sockaddr addr_list[16];
  unsigned addr_cnt = PJ_ARRAY_SIZE(addr_list);
  unsigned i;

  // Set up the vectors of threads.  The threads don't get created until
  // start_stack is called.
  pjsip_threads.resize(num_pjsip_threads);
  worker_threads.resize(num_worker_threads);

  // Get ports and host names specified on options.  If local host was not
  // specified, use the host name returned by pj_gethostname.
  char* local_host_cstr = strdup(local_host.c_str());
  char* public_host_cstr = strdup(public_host.c_str());
  char* home_domain_cstr = strdup(home_domain.c_str());
  char* scscf_uri_cstr;
  if (scscf_uri.empty())
  {
    // Create a default S-CSCF URI using the localhost and S-CSCF port.
    std::string tmp_scscf_uri = "sip:" + local_host + ":" + std::to_string(scscf_port) + ";transport=TCP";
    scscf_uri_cstr = strdup(tmp_scscf_uri.c_str());
  }
  else
  {
    // Use the specified URI.
    scscf_uri_cstr = strdup(scscf_uri.c_str());
  }

  // This is only set on Bono nodes (it's the empty string otherwise)
  char* cdf_domain_cstr = strdup(cdf_domain.c_str());

  // Copy port numbers to stack data.
  stack_data.pcscf_trusted_port = pcscf_trusted_port;
  stack_data.pcscf_untrusted_port = pcscf_untrusted_port;
  stack_data.scscf_port = scscf_port;
  stack_data.icscf_port = icscf_port;

  stack_data.sipresolver = sipresolver;

  // Copy other functional options to stack data.
  stack_data.default_session_expires = default_session_expires;

  // Work out local and public hostnames and cluster domain names.
  stack_data.local_host = (local_host != "") ? pj_str(local_host_cstr) : *pj_gethostname();
  stack_data.public_host = (public_host != "") ? pj_str(public_host_cstr) : stack_data.local_host;
  stack_data.default_home_domain = (home_domain != "") ? pj_str(home_domain_cstr) : stack_data.local_host;
  stack_data.scscf_uri = pj_str(scscf_uri_cstr);
  stack_data.cdf_domain = pj_str(cdf_domain_cstr);

  // Build a set of home domains
  stack_data.home_domains = std::unordered_set<std::string>();
  stack_data.home_domains.insert(PJUtils::pj_str_to_string(&stack_data.default_home_domain));
  if (additional_home_domains != "")
  {
    std::list<std::string> domains;
    Utils::split_string(additional_home_domains, ',', domains, 0, true);
    stack_data.home_domains.insert(domains.begin(), domains.end());
  }

  // Set up the default address family.  This is IPv4 unless our local host is an IPv6 address.
  stack_data.addr_family = AF_INET;
  struct in6_addr dummy_addr;
  if (inet_pton(AF_INET6, local_host_cstr, &dummy_addr) == 1)
  {
    LOG_DEBUG("Local host is an IPv6 address - enabling IPv6 mode");
    stack_data.addr_family = AF_INET6;
  }

  stack_data.record_route_on_every_hop = false;
  stack_data.record_route_on_initiation_of_originating = false;
  stack_data.record_route_on_initiation_of_terminating = false;
  stack_data.record_route_on_completion_of_originating = false;
  stack_data.record_route_on_completion_of_terminating = false;
  stack_data.record_route_on_diversion = false;

  if (scscf_port != 0)
  {
    switch (record_routing_model)
    {
    case 1:
      stack_data.record_route_on_initiation_of_originating = true;
      stack_data.record_route_on_completion_of_terminating = true;
      break;
    case 2:
      stack_data.record_route_on_initiation_of_originating = true;
      stack_data.record_route_on_initiation_of_terminating = true;
      stack_data.record_route_on_completion_of_originating = true;
      stack_data.record_route_on_completion_of_terminating = true;
      stack_data.record_route_on_diversion = true;
      break;
    case 3:
      stack_data.record_route_on_every_hop = true;
      stack_data.record_route_on_initiation_of_originating = true;
      stack_data.record_route_on_initiation_of_terminating = true;
      stack_data.record_route_on_completion_of_originating = true;
      stack_data.record_route_on_completion_of_terminating = true;
      stack_data.record_route_on_diversion = true;
      break;
    default:
      LOG_ERROR("Record-Route setting should be 1, 2, or 3, is %d. Defaulting to Record-Route on every hop.", record_routing_model);
      stack_data.record_route_on_every_hop = true;
    }
  }

  std::string system_name_sas = system_name;
  std::string system_type_sas = (pcscf_trusted_port != 0) ? "bono" : "sprout";
  // Initialize SAS logging.
  if (system_name_sas == "")
  {
    system_name_sas = std::string(stack_data.local_host.ptr, stack_data.local_host.slen);
  }
  SAS::init(system_name,
            system_type_sas,
            SASEvent::CURRENT_RESOURCE_BUNDLE,
            sas_address,
            sas_write);

  // Initialise PJSIP and all the associated resources.
  status = init_pjsip();

  // Register the stack module.
  pjsip_endpt_register_module(stack_data.endpt, &mod_stack);
  stack_data.module_id = mod_stack.id;

  // Initialize the PJUtils module.
  PJUtils::init();

  // Create listening transports for the ports whichtrusted and untrusted ports.
  stack_data.pcscf_trusted_tcp_factory = NULL;
  if (stack_data.pcscf_trusted_port != 0)
  {
    status = start_transports(stack_data.pcscf_trusted_port,
                              stack_data.local_host,
                              &stack_data.pcscf_trusted_tcp_factory);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
  }

  stack_data.pcscf_untrusted_tcp_factory = NULL;
  if (stack_data.pcscf_untrusted_port != 0)
  {
    status = start_transports(stack_data.pcscf_untrusted_port,
                              stack_data.public_host,
                              &stack_data.pcscf_untrusted_tcp_factory);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
  }

  stack_data.scscf_tcp_factory = NULL;
  if (stack_data.scscf_port != 0)
  {
    status = start_transports(stack_data.scscf_port,
                              stack_data.public_host,
                              &stack_data.scscf_tcp_factory);
    if (status == PJ_SUCCESS)
    {
      CL_SPROUT_S_CSCF_AVAIL.log(stack_data.scscf_port);
    }
    else
    {
      CL_SPROUT_S_CSCF_INIT_FAIL2.log(stack_data.scscf_port)
;
    }
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
  }

  stack_data.icscf_tcp_factory = NULL;
  if (stack_data.icscf_port != 0)
  {
    status = start_transports(stack_data.icscf_port,
                              stack_data.public_host,
                              &stack_data.icscf_tcp_factory);
    if (status == PJ_SUCCESS)
    {
      CL_SPROUT_I_CSCF_AVAIL.log(stack_data.icscf_port);
    }
    else
    {
      CL_SPROUT_I_CSCF_INIT_FAIL2.log(stack_data.icscf_port)
;
    }
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
  }

  // List all names matching local endpoint.
  // Note that PJLIB version 0.6 and newer has a function to
  // enumerate local IP interface (pj_enum_ip_interface()), so
  // by using it would be possible to list all IP interfaces in
  // this host.

  // The first address is important since this would be the one
  // to be added in Record-Route.
  stack_data.name[stack_data.name_cnt] = stack_data.local_host;
  stack_data.name_cnt++;

  if (strcmp(local_host_cstr, public_host_cstr))
  {
    stack_data.name[stack_data.name_cnt] = stack_data.public_host;
    stack_data.name_cnt++;
  }

  if ((scscf_port != 0) &&
      (!scscf_uri.empty()))
  {
    // S-CSCF enabled with a specified URI, so add host name from the URI to hostnames.
    pjsip_sip_uri* uri = (pjsip_sip_uri*)PJUtils::uri_from_string(scscf_uri,
                                                                  stack_data.pool);
    if (uri != NULL)
    {
      stack_data.name[stack_data.name_cnt] = uri->host;
      stack_data.name_cnt++;
    }
  }

  if (pj_gethostip(pj_AF_INET(), &pri_addr) == PJ_SUCCESS)
  {
    pj_strdup2(stack_data.pool, &stack_data.name[stack_data.name_cnt],
               pj_inet_ntoa(pri_addr.ipv4.sin_addr));
    stack_data.name_cnt++;
  }

  // Get the rest of IP interfaces.
  if (pj_enum_ip_interface(pj_AF_INET(), &addr_cnt, addr_list) == PJ_SUCCESS)
  {
    for (i = 0; i < addr_cnt; ++i)
    {
      if (addr_list[i].ipv4.sin_addr.s_addr == pri_addr.ipv4.sin_addr.s_addr)
      {
        continue;
      }

      pj_strdup2(stack_data.pool, &stack_data.name[stack_data.name_cnt],
                 pj_inet_ntoa(addr_list[i].ipv4.sin_addr));
      stack_data.name_cnt++;
    }
  }

  // Note that we no longer consider 127.0.0.1 and localhost as aliases.

  // Parse the list of alias host names.
  stack_data.aliases = std::unordered_set<std::string>();
  if (alias_hosts != "")
  {
    std::list<std::string> aliases;
    Utils::split_string(alias_hosts, ',', aliases, 0, true);
    stack_data.aliases.insert(aliases.begin(), aliases.end());
    for (std::unordered_set<std::string>::iterator it = stack_data.aliases.begin();
         it != stack_data.aliases.end();
         ++it)
    {
      pj_strdup2(stack_data.pool, &stack_data.name[stack_data.name_cnt], it->c_str());
      stack_data.name_cnt++;
    }
  }

  LOG_STATUS("Local host aliases:");
  for (i = 0; i < stack_data.name_cnt; ++i)
  {
    LOG_STATUS(" %.*s",
               (int)stack_data.name[i].slen,
               stack_data.name[i].ptr);
  }

  // Set up the Last Value Cache, accumulators and counters.
  std::string zmq_port = SPROUT_ZMQ_PORT;

  if ((stack_data.pcscf_trusted_port != 0) &&
      (stack_data.pcscf_untrusted_port != 0))
  {
    zmq_port = BONO_ZMQ_PORT;
  }

  stack_data.stats_aggregator = new LastValueCache(num_known_stats,
                                                   known_statnames,
                                                   zmq_port);

  latency_accumulator = new StatisticAccumulator("latency_us",
                                                 stack_data.stats_aggregator);
  queue_size_accumulator = new StatisticAccumulator("queue_size",
                                                    stack_data.stats_aggregator);
  requests_counter = new StatisticCounter("incoming_requests",
                                          stack_data.stats_aggregator);
  overload_counter = new StatisticCounter("rejected_overload",
                                          stack_data.stats_aggregator);

  if (load_monitor_arg != NULL)
  {
    load_monitor = load_monitor_arg;
  }

  if (quiescing_mgr_arg != NULL)
  {
    quiescing_mgr = quiescing_mgr_arg;

    // Create an instance of the stack quiesce handler. This acts as a glue
    // class between the stack modulem connections tracker, and the quiescing
    // manager.
    stack_quiesce_handler = new StackQuiesceHandler();

    // Create a new connection tracker, and register the quiesce handler with
    // it.
    connection_tracker = new ConnectionTracker(stack_quiesce_handler);

    // Register the quiesce handler with the quiescing manager (the former
    // implements the connection handling interface).
    quiescing_mgr->register_conns_handler(stack_quiesce_handler);
  }

  return status;
}


pj_status_t start_stack()
{
  pj_status_t status = PJ_SUCCESS;

  quit_flag = PJ_FALSE;

  // Create worker threads first as they take work from the PJSIP threads so
  // need to be ready.
  for (size_t ii = 0; ii < worker_threads.size(); ++ii)
  {
    pj_thread_t* thread;
    status = pj_thread_create(stack_data.pool, "worker", &worker_thread,
                              NULL, 0, 0, &thread);
    if (status != PJ_SUCCESS)
    {
      LOG_ERROR("Error creating worker thread, %s",
                PJUtils::pj_status_to_string(status).c_str());
      return 1;
    }
    worker_threads[ii] = thread;
  }

  // Now create the PJSIP threads.
  for (size_t ii = 0; ii < pjsip_threads.size(); ++ii)
  {
    pj_thread_t* thread;
    status = pj_thread_create(stack_data.pool, "pjsip", &pjsip_thread,
                              NULL, 0, 0, &thread);
    if (status != PJ_SUCCESS)
    {
      LOG_ERROR("Error creating PJSIP thread, %s",
                PJUtils::pj_status_to_string(status).c_str());
      return 1;
    }
    pjsip_threads[ii] = thread;
  }

  return status;
}

void stop_stack()
{
  // Terminate the PJSIP threads and the worker threads to exit.  We kill
  // the PJSIP threads first - if we killed the worker threads first the
  // rx_msg_q will stop getting serviced so could fill up blocking
  // PJSIP threads, causing a deadlock.

  // Set the quit flag to signal the PJSIP threads to exit, then wait
  // for them to exit.
  quit_flag = PJ_TRUE;

  for (std::vector<pj_thread_t*>::iterator i = pjsip_threads.begin();
       i != pjsip_threads.end();
       ++i)
  {
    pj_thread_join(*i);
  }

  // Now it is safe to signal the worker threads to exit via the queue and to
  // wait for them to terminate.
  rx_msg_q.terminate();
  for (std::vector<pj_thread_t*>::iterator i = worker_threads.begin();
       i != worker_threads.end();
       ++i)
  {
    pj_thread_join(*i);
  }
}


// Unregister all modules registered by the stack.  In particular, unregister
// the transaction layer module, which terminates all transactions.
void unregister_stack_modules(void)
{
  PJUtils::term();
  pjsip_tsx_layer_destroy();
  pjsip_endpt_unregister_module(stack_data.endpt, &mod_stack);
}


void term_pjsip()
{
  pjsip_endpt_destroy(stack_data.endpt);
  pj_pool_release(stack_data.pool);
  pj_caching_pool_destroy(&stack_data.cp);
  pj_shutdown();
}


// Destroy stack
void destroy_stack(void)
{
  // Tear down the stack.
  delete latency_accumulator;
  latency_accumulator = NULL;
  delete queue_size_accumulator;
  queue_size_accumulator = NULL;
  delete requests_counter;
  requests_counter = NULL;
  delete overload_counter;
  overload_counter = NULL;
  delete stack_data.stats_aggregator;

  delete stack_quiesce_handler;
  stack_quiesce_handler = NULL;

  delete connection_tracker;
  connection_tracker = NULL;

  pjsip_threads.clear();
  worker_threads.clear();

  SAS::term();

  // Terminate PJSIP.
  term_pjsip();
}
