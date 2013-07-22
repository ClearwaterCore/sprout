/**
 * @file custom_headers.cpp Implementations for custom SIP header handling
 * functions.
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

extern "C" {
  #include <pjsip.h>
}

#include "pjutils.h"
#include "constants.h"
#include "custom_headers.h"


/// Custom parser for Privacy header.  This is registered with PJSIP when
/// we initialize the stack.
pjsip_hdr* parse_hdr_privacy(pjsip_parse_ctx *ctx)
{
  pjsip_generic_array_hdr *privacy = pjsip_generic_array_hdr_create(ctx->pool, &STR_PRIVACY);
  pjsip_parse_generic_array_hdr_imp(privacy, ctx->scanner);
  return (pjsip_hdr*)privacy;
}

typedef void* (*clone_fptr)(pj_pool_t *, const void*);
typedef int   (*print_fptr)(void *hdr, char *buf, pj_size_t len);

pjsip_hdr_vptr identity_hdr_vptr =
{
  (clone_fptr) &identity_hdr_clone,
  (clone_fptr) &identity_hdr_shallow_clone,
  (print_fptr) &identity_hdr_print,
};


/// Custom create, clone and print functions used for the P-Associated-URI,
/// P-Asserted-Identity and P-Preferred-Identity headers
int identity_hdr_print(pjsip_routing_hdr *hdr,
                              char *buf,
                              pj_size_t size)
{
  int printed;
  char *startbuf = buf;
  char *endbuf = buf + size;
  const pjsip_parser_const_t *pc = pjsip_parser_const();

  /* Route and Record-Route don't compact forms */
  copy_advance(buf, hdr->name);
  *buf++ = ':';
  *buf++ = ' ';

  printed = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR,
                            &hdr->name_addr,
                            buf,
                            endbuf-buf);
  if (printed < 1)
  {
    return -1;
  }
  buf += printed;

  printed = pjsip_param_print_on(&hdr->other_param, buf, endbuf-buf,
                                 &pc->pjsip_TOKEN_SPEC,
                                 &pc->pjsip_TOKEN_SPEC, ';');
  if (printed < 0)
  {
    return -1;
  }
  buf += printed;

  return buf-startbuf;
}


pjsip_routing_hdr* identity_hdr_clone(pj_pool_t *pool,
                                      const pjsip_routing_hdr *rhs)
{
  pjsip_routing_hdr *hdr = PJUtils::identity_hdr_create(pool, rhs->name);
  pjsip_name_addr_assign(pool, &hdr->name_addr, &rhs->name_addr);
  pjsip_param_clone(pool, &hdr->other_param, &rhs->other_param);
  return hdr;
}


pjsip_routing_hdr* identity_hdr_shallow_clone(pj_pool_t *pool,
                                              const pjsip_routing_hdr *rhs)
{
  pjsip_routing_hdr *hdr = PJ_POOL_ALLOC_T(pool, pjsip_routing_hdr);
  pj_memcpy(hdr, rhs, sizeof(*hdr));
  pjsip_param_shallow_clone(pool, &hdr->other_param, &rhs->other_param);
  return hdr;
}


/// Custom parser for P-Associated-URI header.  This is registered with PJSIP when
/// we initialize the stack.
pjsip_hdr* parse_hdr_p_associated_uri(pjsip_parse_ctx *ctx)
{
  // The P-Associated-URI header is a comma separated list of name-addrs
  // with optional parameters, so we parse it to multiple header structures,
  // using the pjsip_route_hdr structure for each.
  pjsip_route_hdr *first = NULL;
  pj_scanner *scanner = ctx->scanner;

  do
  {
    pjsip_route_hdr *hdr = PJUtils::identity_hdr_create(ctx->pool, STR_P_ASSOCIATED_URI);
    if (!first)
    {
      first = hdr;
    }
    else
    {
      pj_list_insert_before(first, hdr);
    }
    pjsip_name_addr *temp = pjsip_parse_name_addr_imp(scanner, ctx->pool);

    pj_memcpy(&hdr->name_addr, temp, sizeof(*temp));

    while (*scanner->curptr == ';')
    {
      pjsip_param *p = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
      pjsip_parse_param_imp(scanner, ctx->pool, &p->name, &p->value, 0);
      pj_list_insert_before(&hdr->other_param, p);
    }

    if (*scanner->curptr == ',')
    {
      pj_scan_get_char(scanner);
    }
    else
    {
      break;
    }
  } while (1);
  pjsip_parse_end_hdr_imp(scanner);

  return (pjsip_hdr*)first;
}


