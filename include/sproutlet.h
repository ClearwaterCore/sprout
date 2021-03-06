/**
 * @file sproutlet.h  Abstract Sproutlet API definition
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SPROUTLET_H__
#define SPROUTLET_H__

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdint.h>
}

#include <list>
#include "sas.h"
#include "snmp_success_fail_count_by_request_type_table.h"

#define API_VERSION 1

class SproutletHelper;
class SproutletTsxHelper;
class Sproutlet;
class SproutletTsx;
class SproutletProxy;


/// Typedefs for Sproutlet-specific types
typedef intptr_t TimerID;

typedef enum {NONE, TIMEOUT, TRANSPORT_ERROR} ForkErrorState;

struct ForkState
{
  pjsip_tsx_state_e tsx_state;
  ForkErrorState error_state;
};

/// The SproutletTsxHelper class handles the underlying service-related processing of
/// a single transaction.  Once a service has been triggered as part of handling
/// a transaction, the related SproutletTsxHelper is inspected to determine what should
/// be done next, e.g. forward the request, reject it, fork it etc.
///
/// This is an abstract base class to allow for alternative implementations -
/// in particular, production and test.  It is implemented by the underlying
/// service infrastructure, not by the services themselves.
///
class SproutletTsxHelper
{
public:
  /// Virtual destructor.
  virtual ~SproutletTsxHelper() {}

  /// Returns a mutable clone of the original request.  This can be modified
  /// and sent by the Sproutlet using the send_request call.
  ///
  /// @returns             - A clone of the original request message.
  ///
  virtual pjsip_msg* original_request() = 0;

  /// Sets the transport on the given message to be the same as on the
  /// original incoming request.
  ///
  /// @param  req          - The request message on which to set the
  //                         transport.
  virtual void copy_original_transport(pjsip_msg* req) = 0;

  /// Returns the top Route header from the original incoming request.  This
  /// can be inpsected by the Sproutlet, but should not be modified.  Note that
  /// this Route header is removed from the request passed to the Sproutlet on
  /// the on_rx_*_request calls.
  ///
  /// @returns             - A pointer to a read-only copy of the top Route
  ///                        header from the received request.
  ///
  virtual const pjsip_route_hdr* route_hdr() const = 0;

  /// Returns a URI that could be used to route back to the current Sproutlet.
  /// This URI may contain pre-loaded parameters that should not be modified
  /// by the calling code (or the URI may cease to route as expected).
  ///
  /// @returns             - The SIP URI.
  /// @param  pool         - A pool to allocate the URI from.
  virtual pjsip_sip_uri* get_reflexive_uri(pj_pool_t* pool) const = 0;

  /// Check if a given URI would be routed to the current Sproutlet if it was
  /// recieved as the top Route header on a request.  This can be used to
  /// locate a Sproutlet in a Route set.
  ///
  /// If the URI is not a SIP URI, this function returns FALSE.
  ///
  /// @returns             - Whether the URI is reflexive.
  /// @param  uri          - The URI to check.
  virtual bool is_uri_reflexive(const pjsip_uri* uri) const = 0;

  /// Creates a new, blank request.  This is typically used when creating
  /// a downstream request to another SIP server as part of handling a
  /// request.
  ///
  /// @returns             - A new, blank request message.
  ///
  virtual pjsip_msg* create_request() = 0;

  /// Clones the request.  This is typically used when forking a request if
  /// different request modifications are required on each fork or for storing
  /// off to handle late forking.
  ///
  /// @returns             - The cloned request message.
  /// @param  req          - The request message to clone.
  ///
  virtual pjsip_msg* clone_request(pjsip_msg* req) = 0;

  /// Clones the message.  This is typically used when we want to keep a
  /// message after calling a mutating method on it.
  ///
  /// @returns             - The cloned message.
  /// @param  msg          - The message to clone.
  ///
  virtual pjsip_msg* clone_msg(pjsip_msg* msg) = 0;

  /// Create a response from a given request, this response can be passed to
  /// send_response or stored for later.  It may be freed again by passing
  /// it to free_message.
  ///
  /// @returns             - The new response message.
  /// @param  req          - The request to build a response for.
  /// @param  status_code  - The SIP status code for the response.
  /// @param  status_text  - The text part of the status line.
  ///
  virtual pjsip_msg* create_response(pjsip_msg* req,
                                     pjsip_status_code status_code,
                                     const std::string& status_text="") = 0;

  /// Indicate that the request should be forwarded following standard routing
  /// rules.
  ///
  /// This function may be called repeatedly to create downstream forks of an
  /// original upstream request and may also be called during response processing
  /// or an original request to create a late fork.  When processing an in-dialog
  /// request this function may only be called once.
  ///
  /// This function may be called while processing initial requests,
  /// in-dialog requests and cancels but not during response handling
  ///
  /// @param  req          - The request message to use for forwarding.  If NULL
  ///                        the original request message is used.
  ///
  virtual int send_request(pjsip_msg*& req) = 0;

  /// Indicate that the response should be forwarded following standard routing
  /// rules.  Note that, if this service created multiple forks, the responses
  /// will be aggregated before being sent downstream.
  ///
  /// This function may be called while handling any response.
  ///
  /// @param  rsp          - The response message to use for forwarding.
  ///
  virtual void send_response(pjsip_msg*& rsp) = 0;

  /// Cancels a forked request.  For INVITE requests, this causes a CANCEL
  /// to be sent, so the Sproutlet must wait for the final response.  For
  /// non-INVITE requests the fork is terminated immediately.
  ///
  /// @param fork_id       - The identifier of the fork to cancel.
  ///
  virtual void cancel_fork(int fork_id, int reason=0) = 0;

  /// Cancels all pending forked requests by either sending a CANCEL request
  /// (for INVITE requests) or terminating the transaction (for non-INVITE
  /// requests).
  ///
  virtual void cancel_pending_forks(int reason=0) = 0;

  /// Returns the current status of a downstream fork, including the
  /// transaction state and whether a timeout or transport error has been
  /// detected on the fork.
  ///
  /// @returns             - ForkState structure containing transaction and
  ///                        error status for the fork.
  /// @param  fork_id      - The identifier of the fork.
  ///
  virtual const ForkState& fork_state(int fork_id) = 0;

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into SproutletTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  ///
  virtual void free_msg(pjsip_msg*& msg) = 0;

  /// Returns the pool corresponding to a message.  This pool can then be used
  /// to allocate further headers or bodies to add to the message.
  ///
  /// @returns             - The pool corresponding to this message.
  /// @param  msg          - The message.
  ///
  virtual pj_pool_t* get_pool(const pjsip_msg* msg) = 0;

  /// Returns a brief one line summary of the message.
  ///
  /// @returns             - Message information
  /// @param  msg          - The message
  ///
  virtual const char* msg_info(pjsip_msg* msg) = 0;

  /// Schedules a timer with the specified identifier and expiry period.
  /// The on_timer_expiry callback will be called back with the timer identity
  /// and context parameter when the timer expires.  If the identifier
  /// corresponds to a timer that is already running, the timer will be stopped
  /// and restarted with the new duration and context parameter.
  ///
  /// @returns             - true/false indicating when the timer is programmed.
  /// @param  context      - Context parameter returned on the callback.
  /// @param  id           - The unique identifier for the timer.
  /// @param  duration     - Timer duration in milliseconds.
  ///
  virtual bool schedule_timer(void* context, TimerID& id, int duration) = 0;

  /// Cancels the timer with the specified identifier.  This is a no-op if
  /// there is no timer with this identifier running.
  ///
  /// @param  id           - The unique identifier for the timer.
  ///
  virtual void cancel_timer(TimerID id) = 0;

  /// Queries the state of a timer.
  ///
  /// @returns             - true if the timer is running, false otherwise.
  /// @param  id           - The unique identifier for the timer.
  ///
  virtual bool timer_running(TimerID id) = 0;

  /// Returns the SAS trail identifier that should be used for any SAS events
  /// related to this service invocation.
  ///
  virtual SAS::TrailId trail() const = 0;

  /// Get a URI that routes to the given named service.
  ///
  /// @returns            - The new URI.
  ///
  /// @param service      - Name of the service to route to.
  /// @param route        - An existing Route to use as a base for the new one.
  ///                       Parameters from this URI will be preserved if
  ///                       possible.
  /// @param pool         - Pool to allocate the URI in.
  /// Constructs the next URI for the Sproutlet
  virtual pjsip_sip_uri* next_hop_uri(const std::string& service,
                                      const pjsip_route_hdr* route,
                                      pj_pool_t* pool) const = 0;
};


/// The SproutletTsx class is an abstract base class used to handle the
/// application-server-specific processing of a single transaction.  It
/// is provided with a SproutletTsxHelper, which it may use to perform the
/// underlying service-related processing.
///
class SproutletTsx
{
public:
  /// Constructor.
  ///
  /// @param sproutlet     - The parent sproutlet.
  SproutletTsx(Sproutlet* sproutlet) :
    _sproutlet(sproutlet),
    _helper(NULL)
  {
  }

  /// Virtual destructor.
  virtual ~SproutletTsx() {}

  /// Set the SproutletTsxHelper on the SproutletTsx.
  ///
  /// @param  helper       - The sproutlet helper.
  virtual void set_helper(SproutletTsxHelper* helper) { _helper = helper; }

  /// Called when an initial request (dialog-initiating or out-of-dialog) is
  /// received for the transaction.
  ///
  /// During this function, exactly one of the following functions must be called,
  /// otherwise the request will be rejected with a 503 Server Internal
  /// Error:
  ///
  /// * forward_request() - May be called multiple times
  /// * reject()
  /// * defer_request()
  ///
  /// @param req           - The received initial request.
  virtual void on_rx_initial_request(pjsip_msg* req) { send_request(req); }

  /// Called when an in-dialog request is received for the transaction.
  ///
  /// During this function, exactly one of the following functions must be called,
  /// otherwise the request will be rejected with a 503 Server Internal
  /// Error:
  ///
  /// * forward_request()
  /// * reject()
  /// * defer_request()
  ///
  /// @param req           - The received in-dialog request.
  virtual void on_rx_in_dialog_request(pjsip_msg* req) { send_request(req); }

  /// Called when a request has been transmitted on the transaction (usually
  /// because the service has previously called forward_request() with the
  /// request message.
  ///
  /// @param req           - The transmitted request
  /// @param fork_id       - The identity of the downstream fork on which the
  ///                        request was sent.
  virtual void on_tx_request(pjsip_msg* req, int fork_id) { }

  /// Called with all responses received on the transaction.  If a transport
  /// error or transaction timeout occurs on a downstream leg, this method is
  /// called with a 408 response.
  ///
  /// @param  rsp          - The received request.
  /// @param  fork_id      - The identity of the downstream fork on which
  ///                        the response was received.
  virtual void on_rx_response(pjsip_msg* rsp, int fork_id) { send_response(rsp); }

  /// Called when a response has been transmitted on the transaction.
  ///
  /// @param  rsp          - The transmitted response.
  virtual void on_tx_response(pjsip_msg* rsp) { }

  /// Called if the original request is cancelled (either by a received
  /// CANCEL request, an error on the inbound transport or a transaction
  /// timeout).  On return from this method the transaction (and any remaining
  /// downstream legs) will be cancelled automatically.  No further methods
  /// will be called for this transaction.
  ///
  /// @param  status_code  - Indicates the reason for the cancellation
  ///                        (487 for a CANCEL, 408 for a transport error
  ///                        or transaction timeout)
  /// @param  cancel_req   - The received CANCEL request or NULL if cancellation
  ///                        was triggered by an error or timeout.
  virtual void on_rx_cancel(int status_code, pjsip_msg* cancel_req) {}

  /// Called when a timer programmed by the SproutletTsx expires.
  ///
  /// @param  context      - The context parameter specified when the timer
  ///                        was scheduled.
  virtual void on_timer_expiry(void* context) {}

protected:

  /// Returns a mutable clone of the original request.  This can be modified
  /// and sent by the Sproutlet using the send_request call.
  ///
  /// @returns             - A clone of the original request message.
  ///
  pjsip_msg* original_request()
    {return _helper->original_request();}

  /// Sets the transport on this request to be the same as on the original.
  ///
  /// @param  req          - The request message on which to set the
  ///                        transport.
  void copy_original_transport(pjsip_msg* req)
    {_helper->copy_original_transport(req);}

  /// Returns a URI that could be used to route back to the current Sproutlet.
  /// This URI may contain pre-loaded parameters that should not be modified
  /// by the calling code (or the URI may cease to route as expected).
  ///
  /// @returns             - The SIP URI.
  /// @param  pool         - A pool to allocate the URI from.
  pjsip_sip_uri* get_reflexive_uri(pj_pool_t* pool)
    {return _helper->get_reflexive_uri(pool);}

  /// Check if a given URI would be routed to the current Sproutlet if it was
  /// recieved as the top Route header on a request.  This can be used to
  /// locate a Sproutlet in a Route set.
  ///
  /// If the URI is not a SIP URI, this function returns FALSE.
  ///
  /// @returns             - Whether the URI is reflexive.
  /// @param  uri          - The URI to check.
  bool is_uri_reflexive(const pjsip_uri* uri)
    {return _helper->is_uri_reflexive(uri);}

  /// Returns the top Route header from the original incoming request.  This
  /// can be inpsected by the Sproutlet, but should not be modified.  Note that
  /// this Route header is removed from the request passed to the Sproutlet on
  /// the on_rx_*_request calls.
  ///
  /// @returns             - A pointer to a read-only copy of the top Route
  ///                        header from the received request.
  ///
  const pjsip_route_hdr* route_hdr() const
    {return _helper->route_hdr();}

  /// Creates a new, blank request.  This is typically used when creating
  /// a downstream request to another SIP server as part of handling a
  /// request.
  ///
  /// @returns             - A new, blank request message.
  ///
  pjsip_msg* create_request()
    {return _helper->create_request();}

  /// Clones the request.  This is typically used when forking a request if
  /// different request modifications are required on each fork.
  ///
  /// WARNING: This method is DEPRECATED and only exists for backwards
  ///          compatibilty.
  ///
  /// @returns             - The cloned request message.
  /// @param  req          - The request message to clone.
  ///
  pjsip_msg* clone_request(pjsip_msg* req)
    {return _helper->clone_request(req);}

  /// Clones the message.  This is typically used when we want to keep a
  /// message after calling a mutative method on it.
  ///
  /// @returns             - The cloned message.
  /// @param  msg          - The message to clone.
  ///
  pjsip_msg* clone_msg(pjsip_msg* msg)
    {return _helper->clone_msg(msg);}

  /// Create a response from a given request, this response can be passed to
  /// send_response or stored for later.  It may be freed again by passing
  /// it to free_message.
  ///
  /// @returns             - The new response message.
  /// @param  req          - The request to build a response for.
  /// @param  status_code  - The SIP status code for the response.
  /// @param  status_text  - The text part of the status line.
  ///
  virtual pjsip_msg* create_response(pjsip_msg* req,
                                     pjsip_status_code status_code,
                                     const std::string& status_text="")
    {return _helper->create_response(req, status_code, status_text);}

  /// Indicate that the request should be forwarded following standard routing
  /// rules.  Note that, even if other Route headers are added by this AS, the
  /// request will be routed back to the S-CSCF that sent the request in the
  /// first place after all those routes have been visited.
  ///
  /// This function may be called repeatedly to create downstream forks of an
  /// original upstream request and may also be called during response processing
  /// or an original request to create a late fork.  When processing an in-dialog
  /// request this function may only be called once.
  ///
  /// This function may be called while processing initial requests,
  /// in-dialog requests and cancels but not during response handling.
  ///
  /// @returns             - The ID of this forwarded request
  /// @param  req          - The request message to use for forwarding.
  ///
  int send_request(pjsip_msg*& req)
    {return _helper->send_request(req);}

  /// Indicate that the response should be forwarded following standard routing
  /// rules.  Note that, if this service created multiple forks, the responses
  /// will be aggregated before being sent downstream.
  ///
  /// This function may be called while handling any response.
  ///
  /// @param  rsp          - The response message to use for forwarding.
  ///
  void send_response(pjsip_msg*& rsp)
    {_helper->send_response(rsp);}

  /// Cancels a forked request.  For INVITE requests, this causes a CANCEL
  /// to be sent, so the Sproutlet must wait for the final response.  For
  /// non-INVITE requests the fork is terminated immediately.
  ///
  /// @param fork_id       - The identifier of the fork to cancel.
  ///
  void cancel_fork(int fork_id, int reason=0)
    {_helper->cancel_fork(fork_id, reason);}

  /// Cancels all pending forked requests by either sending a CANCEL request
  /// (for INVITE requests) or terminating the transaction (for non-INVITE
  /// requests).
  ///
  void cancel_pending_forks(int reason=0)
    {_helper->cancel_pending_forks(reason);}

  /// Returns the current status of a downstream fork, including the
  /// transaction state and whether a timeout or transport error has been
  /// detected on the fork.
  ///
  /// @returns             - ForkState structure containing transaction and
  ///                        error status for the fork.
  /// @param  fork_id      - The identifier of the fork.
  ///
  const ForkState& fork_state(int fork_id)
    {return _helper->fork_state(fork_id);}

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into SproutletTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  ///
  void free_msg(pjsip_msg*& msg)
    {return _helper->free_msg(msg);}

  /// Returns the pool corresponding to a message.  This pool can then be used
  /// to allocate further headers or bodies to add to the message.
  ///
  /// @returns             - The pool corresponding to this message.
  /// @param  msg          - The message.
  ///
  pj_pool_t* get_pool(const pjsip_msg* msg)
    {return _helper->get_pool(msg);}

  /// Returns a brief one line summary of the message.
  ///
  /// @returns             - Message information
  /// @param  msg          - The message
  ///
  const char* msg_info(pjsip_msg* msg)
    {return _helper->msg_info(msg);}

  /// Schedules a timer with the specified identifier and expiry period.
  /// The on_timer_expiry callback will be called back with the timer identity
  /// and context parameter when the timer expires.  If the identifier
  /// corresponds to a timer that is already running, the timer will be stopped
  /// and restarted with the new duration and context parameter.
  ///
  /// @returns             - true/false indicating when the timer is programmed.
  /// @param  context      - Context parameter returned on the callback.
  /// @param  id           - A unique identifier for the timer.
  /// @param  duration     - Timer duration in milliseconds.
  ///
  bool schedule_timer(void* context, TimerID& id, int duration)
    {return _helper->schedule_timer(context, id, duration);}

  /// Cancels the timer with the specified identifier.  This is a no-op if
  /// there is no timer with this identifier running.
  ///
  /// @param  id           - The unique identifier for the timer.
  ///
  void cancel_timer(TimerID id)
    {_helper->cancel_timer(id);}

  /// Queries the state of a timer.
  ///
  /// @returns             - true if the timer is running, false otherwise.
  /// @param  id           - The unique identifier for the timer.
  ///
  bool timer_running(TimerID id)
    {return _helper->timer_running(id);}

  /// Returns the SAS trail identifier that should be used for any SAS events
  /// related to this service invocation.
  ///
  SAS::TrailId trail() const
    {return _helper->trail();}

  /// Get a URI that routes to the given named service.
  ///
  /// @returns            - The new URI.
  ///
  /// @param service      - Name of the service to route to.
  /// @param route        - An existing Route to use as a base for the new one.
  ///                       Parameters from this URI will be preserved if
  ///                       possible, but the user part will be stripped.
  /// @param pool         - Pool to allocate the URI in.
  pjsip_sip_uri* next_hop_uri(const std::string& service,
                              const pjsip_route_hdr* route,
                              pj_pool_t* pool) const
  {
    return _helper->next_hop_uri(service, route, pool);
  }

private:
  /// Parent sproutlet object.
  Sproutlet* _sproutlet;

  /// Transaction helper to use for underlying service-related processing.
  SproutletTsxHelper* _helper;

  friend class SproutletProxy;
};


/// An abstract base class that handles service-related processing for a
/// Sproutlet.
class SproutletHelper
{
public:
  /// Virtual descrustor.
  virtual ~SproutletHelper() {}

  /// Constructs the next URI for the Sproutlet that doesn't want to handle a
  /// request.
  virtual pjsip_sip_uri* next_hop_uri(const std::string& service,
                                      const pjsip_route_hdr* route,
                                      pj_pool_t* pool) const = 0;

  /// Check if a given URI would be routed to the current Sproutlet if it was
  /// recieved as the top Route header on a request.  This can be used to
  /// locate a Sproutlet in a Route set.
  ///
  /// If the URI is not a SIP URI, this function returns FALSE.
  virtual bool is_uri_reflexive(const pjsip_uri* uri,
                                Sproutlet* sproutlet,
                                SAS::TrailId trail) = 0;
};


/// The Sproutlet class is a base class on which SIP services can be
/// built.
///
/// Derived classes are instantiated during system initialization and
/// register a service name with Sprout.  Sprout calls the create_tsx method
/// on an Sproutlet derived class when the ServiceManager determines that
/// the next hop for a request contains a hostname of the form
/// &lt;service_name&gt;.&lt;homedomain&gt;.  This may happen if:
///
/// -  an initial request is received with a top route header/ReqURI indicating
///    this service.
/// -  an initial request has been forwarded by some earlier service instance
///    to this service.
/// -  an in-dialog request is received for a dialog on which the service
///    previously called add_to_dialog.
///
class Sproutlet
{
public:
  /// Virtual destructor.
  virtual ~Sproutlet() {}

  SNMP::SuccessFailCountByRequestTypeTable* _incoming_sip_transactions_tbl;
  SNMP::SuccessFailCountByRequestTypeTable* _outgoing_sip_transactions_tbl;

  /// Called when the system determines the service should be invoked for a
  /// received request.  The Sproutlet can either return NULL indicating it
  /// does not want to process the request, or create a suitable object
  /// derived from the SproutletTsx class to process the request.
  ///
  /// @param  proxy         - The Sproutlet proxy.
  /// @param  alias         - The alias of this Sproutlet that matched the
  ///                         incoming request.
  /// @param  req           - The received request message.
  /// @param  next_hop      - The Sproutlet can use this field to specify a
  ///                         next hop URI when it returns a NULL Tsx. Filling
  ///                         in this field is optional.
  /// @param  pool          - The pool for creating the next_hop uri.
  /// @param  trail         - The SAS trail id for the message.
  virtual SproutletTsx* get_tsx(SproutletHelper* proxy,
                                const std::string& alias,
                                pjsip_msg* req,
                                pjsip_sip_uri*& next_hop,
                                pj_pool_t* pool,
                                SAS::TrailId trail) = 0;

  /// Returns the name of this service.
  const std::string service_name() const { return _service_name; }

  /// Returns the URI of this service (as a string)
  const std::string uri_as_str() const { return _uri; }

  /// Returns the API version required by this Sproutlet.
  int api_version() const { return API_VERSION; }

  /// Returns the default port for this service.
  int port() const { return _port; }

  /// Returns the host name of this service.
  const std::string service_host() const { return _service_host; }

  /// Returns the aliases of this service.
  virtual const std::list<std::string> aliases() const
    { return std::list<std::string>(); }

protected:
  /// Constructor.
  Sproutlet(const std::string& service_name,
            int port,
            const std::string& uri,
            const std::string& service_host="",
            SNMP::SuccessFailCountByRequestTypeTable* incoming_sip_transactions_tbl = NULL,
            SNMP::SuccessFailCountByRequestTypeTable* outgoing_sip_transactions_tbl = NULL) :
    _incoming_sip_transactions_tbl(incoming_sip_transactions_tbl),
    _outgoing_sip_transactions_tbl(outgoing_sip_transactions_tbl),
    _service_name(service_name),
    _port(port),
    _uri(uri),
    _service_host(service_host)
  {
  }

private:
  /// The name of this service.
  const std::string _service_name;

  /// The default port for this service (0 if no default).
  const int _port;

  /// The URI of this service.
  const std::string _uri;

  /// The host name of this service.
  const std::string _service_host;
};

#endif
