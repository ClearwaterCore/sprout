/**
 * @file icscfsproutlet.cpp Definition of the I-CSCF Sproutlet classes,
 *                          implementing I-CSCF specific SIP proxy functions.
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

#ifndef ICSCFSPROUTLET_H__
#define ICSCFSPROUTLET_H__

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdint.h>
}

#include <vector>
#include <map>

#include "pjutils.h"
#include "hssconnection.h"
#include "scscfselector.h"
#include "enumservice.h"
#include "icscfrouter.h"
#include "acr.h"
#include "sproutlet.h"


class ICSCFSproutletTsx;
class ICSCFSproutletRegTsx;

class ICSCFSproutlet : public Sproutlet
{
public:
  ICSCFSproutlet(const std::string& bgcf_uri,
                 int port,
                 HSSConnection* hss,
                 ACRFactory* acr_factory,
                 SCSCFSelector* scscf_selector,
                 EnumService* enum_service,
                 bool enforce_global_only_lookups,
                 bool enforce_user_phone);

  virtual ~ICSCFSproutlet();

  SproutletTsx* get_tsx(SproutletTsxHelper* helper,
                        const std::string& alias,
                        pjsip_msg* req);

#ifdef UNIT_TEST
  inline void set_global_only_lookups_enforced(bool enforce_global_only_lookups)
  {
    _global_only_lookups = enforce_global_only_lookups;
  }
#endif

private:

  /// Returns the configured BGCF URI for this system.
  inline const pjsip_uri* bgcf_uri() const
  {
    return _bgcf_uri;
  }

  inline HSSConnection* get_hss_connection() const
  {
    return _hss;
  }

  inline SCSCFSelector* get_scscf_selector() const
  {
    return _scscf_selector;
  }

  inline bool get_user_phone() const
  {
    return _user_phone;
  }

  inline bool are_global_only_lookups_enforced() const
  {
    return _global_only_lookups;
  }

  /// Attempts to use ENUM to translate the specified Tel URI into a SIP URI.
  std::string enum_translate_tel_uri(pjsip_tel_uri* uri, SAS::TrailId trail);

  /// Get an ACR instance from the factory.
  /// @param trail                SAS trail identifier to use for the ACR.
  ACR* get_acr(SAS::TrailId trail);

  friend class ICSCFSproutletTsx;
  friend class ICSCFSproutletRegTsx;

  /// A URI which routes to the BGCF.
  pjsip_uri* _bgcf_uri;

  HSSConnection* _hss;

  SCSCFSelector* _scscf_selector;

  ACRFactory* _acr_factory;

  EnumService* _enum_service;

  bool _global_only_lookups;
  bool _user_phone;
};


class ICSCFSproutletTsx : public SproutletTsx
{
public:
  ICSCFSproutletTsx(SproutletTsxHelper* helper, ICSCFSproutlet* icscf);
  ~ICSCFSproutletTsx();

  virtual void on_rx_initial_request(pjsip_msg* req);
  virtual void on_rx_in_dialog_request(pjsip_msg* req);
  virtual void on_tx_request(pjsip_msg* req);
  virtual void on_rx_response(pjsip_msg* rsp, int fork_id);
  virtual void on_tx_response(pjsip_msg* rsp);
  virtual void on_cancel(int status_code, pjsip_msg* req);

private:
  /// Determine whether a status code indicates that the S-CSCF wasn't
  /// found.
  ///
  /// @returns                    True/false.
  /// @param status_code          The status code returned from the S-CSCF
  ///                             lookup.
  inline bool scscf_not_found(const pjsip_status_code scscf_lookup)
  {
    return ((scscf_lookup == PJSIP_SC_NOT_FOUND) ||
            (scscf_lookup == PJSIP_SC_DOES_NOT_EXIST_ANYWHERE));
  }

  /// Perform an ENUM lookup. We only do this for requests containing tel
  /// URIs.
  ///
  /// @returns                    True if we succesfully translate the URI,
  ///                             false otherwise.
  /// @param req                  The request whose URI we are trying to
  ///                             translate
  /// @param pool                 A pool.
  bool translate_tel_uri(pjsip_msg* req, pj_pool_t* pool);

  /// Routes a request to a BGCF.
  ///
  /// @param req                  The request to route.
  void route_to_bgcf(pjsip_msg* req);

  ICSCFSproutlet* _icscf;
  ACR* _acr;
  ICSCFRouter* _router;
  bool _originating;
  bool _routed_to_bgcf;
};

class ICSCFSproutletRegTsx : public SproutletTsx
{
public:
  ICSCFSproutletRegTsx(SproutletTsxHelper* helper, ICSCFSproutlet* icscf);
  ~ICSCFSproutletRegTsx();

  virtual void on_rx_initial_request(pjsip_msg* req);
  virtual void on_rx_in_dialog_request(pjsip_msg* req);
  virtual void on_tx_request(pjsip_msg* req);
  virtual void on_rx_response(pjsip_msg* rsp, int fork_id);
  virtual void on_tx_response(pjsip_msg* rsp);
  virtual void on_cancel(int status_code, pjsip_msg* req);

private:
  ICSCFSproutlet* _icscf;
  ACR* _acr;
  ICSCFRouter* _router;
};

#endif