/// Custom parser for P-Asserted-Identity header.  This is registered with PJSIP when
/// we initialize the stack.
pjsip_hdr* parse_hdr_p_asserted_identity(pjsip_parse_ctx *ctx)
{
  // The P-Asserted-Identity header is a comma separated list of name-addrs
  // so we parse it to multiple header structures, using the pjsip_route_hdr
  // structure for each.  Note that P-Asserted-Identity cannot have parameters
  // after the name-addr.
  pjsip_route_hdr *first = NULL;
  pj_scanner *scanner = ctx->scanner;

  do
  {
    pjsip_route_hdr *hdr = PJUtils::identity_hdr_create(ctx->pool, STR_P_ASSERTED_IDENTITY);
    if (!first)
    {
      first = hdr;
    }
    else
    {
      pj_list_insert_before(first, hdr);
    }
    pjsip_name_addr *temp = pjsip_parse_name_addr_imp(scanner, ctx->pool);

    pj_memcpy(&hdr->name_addr, temp, sizeof(*temp));

    if (*scanner->curptr == ',')
    {
      pj_scan_get_char(scanner);
    }
    else
    {
      break;
    }
  } while (1);
  pjsip_parse_end_hdr_imp(scanner);

  return (pjsip_hdr*)first;
}


/// Custom parser for P-Preferred-Identity header.  This is registered with PJSIP when
/// we initialize the stack.
pjsip_hdr* parse_hdr_p_preferred_identity(pjsip_parse_ctx *ctx)
{
  // The P-Preferred-Identity header is a comma separated list of name-addrs
  // so we parse it to multiple header structures, using the pjsip_route_hdr
  // structure for each.  Note that P-Preferred-Identity cannot have parameters
  // after the name-addr.
  pjsip_route_hdr *first = NULL;
  pj_scanner *scanner = ctx->scanner;

  do
  {
    pjsip_route_hdr *hdr = PJUtils::identity_hdr_create(ctx->pool, STR_P_PREFERRED_IDENTITY);
    if (!first)
    {
      first = hdr;
    }
    else
    {
      pj_list_insert_before(first, hdr);
    }
    pjsip_name_addr *temp = pjsip_parse_name_addr_imp(scanner, ctx->pool);

    pj_memcpy(&hdr->name_addr, temp, sizeof(*temp));

    if (*scanner->curptr == ',')
    {
      pj_scan_get_char(scanner);
    }
    else
    {
      break;
    }
  } while (1);
  pjsip_parse_end_hdr_imp(scanner);

  return (pjsip_hdr*)first;
}

pjsip_hdr* parse_hdr_p_charging_vector(pjsip_parse_ctx* ctx)
{
  // The P-Charging-Vector header has the following ABNF:
  //
  // P-Charging-Vector     = "P-Charging-Vector" HCOLON icid-value
  //                         *(SEMI charge-params)
  // charge-params         = icid-gen-addr / orig-ioi /
  //                         term-ioi / generic-param
  // icid-value            = "icid-value" EQUAL gen-value
  // icid-gen-addr         = "icid-generated-at" EQUAL host
  // orig-ioi              = "orig-ioi" EQUAL gen-value
  // term-ioi              = "term-ioi" EQUAL gen-value

  pj_pool_t* pool = ctx->pool;
  pj_scanner* scanner = ctx->scanner;
  pjsip_p_c_v_hdr* hdr = pjsip_p_c_v_hdr_create(pool);
  pj_str_t name;
  pj_str_t value;

  // Parse the required icid-value parameter first.
  pjsip_parse_param_imp(scanner, pool, &name, &value,
                        PJSIP_PARSE_REMOVE_QUOTE);
  if (!pj_stricmp2(&name, "icid-value")) {
    hdr->icid = value;
  } else {
    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
  }

  // Should always need to swallow the ';' for the icid-value param.
  if (*scanner->curptr == ';') {
    pj_scan_get_char(scanner);
  } else {
    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
  }

  // Now parse the rest of the params.
  for (;;) {
    pjsip_parse_param_imp(scanner, pool, &name, &value,
                          PJSIP_PARSE_REMOVE_QUOTE);

    if (!pj_stricmp2(&name, "orig-ioi")) {
      hdr->orig_ioi = value;
    } else if (!pj_stricmp2(&name, "term-ioi")) {
      hdr->term_ioi = value;
    } else if (!pj_stricmp2(&name, "icid-generated-at")) {
      hdr->icid_gen_addr = value;
    } else {
      pjsip_param *param = PJ_POOL_ALLOC_T(pool, pjsip_param);
      param->name = name;
      param->value = value;
      pj_list_insert_before(&hdr->other_param, param);
    }

    // May need to swallow the ';' for the previous param.
    if (!pj_scan_is_eof(scanner) && *scanner->curptr == ';') {
      pj_scan_get_char(scanner);
    }

    // If the next character is a newline (after skipping whitespace)
    // we're done.
    pj_scan_skip_whitespace(scanner);
    if (pj_scan_is_eof(scanner) ||
        (*scanner->curptr == '\r') ||
        (*scanner->curptr == '\n')) {
      break;
    }
  }

  // We're done parsing this header.
  pjsip_parse_end_hdr_imp(scanner);

  return (pjsip_hdr*)hdr;
}

