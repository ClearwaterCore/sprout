/**
 * @file sproutdcea.cpp - Craft Description, Cause, Effect, and Action
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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
PDLog1<const char*> CL_SPROUT_INVALID_S_CSCF_PORT
  (
   CL_SPROUT_ID + 1,
   PDLOG_ERR,
   "The S-CSCF port specified in /etc/clearwater/config must be in a range from 1 to 65535 but has a value of %s",
   "The scscf=<port> port value is outside the permitted range",
   "The sprout application will exit.",
   "Correct the port value.  Typically this is set to port value 5054 as an example"
   );
PDLog1<const char*> CL_SPROUT_INVALID_I_CSCF_PORT
  (
   CL_SPROUT_ID + 2,
   PDLOG_ERR,
   "Fatal - The I-CSCF port specified in /etc/clearwater/config must be in a range from 1 to 65535 but has a value of %s",
   "The icscf=<port> value is outside the permitted range",
   "The sprout application will exit",
   "Correct the port value.  Typically this is set to 5052"
   );
PDLog CL_SPROUT_INVALID_SAS_OPTION 
  (
   CL_SPROUT_ID + 3,
   PDLOG_ERR,
   "The sas_server option in /etc/clearwater/config is invalid or not configured",
   "The interface to the SAS is not specified.",
   "No call traces will appear in the sas",
   "Set the fully qualified sas hostname for the sas_server=<host> option.  Example: sas_server=sas-1.os3.richlab.datcon.co.uk.  The application must be restarted to take effect."
   );
PDLog1<const char*> CL_SPROUT_CRASH
  (
   CL_SPROUT_ID + 4,
   PDLOG_ERR,
   "Fatal - Sprout has exited or crashed with signal %s",
   "Sprout has encountered a fatal software error or has been terminated",
   "The Sprout application will restart.",
   "This error can occur if Sprout has been terminated by operator command.  Check the craft log to see if Monit has reported a sprout timeout.  This would be reported as a 'poll_sprout' failed.  Monit will restart sprout for this case.  Actual crashes such as abort, segment trap, bus error trap, should be reported as a problem. "
   );
PDLog CL_SPROUT_STARTED
  (
   CL_SPROUT_ID + 5,
   PDLOG_ERR,
   "Sprout started",
   "The Sprout application is starting.",
   "Normal",
   "None"
   );
PDLog CL_SPROUT_NO_PSI_CSCF
  (
   CL_SPROUT_ID + 6,
   PDLOG_ERR,
   "Fatal - Must enable P-CSCF, S-CSCF or I-CSCF in /etc/clearwater/config",
   "Neither a P-CSCF, S-CSCF, nor an I-CSCF was configured in the /etc/clearwater/config",
   "The Sprout application will exit until the problem is fixed.",
   "The P-CSCF is configured by setting the pcscf=<port> option.  The I-CSCF is configured by setting the scfcf=<port> option.  The I-CSCF is configured by setting the icscf=<port> option."
   );
PDLog CL_SPROUT_SI_CSCF_NO_HOMESTEAD
  (
   CL_SPROUT_ID + 7,
   PDLOG_ERR,
   "Fatal - S/I-CSCF enabled with no Homestead server specified in /etc/clearwater/config",
   "The S-CSCF and/or the I-CSCF options (scscf=<port>, icscf=<port>) were configured in the /etc/clearwater/config file but no Homestead was configured in the same file.",
   "The Sprout application will exit until the problem is fixed.",
   "Set the hs_hostname=<fully-qualified-homestead-name> option in the /etc/clearwater/config file.  Example: hs_hostname=homestead-1.os3.richlab.datcon.co.uk"
   );
PDLog CL_SPROUT_AUTH_NO_HOMESTEAD
  (
   CL_SPROUT_ID + 8,
   PDLOG_ERR,
   "Fatal - Authentication enabled, but no Homestead server specified in /etc/clearwater/config",
   "The hs_hostname was not set in the /etc/clearwater/config file",
   "The Sprout application will exit.",
   "Set the hs_hostname=<fully-qualified-homestead-name> option in the /etc/clearwater/config file.  Example: hs_hostname=homestead-1.os3.richlab.datcon.co.uk"
   );
PDLog CL_SPROUT_XDM_NO_HOMESTEAD
  (
   CL_SPROUT_ID + 9,
   PDLOG_ERR,
   "Fatal - Homer XDM service is configured but no Homestead server specified in /etc/clearwater/config",
   "",
   "",
   ""
   );
PDLog CL_SPROUT_S_CSCF_NO_CHRONOS
  (
   CL_SPROUT_ID + 10,
   PDLOG_ERR,
   "Fatal - S-CSCF enabled with no Chronos service specified in /etc/clearwater/config",
   "The chronos_hostname=<host:port> was not set in /etc/clearwater/config",
   "The Sprout application will exit.",
   "Set the chronos_hostname=<host:port> option in the /etc/clearwater/config. Example: chronos_hostname=localhost:7253 "
   );
PDLog CL_SPROUT_BAD_S_CSCF_JSON
  (
   CL_SPROUT_ID + 11,
   PDLOG_ERR,
   "2011 - Fatal - Missing or malformed /etc/clearwater/s-cscf.json file",
   "",
   "",
   ""
   );
PDLog1<const char*> CL_SPROUT_SIP_INIT_IFC_FAIL
  (
   CL_SPROUT_ID + 12,
   PDLOG_ERR,
   "Fatal - Error initializing sip interfaces with error %s",
   "",
   "",
   ""
   );
PDLog CL_SPROUT_NO_RALF_CONFIGURED
  (
   CL_SPROUT_ID + 13,
   PDLOG_ERR,
   "2013 - Sprout did not start a connection to Ralf because Ralf is not enabled",
   "",
   "",
   ""
   );
PDLog CL_SPROUT_MEMCACHE_CONN_FAIL
  (
   CL_SPROUT_ID + 14,
   PDLOG_ERR,
   "Fatal - Failed to connect to the memcache data store",
   "",
   "",
   ""
   );
PDLog1<const char*> CL_SPROUT_INIT_SERVICE_ROUTE_FAIL
  (
   CL_SPROUT_ID + 15,
   PDLOG_ERR,
   "Fatal - Failed to send a service-route header to the S-CSCF service %s",
   "On initialization Sprout sends a service route request.  This failed.",
   "The S-CSCF was not enabled",
   "The Sprout application fails.  Report the problem as a softwae issue."
   );
PDLog1<const char*> CL_SPROUT_REG_SUBSCRIBER_HAND_FAIL
  (
   CL_SPROUT_ID + 16,
   PDLOG_ERR,
   "Fatal - Failed to register the SUBSCRIBE handlers with the SIP stack %s",
   "",
   "",
   ""
   );
PDLog1<const char*> CL_SPROUT_S_CSCF_INIT_FAIL
  (
   CL_SPROUT_ID + 17,
   PDLOG_ERR,
   "Fatal - The S-CSCF service failed to initialize %s",
   "The S-CSCF did not initialize",
   "The S-CSCF proxy is not enabled.",
   "Report the error."
   );
PDLog CL_SPROUT_I_CSCF_INIT_FAIL 
  (
   CL_SPROUT_ID + 18,
   PDLOG_ERR,
   "Fatal - The I-CSCF service failed to initialize",
   "",
   "",
   ""
   );
PDLog1<const char*> CL_SPROUT_SIP_STACK_INIT_FAIL
  (
   CL_SPROUT_ID + 19,
   PDLOG_ERR,
   "Fatal - The SIP stack failed to initialize with error, %s",
   "",
   "",
   ""
   );
PDLog2<const char*, int> CL_SPROUT_HTTP_IFC_FAIL
  (
   CL_SPROUT_ID + 20,
   PDLOG_ERR,
   "An HTTP interface failed to initializeor start in %s with error %d",
   "",
   "",
   ""
   );
PDLog CL_SPROUT_ENDED 
  (
   CL_SPROUT_ID + 21,
   PDLOG_ERR,
   "Sprout is ending -- Shutting down",
   "",
   "",
   ""
   );
PDLog2<const char*, int> CL_SPROUT_HTTP_IFC_STOP_FAIL
  (
   CL_SPROUT_ID + 22,
   PDLOG_ERR,
   "The HTTP interfaces encountered an error when stopping the HTTP stack in %s with error %d",
   "",
   "",
   ""
   );
PDLog2<const char*, const char*> CL_SPROUT_SIP_SEND_REQUEST_ERR
  (
   CL_SPROUT_ID + 23,
   PDLOG_ERR,
   "Failed to send SIP request to %s with error %s",
   "",
   "",
   ""
   );
PDLog CL_SPROUT_SIP_DEADLOCK
  (
   CL_SPROUT_ID + 24,
   PDLOG_ERR,
   "Fatal - Sprout detected a fatal software deadlock affecting SIP communication",
   "",
   "",
   ""
   );
PDLog2<int, const char*> CL_SPROUT_SIP_UDP_IFC_START_FAIL
  (
   CL_SPROUT_ID + 25,
   PDLOG_ERR,
   "Failed to start a SIP UDP interface for port %d with error %s",
   "",
   "",
   ""
   );
PDLog2<int, const char*> CL_SPROUT_SIP_TCP_START_FAIL
  (
   CL_SPROUT_ID + 26,
   PDLOG_ERR,
   "Failed to start a SIP TCP transport for port %d with error %s",
   "",
   "",
   ""
   );
PDLog2<int, const char*> CL_SPROUT_SIP_TCP_SERVICE_START_FAIL
  (
   CL_SPROUT_ID + 27,
   PDLOG_ERR,
   "Failed to start a SIP TCP service for port %d with error %s",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_UNTRUSTED_P_CSCF_END 
  (
   CL_SPROUT_ID + 28,
   PDLOG_ERR,
   "The untrusted P-CSCF service on port %d has ended",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_TRUSTED_P_CSCF_END
  (
   CL_SPROUT_ID + 29,
   PDLOG_ERR,
   "The trusted P-CSCF service on port %d has ended",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_S_CSCF_END
  (
   CL_SPROUT_ID + 30,
   PDLOG_ERR,
   "The S-CSCF service on port %d has ended",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_I_CSCF_END
  (
   CL_SPROUT_ID + 31,
   PDLOG_ERR,
   "The I-CSCF service on port %d has ended",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_UNTRUSTED_P_CSCF_STARTED
  (
   CL_SPROUT_ID + 32,
   PDLOG_NOTICE,
   "The untrusted P-CSCF service on port %d was started",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_P_CSCF_INIT_FAIL
  (
   CL_SPROUT_ID + 33,
   PDLOG_ERR,
   "The untrusted P-CSCF service on port %d failed to initialize",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_S_CSCF_AVAIL
  (
   CL_SPROUT_ID + 34,
   PDLOG_NOTICE,
   "The S-CSCF service on port %d is now available",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_S_CSCF_INIT_FAIL2
  (
   CL_SPROUT_ID + 35,
   PDLOG_ERR,
   "The S-CSCF service on port %d failed to initialize",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_I_CSCF_AVAIL
  (
   CL_SPROUT_ID + 36,
   PDLOG_NOTICE,
   "The I-CSCF service on port %d is now available",
   "",
   "",
   ""
   );
PDLog1<int> CL_SPROUT_I_CSCF_INIT_FAIL2
  (
   CL_SPROUT_ID + 37,
   PDLOG_ERR,
   "The I-CSCF service on port %d failed to initialize",
   "",
   "",
   ""
   );
