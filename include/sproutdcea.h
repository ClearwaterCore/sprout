/**
 * @file sproutdcea.h  Sprout Craft Log declarations.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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

#ifndef _SPROUTDCEA_H__
#define _SPROUTDCEA_H__

#include <string>
#include "craft_dcea.h"

// Sprout syslog identities
/**********************************************************
/ log_id
/ severity
/ Description: (formatted)
/ Cause: 
/ Effect:
/ Action:
**********************************************************/
static const PDLog1<const char*> CL_SPROUT_INVALID_S_CSCF_PORT
  (
   PDLogBase::CL_SPROUT_ID + 1,
   PDLOG_ERR,
   "The S-CSCF port specified in /etc/clearwater/config must be in a range from 1 to 65535 but has a value of %s",
   "The scscf=<port> port value is outside the permitted range",
   "The sprout application will exit.",
   1,
   "Correct the port value.  Typically this is set to 5054."
   );
static const PDLog1<const char*> CL_SPROUT_INVALID_I_CSCF_PORT
  (
   PDLogBase::CL_SPROUT_ID + 2,
   PDLOG_ERR,
   "Fatal - The I-CSCF port specified in /etc/clearwater/config must be in a range from 1 to 65535 but has a value of %s",
   "The icscf=<port> value is outside the permitted range",
   "The sprout application will exit",
   1,
   "Correct the port value.  Typically this is set to 5052"
   );
static const PDLog CL_SPROUT_INVALID_SAS_OPTION 
  (
   PDLogBase::CL_SPROUT_ID + 3,
   PDLOG_ERR,
   "The sas_server option in /etc/clearwater/config is invalid or not configured",
   "The interface to the SAS is not specified.",
   "No call traces will appear in the sas",
   2,
   "Set the fully qualified sas hostname for the sas_server=<host> option.",
   "Example: sas_server=sas-1.os3.richlab.datcon.co.uk.  The Sprout application must be restarted to take effect."
   );
static const PDLog1<const char*> CL_SPROUT_CRASH
  (
   PDLogBase::CL_SPROUT_ID + 4,
   PDLOG_ERR,
   "Fatal - Sprout has exited or crashed with signal %s",
   "Sprout has encountered a fatal software error or has been terminated",
   "The Sprout application will restart.",
   4,
   "This error can occur if Sprout has been terminated by operator command.",
   "Check the craft log to see if Monit has reported a sprout timeout.  This would be reported as a 'poll_sprout' failed.  Monit will restart sprout for this case.",
   "Actual crashes such as abort, segment trap, bus error trap, should be reported as a problem. "
   );
static const PDLog CL_SPROUT_STARTED
  (
   PDLogBase::CL_SPROUT_ID + 5,
   PDLOG_ERR,
   "Sprout started",
   "The Sprout application is starting.",
   "Normal",
   1,
   "None"
   );
static const PDLog CL_SPROUT_NO_PSI_CSCF
  (
   PDLogBase::CL_SPROUT_ID + 6,
   PDLOG_ERR,
   "Fatal - Must enable P-CSCF, S-CSCF or I-CSCF in /etc/clearwater/config",
   "Neither a P-CSCF, S-CSCF, nor an I-CSCF was configured in the /etc/clearwater/config",
   "The Sprout application will exit until the problem is fixed.",
   3,
   "The P-CSCF is configured by setting the pcscf=<port> option.",
   "The S-CSCF is configured by setting the scfcf=<port> option.",
   "The I-CSCF is configured by setting the icscf=<port> option."
   );
static const PDLog CL_SPROUT_SI_CSCF_NO_HOMESTEAD
  (
   PDLogBase::CL_SPROUT_ID + 7,
   PDLOG_ERR,
   "Fatal - S/I-CSCF enabled with no Homestead server specified in /etc/clearwater/config",
   "The S-CSCF and/or the I-CSCF options (scscf=<port>, icscf=<port>) were configured in the /etc/clearwater/config file but no Homestead was configured in the same file.",
   "The Sprout application will exit until the problem is fixed.",
   2,
   "Set the hs_hostname=<fully-qualified-homestead-name> option in the /etc/clearwater/config file.",
   "Example: hs_hostname=homestead-1.os3.richlab.datcon.co.uk"
   );