pjsip_p_c_v_hdr* pjsip_p_c_v_hdr_create(pj_pool_t* pool)
{
  void* mem = pj_pool_alloc(pool, sizeof(pjsip_p_c_v_hdr));
  return pjsip_p_c_v_hdr_init(pool, mem);
}

pjsip_hdr_vptr pjsip_p_c_v_vptr = {
  pjsip_p_c_v_hdr_clone,
  pjsip_p_c_v_hdr_shallow_clone,
  pjsip_p_c_v_hdr_print_on
};

pjsip_p_c_v_hdr* pjsip_p_c_v_hdr_init(pj_pool_t* pool, void* mem)
{
  pjsip_p_c_v_hdr* hdr = (pjsip_p_c_v_hdr*)mem;
  PJ_UNUSED_ARG(pool);
  
  // Based on init_hdr from sip_msg.c
  hdr->type = PJSIP_H_OTHER;
  hdr->name = STR_P_C_V;
  hdr->sname = STR_P_C_V;
  hdr->vptr = &pjsip_p_c_v_vptr;
  pj_list_init(hdr);
  pj_list_init(&hdr->other_param);

  return hdr;
}

void *pjsip_p_c_v_hdr_clone(pj_pool_t* pool, const void* o)
{
  pjsip_p_c_v_hdr* hdr = pjsip_p_c_v_hdr_create(pool);
  pjsip_p_c_v_hdr* other = (pjsip_p_c_v_hdr*)o;
  pj_strdup(pool, &hdr->icid, &other->icid);
  pj_strdup(pool, &hdr->orig_ioi, &other->orig_ioi);
  pj_strdup(pool, &hdr->term_ioi, &other->term_ioi);
  pj_strdup(pool, &hdr->icid_gen_addr, &other->icid_gen_addr);
  pjsip_param_clone(pool, &hdr->other_param, &other->other_param);
  return hdr;
}

void *pjsip_p_c_v_hdr_shallow_clone(pj_pool_t* pool, const void* o)
{
  pjsip_p_c_v_hdr* hdr = pjsip_p_c_v_hdr_create(pool);
  pjsip_p_c_v_hdr* other = (pjsip_p_c_v_hdr*)o;
  hdr->icid = other->icid;
  hdr->orig_ioi = other->orig_ioi;
  hdr->term_ioi = other->term_ioi;
  hdr->icid_gen_addr = other->icid_gen_addr;
  pjsip_param_shallow_clone(pool, &hdr->other_param, &other->other_param);
  return hdr;
}

int pjsip_p_c_v_hdr_print_on(void* h, char* buf, pj_size_t len)
{
  const pjsip_parser_const_t *pc = pjsip_parser_const();
  pjsip_p_c_v_hdr* hdr = (pjsip_p_c_v_hdr*)h;
  char* p = buf;

  // Check the fixed parts of the header will fit.
  int needed = 0;
  needed += hdr->name.slen; // Header name
  needed += 2;              // : and space
  needed += 11;              // icid-value=
  needed += hdr->icid.slen; // <icid>
  needed += 1;              // ;
  if (hdr->orig_ioi.slen) {
    needed += 9;              // orig-ioi=
    needed += hdr->orig_ioi.slen; // <orig-ioi>
    needed += 1;              // ;
  }
  if (hdr->term_ioi.slen) {
    needed += 9;              // term-ioi=
    needed += hdr->term_ioi.slen; // <term-ioi>
    needed += 1;              // ;
  }
  if (hdr->icid_gen_addr.slen) {
    needed += 18;              // icid-generated-at=
    needed += hdr->icid_gen_addr.slen; // <icid-generated-at>
  }

  if (needed > (pj_ssize_t)len) {
    return -1;
  }
  
  // Now write the fixed header out.
  pj_memcpy(p, hdr->name.ptr, hdr->name.slen);
  p += hdr->name.slen;
  *p++ = ':';
  *p++ = ' ';
  pj_memcpy(p, "icid-value=", 11);
  p += 11;
  pj_memcpy(p, hdr->icid.ptr, hdr->icid.slen);
  p += hdr->icid.slen;
  if (hdr->orig_ioi.slen) {
    *p++ = ';';
    pj_memcpy(p, "orig-ioi=", 9);
    p += 9;
    pj_memcpy(p, hdr->orig_ioi.ptr, hdr->orig_ioi.slen);
    p += hdr->orig_ioi.slen;
  }
  if (hdr->term_ioi.slen) {
    *p++ = ';';
    pj_memcpy(p, "term-ioi=", 9);
    p += 9;
    pj_memcpy(p, hdr->term_ioi.ptr, hdr->term_ioi.slen);
    p += hdr->term_ioi.slen;
  }
  if (hdr->icid_gen_addr.slen) {
    *p++ = ';';
    pj_memcpy(p, "icid-generated-at=", 18);
    p += 18;
    pj_memcpy(p, hdr->icid_gen_addr.ptr, hdr->icid_gen_addr.slen);
    p += hdr->icid_gen_addr.slen;
  }
  
  // Attempt to write out the other params.
  pj_ssize_t printed = pjsip_param_print_on(&hdr->other_param, p, buf+len-p,
                                            &pc->pjsip_TOKEN_SPEC,
                                            &pc->pjsip_TOKEN_SPEC, ';');
  if (printed < 0) {
    return -1;
  }
  p += printed;
  *p = '\0';

  return p - buf;
}

