/**
 * @file scscfplugin.cpp  Plug-in wrapper for the S-CSCF Sproutlet.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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

#include "cfgoptions.h"
#include "ipv6utils.h"
#include "sproutletplugin.h"
#include "scscfsproutlet.h"
#include "sprout_alarmdefinition.h"
#include "sprout_pd_definitions.h"

class SCSCFPlugin : public SproutletPlugin
{
public:
  SCSCFPlugin();
  ~SCSCFPlugin();

  bool load(struct options& opt, std::list<Sproutlet*>&);
  void unload();

private:
  SCSCFSproutlet* _scscf_sproutlet;
  Alarm* _sess_cont_as_alarm;
  Alarm* _sess_term_as_alarm;
};

/// Export the plug-in using the magic symbol "sproutlet_plugin"
extern "C" {
SCSCFPlugin sproutlet_plugin;
}

SCSCFPlugin::SCSCFPlugin() :
  _scscf_sproutlet(NULL)
{
}

SCSCFPlugin::~SCSCFPlugin()
{
}

/// Loads the S-CSCF plug-in, returning the supported Sproutlets.
bool SCSCFPlugin::load(struct options& opt, std::list<Sproutlet*>& sproutlets)
{
  bool plugin_loaded = true;

  if (opt.enabled_scscf)
  {
    // Determine the S-CSCF, BGCF and I-CSCF URIs.
    std::string node_ip(stack_data.local_host.ptr, stack_data.local_host.slen);

    if (is_ipv6(node_ip))
    {
      node_ip = "[" + node_ip + "]";
    }

    std::string scscf_node_uri = "sip:" + node_ip + ":" + std::to_string(opt.port_scscf);
    std::string icscf_uri;

    if (opt.enabled_icscf)
    {
      icscf_uri = opt.uri_icscf;
    }
    else
    {
      icscf_uri = opt.external_icscf_uri;
    }

    // Create Application Server communication trackers.
    _sess_term_as_alarm = new Alarm("sprout",
                                    AlarmDef::SPROUT_SESS_TERMINATED_AS_COMM_ERROR,
                                    AlarmDef::MAJOR);
    AsCommunicationTracker* sess_term_as_tracker =
        new AsCommunicationTracker(_sess_term_as_alarm,
                                   &CL_SPROUT_SESS_TERM_AS_COMM_FAILURE,
                                   &CL_SPROUT_SESS_TERM_AS_COMM_SUCCESS);

    _sess_cont_as_alarm =  new Alarm("sprout",
                                     AlarmDef::SPROUT_SESS_CONTINUED_AS_COMM_ERROR,
                                     AlarmDef::MINOR);
    AsCommunicationTracker* sess_cont_as_tracker =
        new AsCommunicationTracker(_sess_cont_as_alarm,
                                   &CL_SPROUT_SESS_CONT_AS_COMM_FAILURE,
                                   &CL_SPROUT_SESS_CONT_AS_COMM_SUCCESS);

    _scscf_sproutlet = new SCSCFSproutlet(opt.prefix_scscf,
                                          opt.uri_scscf,
                                          scscf_node_uri,
                                          icscf_uri,
                                          opt.uri_bgcf,
                                          opt.port_scscf,
                                          local_sdm,
                                          {remote_sdm},
                                          hss_connection,
                                          enum_service,
                                          scscf_acr_factory,
                                          opt.override_npdi,
                                          opt.session_continued_timeout_ms,
                                          opt.session_terminated_timeout_ms,
                                          sess_term_as_tracker,
                                          sess_cont_as_tracker);
    plugin_loaded = _scscf_sproutlet->init();

    // We want to prioritise choosing the S-CSCF in ambiguous situations, so
    // make sure it's at the front of the sproutlet list
    sproutlets.push_front(_scscf_sproutlet);
  }

  return plugin_loaded;
}


/// Unloads the S-CSCF plug-in.
void SCSCFPlugin::unload()
{
  delete _scscf_sproutlet;
  delete _sess_term_as_alarm; _sess_term_as_alarm = NULL;
  delete _sess_cont_as_alarm; _sess_cont_as_alarm = NULL;
}