static const PDLog CL_SPROUT_AUTH_NO_HOMESTEAD
  (
   PDLogBase::CL_SPROUT_ID + 8,
   PDLOG_ERR,
   "Fatal - Authentication enabled, but no Homestead server specified in /etc/clearwater/config",
   "The hs_hostname was not set in the /etc/clearwater/config file",
   "The Sprout application will exit.",
   1,
   "Set the hs_hostname=<fully-qualified-homestead-name> option in the /etc/clearwater/config file.  Example: hs_hostname=homestead-1.os3.richlab.datcon.co.uk"
   );
static const PDLog CL_SPROUT_XDM_NO_HOMESTEAD
  (
   PDLogBase::CL_SPROUT_ID + 9,
   PDLOG_ERR,
   "Fatal - Homer XDM service is configured but no Homestead server specified in /etc/clearwater/config",
   "The hs_hostname was not set in the /etc/clearwater/config file",
   "The Sprout application will exit.",
   1,
   "Set the hs_hostname=<fully-qualified-homestead-name> option in the /etc/clearwater/config file.  Example: hs_hostname=homestead-1.os3.richlab.datcon.co.uk"
   );
static const PDLog CL_SPROUT_S_CSCF_NO_CHRONOS
  (
   PDLogBase::CL_SPROUT_ID + 10,
   PDLOG_ERR,
   "Fatal - S-CSCF enabled with no Chronos service specified in /etc/clearwater/config",
   "The chronos_hostname=<host:port> was not set in /etc/clearwater/config",
   "The Sprout application will exit.",
   1,
   "Set the chronos_hostname=<host:port> option in the /etc/clearwater/config. Example: chronos_hostname=localhost:7253 "
   );
static const PDLog CL_SPROUT_BAD_S_CSCF_JSON
  (
   PDLogBase::CL_SPROUT_ID + 11,
   PDLOG_ERR,
   "Fatal - Missing or malformed /etc/clearwater/s-cscf.json file",
   "The s-cscf.json file must be corrected or created to provide S-CSCF service.",
   "The Sprout application will exit.",
   1,
   "Consult the Clearwater installation document and correct or add the /etc/clearwater/s-cscf.json file."
   );
static const PDLog1<const char*> CL_SPROUT_SIP_INIT_IFC_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 12,
   PDLOG_ERR,
   "Fatal - Error initializing sip interfaces with error %s",
   "The SIP interfaces could not be started.",
   "Sprout will exit.",
   1,
   "Report the error."
   );
static const PDLog CL_SPROUT_NO_RALF_CONFIGURED
  (
   PDLogBase::CL_SPROUT_ID + 13,
   PDLOG_ERR,
   "Sprout did not start a connection to Ralf because Ralf is not enabled",
   "Ralf was not configured in the /etc/clearwater/config file.",
   "Billing service will not be available.",
   2,
   "Correct the /etc/clearwater/config file.",
   "Example: ralf_hostname=ralf.os3.richlab.datcon.co.uk:10888"
   );
static const PDLog CL_SPROUT_MEMCACHE_CONN_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 14,
   PDLOG_ERR,
   "Fatal - Failed to connect to the memcache data store",
   "The connection to the local store could not be crated.",
   "Sprout will exit.",
   1,
   "Report this issue."
   );
static const PDLog1<const char*> CL_SPROUT_INIT_SERVICE_ROUTE_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 15,
   PDLOG_ERR,
   "Fatal - Failed to send a service-route header to the S-CSCF service %s",
   "On initialization Sprout sends a service route request.  This failed.",
   "The S-CSCF was not enabled.  The Sprout application fails.",
   1,
   "Report the problem as a software issue."
   );
static const PDLog1<const char*> CL_SPROUT_REG_SUBSCRIBER_HAND_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 16,
   PDLOG_ERR,
   "Fatal - Failed to register the SUBSCRIBE handlers with the SIP stack %s",
   "The Sprout subscription module could not be loaded.",
   "Sprout will exit.",
   1,
   "Report this issue."
   );