pjsip_hdr* parse_hdr_p_charging_function_addresses(pjsip_parse_ctx* ctx)
{
  // The P-Charging-Function-Addresses header has the following ABNF:
  //
  // P-Charging-Addr        = "P-Charging-Function-Addresses" HCOLON
  //                          charge-addr-params
  //                          *(SEMI charge-addr-params)
  // charge-addr-params     = ccf / ecf / generic-param
  // ccf                    = "ccf" EQUAL gen-value
  // ecf                    = "ecf" EQUAL gen-value
  //
  // Where the ccf and ecf elements may be repeated to specify backup CDFs
  // for redundancy.

  pj_pool_t* pool = ctx->pool;
  pj_scanner* scanner = ctx->scanner;
  pjsip_p_c_f_a_hdr* hdr = pjsip_p_c_f_a_hdr_create(pool);
  pj_str_t name;
  pj_str_t value;
  pjsip_param *param;

  for (;;) {
    pjsip_parse_param_imp(scanner, pool, &name, &value,
                          PJSIP_PARSE_REMOVE_QUOTE);
    param = PJ_POOL_ALLOC_T(pool, pjsip_param);
    param->name = name;
    param->value = value;
    if (!pj_stricmp2(&name, "ccf")) {
      pj_list_insert_before(&hdr->ccf, param);
    } else if (!pj_stricmp2(&name, "ecf")) {
      pj_list_insert_before(&hdr->ecf, param);
    } else {
      pj_list_insert_before(&hdr->other_param, param);
    }

    // We might need to swallow the ';'.
    if (!pj_scan_is_eof(scanner) && *scanner->curptr == ';') {
      pj_scan_get_char(scanner);
    }

    // If we're EOF or looking at a newline, we're done.
    pj_scan_skip_whitespace(scanner);
    if (pj_scan_is_eof(scanner) ||
        (*scanner->curptr == '\r') ||
        (*scanner->curptr == '\n')) {
      break;
    }
  }

  // We're done parsing this header.
  pjsip_parse_end_hdr_imp(scanner);

  return (pjsip_hdr*)hdr;
}

pjsip_p_c_f_a_hdr* pjsip_p_c_f_a_hdr_create(pj_pool_t* pool)
{
  void* mem = pj_pool_alloc(pool, sizeof(pjsip_p_c_f_a_hdr));
  return pjsip_p_c_f_a_hdr_init(pool, mem);
}

pjsip_hdr_vptr pjsip_p_c_f_a_vptr = {
  pjsip_p_c_f_a_hdr_clone,
  pjsip_p_c_f_a_hdr_shallow_clone,
  pjsip_p_c_f_a_hdr_print_on
};

pjsip_p_c_f_a_hdr* pjsip_p_c_f_a_hdr_init(pj_pool_t* pool, void* mem)
{
  pjsip_p_c_f_a_hdr* hdr = (pjsip_p_c_f_a_hdr*)mem;
  PJ_UNUSED_ARG(pool);

  // Based on init_hdr from sip_msg.c
  hdr->type = PJSIP_H_OTHER;
  hdr->name = STR_P_C_F_A;
  hdr->sname = STR_P_C_F_A;
  hdr->vptr = &pjsip_p_c_f_a_vptr;
  pj_list_init(hdr);
  pj_list_init(&hdr->ccf);
  pj_list_init(&hdr->ecf);
  pj_list_init(&hdr->other_param);

  return hdr;
}