static const PDLog1<const char*> CL_SPROUT_S_CSCF_INIT_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 17,
   PDLOG_ERR,
   "Fatal - The S-CSCF service failed to initialize %s",
   "The S-CSCF did not initialize",
   "The S-CSCF proxy is not enabled.",
   1,
   "Report this issue."
   );
static const PDLog CL_SPROUT_I_CSCF_INIT_FAIL 
  (
   PDLogBase::CL_SPROUT_ID + 18,
   PDLOG_ERR,
   "Fatal - The I-CSCF service failed to initialize",
   "The I-CSCF service did not initialize.",
   "Sprout will exit.",
   1,
   "Report this issue."
   );
static const PDLog1<const char*> CL_SPROUT_SIP_STACK_INIT_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 19,
   PDLOG_ERR,
   "Fatal - The SIP stack failed to initialize with error, %s",
   "The SIP interfaces could not be started.",
   "Sprout will exit.",
   1,
   "Report the issue."
   );
static const PDLog2<const char*, int> CL_SPROUT_HTTP_IFC_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 20,
   PDLOG_ERR,
   "An HTTP interface failed to initialize or start in %s with error %d",
   "The timeout handlers for Sprout could not be registered with Chronos.",
   "Timeout events won't occur.",
   1,
   "Restart Sprout.  if this does not restart the issue report it."
   );
static const PDLog CL_SPROUT_ENDED 
  (
   PDLogBase::CL_SPROUT_ID + 21,
   PDLOG_ERR,
   "Sprout is ending -- Shutting down",
  "Sprout has been terminated by monit or has exited",
  "Sprout services are not longer available",
   2,
  "(1)This occurs normally when Sprout is stopped.",
  "(2). If Sprout failed to respond then monit can restart Chronos.  Report this issue."
   );
static const PDLog2<const char*, int> CL_SPROUT_HTTP_IFC_STOP_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 22,
   PDLOG_ERR,
   "The HTTP interfaces encountered an error when stopping the HTTP stack in %s with error %d",
   "When Sprout was exiting it encountered an error when shutting down the HTTP stack.",
   "Not critical as Sprout is exiting anyway.",
   1,
   "Report this issue."
   );
// Need to make an interval
static const PDLog2<const char*, const char*> CL_SPROUT_SIP_SEND_REQUEST_ERR
  (
   PDLogBase::CL_SPROUT_ID + 23,
   PDLOG_ERR,
   "Failed to send SIP request to %s with error %s",
   "An attempt to send a SIP request failed.",
   "This may cause a call to fail.",
   2,
   "(1). Check to see if the target has failed."
   "(2). If the problem persists check the network interfaces to the target of the SIP request using Wireshark."
   );
static const PDLog CL_SPROUT_SIP_DEADLOCK
  (
   PDLogBase::CL_SPROUT_ID + 24,
   PDLOG_ERR,
   "Fatal - Sprout detected a fatal software deadlock affecting SIP communication",
   "An internal Sprout software error has been detected.",
   "A SIP interface has failed.",
   1,
   "Report the issue."
   );
static const PDLog2<int, const char*> CL_SPROUT_SIP_UDP_IFC_START_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 25,
   PDLOG_ERR,
   "Failed to start a SIP UDP interface for port %d with error %s",
   "Sprout could not start a UDP interface.",
   "This may affect call processing.",
   2,
   "(1). If the problem persists, restart the sprout application.",
   "(2). If the problem does not clear report the issue"
   );
static const PDLog2<int, const char*> CL_SPROUT_SIP_TCP_START_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 26,
   PDLOG_ERR,
   "Failed to start a SIP TCP transport for port %d with error %s",
   "Failed to start a SIP TCP connection.",
   "This may affect call processing.",
   2,
   "(1). If the problem persists, restart the sprout application.",
   "(2). If the problem does not clear report the issue"
   );
static const PDLog2<int, const char*> CL_SPROUT_SIP_TCP_SERVICE_START_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 27,
   PDLOG_ERR,
   "Failed to start a SIP TCP service for port %d with error %s",
   "Sprout could not start a TCP service.",
   "This may affect call processing.",
   3,
   "(1). Check to see that the scscf_port or icscf_port in the /etc/clearwater/config file do not conflict with any other service.",
   "(2). If the problem persists, restart the sprout application.",
   "(3). If the problem does not clear report the issue"
   );