void *pjsip_p_c_f_a_hdr_clone(pj_pool_t* pool, const void* o)
{
  pjsip_p_c_f_a_hdr* hdr = pjsip_p_c_f_a_hdr_create(pool);
  pjsip_p_c_f_a_hdr* other = (pjsip_p_c_f_a_hdr*)o;

  pjsip_param_clone(pool, &hdr->ccf, &other->ccf);
  pjsip_param_clone(pool, &hdr->ecf, &other->ecf);
  pjsip_param_clone(pool, &hdr->other_param, &other->other_param);

  return hdr;
}

void *pjsip_p_c_f_a_hdr_shallow_clone(pj_pool_t* pool, const void* o)
{
  pjsip_p_c_f_a_hdr* hdr = pjsip_p_c_f_a_hdr_create(pool);
  pjsip_p_c_f_a_hdr* other = (pjsip_p_c_f_a_hdr*)o;

  pjsip_param_shallow_clone(pool, &hdr->ccf, &other->ccf);
  pjsip_param_shallow_clone(pool, &hdr->ecf, &other->ecf);
  pjsip_param_shallow_clone(pool, &hdr->other_param, &other->other_param);

  return hdr;
}

int pjsip_p_c_f_a_hdr_print_on(void *h, char* buf, pj_size_t len)
{
  const pjsip_parser_const_t *pc = pjsip_parser_const();
  pjsip_p_c_f_a_hdr* hdr = (pjsip_p_c_f_a_hdr*)h;
  char* p = buf;

  // Check that at least the header name will fit.
  int needed = 0;
  needed += hdr->name.slen; // Header name
  needed += 2;              // : and space

  if (needed > (pj_ssize_t)len) {
    return -1;
  }
  
  // Now write the header name out.
  pj_memcpy(p, hdr->name.ptr, hdr->name.slen);
  p += hdr->name.slen;
  *p++ = ':';
  *p++ = ' ';

  // Now try to write out the three parameter lists.  Annoyingly,
  // pjsip_param_print_on() will always print the separator before each
  // parameter, including the first parameter in this case.
  //
  // The P-Charging-Function-Addresses header has no body (technically 
  // invalid SIP) and thus we need to print the first parameter without the 
  // separator.  Since this first parameter could be in any of the parameter
  // lists, we have to track (with the found_first_param flag) when we've
  // handled it.
  bool found_first_param = false;
  int printed;

  pjsip_param* param_list = NULL;
  for (int i = 0; i < 3; i++) {
    switch (i) {
      case 0:
        param_list = &hdr->ccf;
        break;
      case 1:
        param_list = &hdr->ecf;
        break;
      case 2:
        param_list = &hdr->other_param;
        break;
    }

    if (pj_list_empty(param_list)) {
      continue;
    }

    if (found_first_param) {
      // Simply write out the parameters
      printed = pjsip_param_print_on(param_list, p, buf+len-p,
                                     &pc->pjsip_TOKEN_SPEC,
                                     &pc->pjsip_TOKEN_SPEC, ';');
      if (printed < 0) {
        return -1;
      }
      p += printed;
    } else {
      // We print the first parameter manually then print the rest.
      pjsip_param* first_param = param_list->next;
      pj_list_erase(first_param);

      // Check we have space for the first param before printing it out.
      needed = pj_strlen(&first_param->name);
      if (first_param->value.slen) {
        needed += 1 + pj_strlen(&first_param->value);
      }
      if (needed > buf+len-p) {
        return -1;
      }

      pj_memcpy(p, first_param->name.ptr, first_param->name.slen);
      p += first_param->name.slen;
      if (first_param->value.slen) {
        *p++ = '=';
        pj_memcpy(p, first_param->value.ptr, first_param->value.slen);
        p += first_param->value.slen;
      }

      // Now print the rest of this parameter list (may be empty).
      printed = pjsip_param_print_on(param_list, p, buf+len-p,
                                     &pc->pjsip_TOKEN_SPEC,
                                     &pc->pjsip_TOKEN_SPEC, ';');
      if (printed < 0) {
        return -1;
      }
      p += printed;

      // Finally, restore the first param to the head of the parameter list.
      pj_list_insert_after(param_list, first_param);

      // We've found the first parameter, everything else is simple.
      found_first_param = true;
    }
  }
 
  *p = '\0';

  return p - buf;
}