static const PDLog1<int> CL_SPROUT_UNTRUSTED_P_CSCF_END 
  (
   PDLogBase::CL_SPROUT_ID + 28,
   PDLOG_ERR,
   "The untrusted P-CSCF service on port %d has ended",
   "The untrusted P-CSCF service is no longer available.",
   "The untrusted P-CSCF service is no longer available.",
   1,
   "Restart Sprout."
   );
static const PDLog1<int> CL_SPROUT_TRUSTED_P_CSCF_END
  (
   PDLogBase::CL_SPROUT_ID + 29,
   PDLOG_ERR,
   "The trusted P-CSCF service on port %d has ended",
   "The trusted P-CSCF service is no longer available.",
   "This may affect call processing",
   1,
   "Restart Sprout."
   );
static const PDLog1<int> CL_SPROUT_S_CSCF_END
  (
   PDLogBase::CL_SPROUT_ID + 30,
   PDLOG_ERR,
   "The S-CSCF service on port %d has ended",
   "The S-CSCF service is no longer available.",
   "Call processing is no longer available.",
   2,
   "(1). Restart the Sprout application.",
   "(2).  If the problem persists report the issue."
   );
static const PDLog1<int> CL_SPROUT_I_CSCF_END
  (
   PDLogBase::CL_SPROUT_ID + 31,
   PDLOG_ERR,
   "The I-CSCF service on port %d has ended",
   "The I-CSCF service is no longer available.",
   "Call processing is no longer available.",
   2,
   "(1). Restart the Sprout application.",
   "(2).  If the problem persists report the issue."
   );
static const PDLog1<int> CL_SPROUT_UNTRUSTED_P_CSCF_STARTED
  (
   PDLogBase::CL_SPROUT_ID + 32,
   PDLOG_NOTICE,
   "The untrusted P-CSCF service on port %d was started",
   "The untrusted P-CSCF service is now available.",
   "Normal",
   1,
   "None"
   );
static const PDLog1<int> CL_SPROUT_P_CSCF_INIT_FAIL
  (
   PDLogBase::CL_SPROUT_ID + 33,
   PDLOG_ERR,
   "The untrusted P-CSCF service on port %d failed to initialize",
   "The untrusted P-CSCF service is not available.",
   "This will affect call processing.",
   2,
   "(1). Restart the Sprout application.",
   "(2).  If the problem persists report the issue."
   );
static const PDLog1<int> CL_SPROUT_S_CSCF_AVAIL
  (
   PDLogBase::CL_SPROUT_ID + 34,
   PDLOG_NOTICE,
   "The S-CSCF service on port %d is now available",
   "The S-CSCF service is now available.",
   "Normal",
   1,
   "None"
   );
static const PDLog1<int> CL_SPROUT_S_CSCF_INIT_FAIL2
  (
   PDLogBase::CL_SPROUT_ID + 35,
   PDLOG_ERR,
   "The S-CSCF service on port %d failed to initialize",
   "The S-CSCF service is no longer available.",
   "Call processing is no longer available.",
   2,
   "(1). Restart the Sprout application.",
   "(2).  If the problem persists report the issue."
   );
static const PDLog1<int> CL_SPROUT_I_CSCF_AVAIL
  (
   PDLogBase::CL_SPROUT_ID + 36,
   PDLOG_NOTICE,
   "The I-CSCF service on port %d is now available",
   "The I-CSCF service is now available.",
   "Call processing is no longer available.",
   2,
   "(1). Restart the Sprout application.",
   "(2).  If the problem persists report the issue."
   );
static const PDLog1<int> CL_SPROUT_I_CSCF_INIT_FAIL2
  (
   PDLogBase::CL_SPROUT_ID + 37,
   PDLOG_ERR,
   "The I-CSCF service on port %d failed to initialize",
   "The I-CSCF service is now available.",
   "Call processing is no longer available.",
   2,
   "(1). Restart the Sprout application.",
   "(2).  If the problem persists report the issue."
   );



#endif
