/**
 * @file sprout_test.cpp UT for Sprout
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

///
/// Uses the Google Test framework to unit-test parts of Sprout.
///
///----------------------------------------------------------------------------

#include <string>
#include <list>
#include <vector>
#include "gtest/gtest.h"

#include "simservs.h"
#include "fakelogger.h"

using namespace std;

/// Fixture for test.
class SimServsTest : public ::testing::Test
{

  SimServsTest()
  {
  }

  virtual ~SimServsTest()
  {
  }
};

struct ss_cdiv_rule;
struct ss_cb_rule;

/// Expected results, defaulting to the normal defaults
struct ss_values {
  bool oip_enabled;
  bool oir_enabled;
  bool oir_presentation_restricted;
  bool cdiv_enabled;
  unsigned int cdiv_no_reply_timer;
  list<ss_cdiv_rule> cdiv_rules;
  bool inbound_cb_enabled;
  list<ss_cb_rule> inbound_cb_rules;
  bool outbound_cb_enabled;
  list<ss_cb_rule> outbound_cb_rules;

  /// Defaults
  ss_values() : oip_enabled(false),
                oir_enabled(false),
                oir_presentation_restricted(true),
                cdiv_enabled(false),
                cdiv_no_reply_timer(20u),
                inbound_cb_enabled(false),
                outbound_cb_enabled(false)
  {
  }
};

///Expected results, defaulting to the normal defaults.
struct ss_cdiv_rule {
  unsigned int conditions;  //< Logical "or" of simservs::Rule::CONDITION_BUSY and friends
  string forward_target;

  /// Defaults
  ss_cdiv_rule() : conditions(0u)
  {
  }
};

///Expected results, defaulting to the normal defaults.
struct ss_cb_rule {
  unsigned int conditions;  //< Logical "or" of simservs::Rule::CONDITION_BUSY and friends
  bool allow_call;

  /// Defaults
  ss_cb_rule() : conditions(0u),
                 allow_call(true)
  {
  }
};

/// Check the expectation of a vector.
template <class E, class A>
void expect_eq(const list<E>& expected,
               const vector<A>& actual,
               void (&do_expect)(const E&, const A&)) //< compare individual elements
{
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() == actual.size())
  {
    typename list<E>::const_iterator exp;
    typename vector<A>::const_iterator act;
    int i;
    for (exp = expected.begin(), act = actual.begin(), i = 0;
         exp != expected.end();
         ++exp, ++act, ++i)
    {
      stringstream ss;
      ss << "Element " << i;
      SCOPED_TRACE(ss.str());
      do_expect(*exp, *act);
    }
  }
}

void expect_cdiv_rule(const ss_cdiv_rule& expected, const simservs::CDIVRule& actual)
{
  EXPECT_EQ(expected.conditions, actual.conditions());
  EXPECT_EQ(expected.forward_target, actual.forward_target());
}

void expect_cb_rule(const ss_cb_rule& expected, const simservs::CBRule& actual)
{
  EXPECT_EQ(expected.conditions, actual.conditions());
  EXPECT_EQ(expected.allow_call, actual.allow_call());
}

/// Check the expectation of simserv values
void expect_ss(ss_values& expected, simservs& actual)
{
  EXPECT_EQ(expected.oip_enabled, actual.oip_enabled());

  EXPECT_EQ(expected.oir_enabled, actual.oir_enabled());
  if (expected.oir_enabled && actual.oir_enabled())
  {
    EXPECT_EQ(expected.oir_presentation_restricted, actual.oir_presentation_restricted());
  }

  SCOPED_TRACE("cdiv");
  EXPECT_EQ(expected.cdiv_enabled, actual.cdiv_enabled());
  if (expected.cdiv_enabled && actual.cdiv_enabled())
  {
    EXPECT_EQ(expected.cdiv_no_reply_timer, actual.cdiv_no_reply_timer());
    expect_eq(expected.cdiv_rules, *actual.cdiv_rules(), expect_cdiv_rule);
  }

  SCOPED_TRACE("inbound");
  EXPECT_EQ(expected.inbound_cb_enabled, actual.inbound_cb_enabled());
  if (expected.inbound_cb_enabled && actual.inbound_cb_enabled())
  {
    expect_eq(expected.inbound_cb_rules, *actual.inbound_cb_rules(), expect_cb_rule);
  }

  SCOPED_TRACE("outbound");
  EXPECT_EQ(expected.outbound_cb_enabled, actual.outbound_cb_enabled());
  if (expected.outbound_cb_enabled && actual.outbound_cb_enabled())
  {
    expect_eq(expected.outbound_cb_rules, *actual.outbound_cb_rules(), expect_cb_rule);
  }
}


/// Empty XML should give the default
TEST_F(SimServsTest, EmptyXml)
{
  SCOPED_TRACE("");
  string xml = "<simservs/>";
  simservs ss(xml);
  ss_values exp;
  expect_ss(exp, ss);
}

/// No simservs element, while an error, should silently give the default.
TEST_F(SimServsTest, MissingElement) {
  SCOPED_TRACE("");
  string xml = "<wrongservs><funstuff/></wrongservs>";
  simservs ss(xml);
  ss_values exp;
  expect_ss(exp, ss);
}

/// XML parse errors are illegal and should give the default, with a log message.
TEST_F(SimServsTest, InvalidXml1) {
  CapturingTestLogger log;
  SCOPED_TRACE("");
  string xml = "<blah";
  simservs ss(xml);
  ss_values exp;
  expect_ss(exp, ss);
  EXPECT_TRUE(log.contains("Parse error"));
}

TEST_F(SimServsTest, Typical)
{
  SCOPED_TRACE("");
  // This nastily-formatted XML is just the way it comes from
  // Ellis. We test it like this to be sure that we can parse it OK;
  // the other tests in this file use more naturally-formatted XML.
  string xml = "<simservs xmlns=\"http://uri.etsi.org/ngn/params/xml/simservs/xcap\" xmlns:cp=\"urn:ietf:params:xml:ns:common-policy\">\n"
               "  <originating-identity-presentation active=\"true\" />"
               "  <originating-identity-presentation-restriction active=\"true\">"
               "    <default-behaviour>presentation-not-restricted</default-behaviour>"
               "  </originating-identity-presentation-restriction>"
               "  <communication-diversion active=\"true\">"
               "    <NoReplyTimer>19</NoReplyTimer>"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions><busy /></cp:conditions>"
               "        <cp:actions><forward-to><target>sip:441316500818@cw-ngv.com</target></forward-to></cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </communication-diversion>"
               "  <incoming-communication-barring active=\"true\">"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions />"
               "        <cp:actions><allow>true</allow></cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </incoming-communication-barring>"
               "  <outgoing-communication-barring active=\"true\">"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions />"
               "        <cp:actions><allow>true</allow></cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </outgoing-communication-barring>"
               "</simservs>\n";
  simservs ss(xml);
  ss_values exp;
  exp.oip_enabled = true;
  exp.oir_enabled = true;
  exp.oir_presentation_restricted = false;
  exp.cdiv_enabled = true;
  exp.cdiv_no_reply_timer = 19;
  ss_cdiv_rule cdiv;
  cdiv.conditions = simservs::Rule::CONDITION_BUSY;
  cdiv.forward_target = "sip:441316500818@cw-ngv.com";
  exp.cdiv_rules.push_back(cdiv);
  exp.inbound_cb_enabled = true;
  ss_cb_rule cb;
  cb.conditions = 0u;
  cb.allow_call = true;
  exp.inbound_cb_rules.push_back(cb);
  exp.outbound_cb_enabled = true;
  exp.outbound_cb_rules.push_back(cb);
  expect_ss(exp, ss);
}

TEST_F(SimServsTest, Alternate)
{
  SCOPED_TRACE("");
  string xml = "<simservs xmlns=\"http://uri.etsi.org/ngn/params/xml/simservs/xcap\" xmlns:cp=\"urn:ietf:params:xml:ns:common-policy\">"
               "  <originating-identity-presentation active=\"false\" />"
               "  <originating-identity-presentation-restriction active=\"false\">"
               "    <default-behaviour>presentation-not-restricted</default-behaviour>"
               "  </originating-identity-presentation-restriction>"
               "  <communication-diversion xmlns=\"\" active=\"false\">"
               "    <NoReplyTimer>19</NoReplyTimer>"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions>"
               "          <busy />"
               "        </cp:conditions>"
               "        <cp:actions>"
               "          <forward-to>"
               "            <target>sip:441316500818@cw-ngv.com</target>"
               "          </forward-to>"
               "        </cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </communication-diversion>"
               "  <incoming-communication-barring xmlns=\"\" active=\"false\">"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions />"
               "        <cp:actions>"
               "          <allow>true</allow>"
               "        </cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </incoming-communication-barring>"
               "  <outgoing-communication-barring xmlns=\"\" active=\"true\">"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions />"
               "        <cp:actions>"
               "          <allow>true</allow>"
               "        </cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </outgoing-communication-barring>"
               "</simservs>";
  simservs ss(xml);
  ss_values exp;
  exp.oip_enabled = false;
  exp.oir_enabled = false;
  exp.oir_presentation_restricted = false;
  exp.cdiv_enabled = false;
  exp.inbound_cb_enabled = false;
  exp.outbound_cb_enabled = true;
  ss_cb_rule cb;
  cb.conditions = 0u;
  cb.allow_call = true;
  exp.outbound_cb_rules.push_back(cb);
  expect_ss(exp, ss);
}

TEST_F(SimServsTest, Alternate2)
{
  SCOPED_TRACE("");
  string xml = "<simservs xmlns=\"http://uri.etsi.org/ngn/params/xml/simservs/xcap\" xmlns:cp=\"urn:ietf:params:xml:ns:common-policy\">"
               "  <originating-identity-presentation active=\"false\" />"
               "  <originating-identity-presentation-restriction active=\"true\">"
               "    <default-behaviour>presentation-restricted</default-behaviour>"
               "  </originating-identity-presentation-restriction>"
               "  <communication-diversion xmlns=\"\" active=\"true\">"
               "    <NoReplyTimer>19</NoReplyTimer>"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions>"
               "          <media>video</media>"
               "          <international/>"
               "        </cp:conditions>"
               "        <cp:actions>"
               "          <forward-to>"
               "            <target>sip:441316500818@cw-ngv.com</target>"
               "          </forward-to>"
               "        </cp:actions>"
               "      </cp:rule>"
               "      <cp:rule id=\"rule2\">"
               "        <cp:conditions>"
               "          <not-reachable/>"
               "        </cp:conditions>"
               "        <cp:actions>"
               "          <forward-to>"
               "            <target>sip:441316500819@cw-ngv.com</target>"
               "          </forward-to>"
               "        </cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </communication-diversion>"
               "  <incoming-communication-barring xmlns=\"\" active=\"true\">"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions>"
               "          <no-answer/>"
               "          <not-registered/>"
               "        </cp:conditions>"
               "        <cp:actions>"
               "          <allow>true</allow>"
               "        </cp:actions>"
               "      </cp:rule>"
               "      <cp:rule id=\"rule2\">"
               "        <cp:conditions>"
               "          <media>audio</media>"
               "          <media>sub-etheric</media>"
               "        </cp:conditions>"
               "        <cp:actions>"
               "          <allow>false</allow>"
               "        </cp:actions>"
               "      </cp:rule>"
               "      <cp:rule id=\"rule3\">"
               "        <cp:conditions>"
               "          <roaming/>"
               "          <unknown-conds-ignored>even with stuff inside</unknown-conds-ignored>"
               "          <international-exHC/>"
               "        </cp:conditions>"
               "        <cp:actions>"
               "          <allow>true</allow>"
               "        </cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </incoming-communication-barring>"
               "  <outgoing-communication-barring xmlns=\"\" active=\"false\">"
               "    <cp:ruleset>"
               "      <cp:rule id=\"rule1\">"
               "        <cp:conditions />"
               "        <cp:actions>"
               "          <allow>true</allow>"
               "        </cp:actions>"
               "      </cp:rule>"
               "    </cp:ruleset>"
               "  </outgoing-communication-barring>"
               "</simservs>";
  simservs ss(xml);
  ss_values exp;
  exp.oip_enabled = false;
  exp.oir_enabled = true;
  exp.oir_presentation_restricted = true;
  exp.cdiv_enabled = true;
  exp.cdiv_no_reply_timer = 19;
  ss_cdiv_rule cdiv;
  cdiv.conditions = simservs::Rule::CONDITION_MEDIA_VIDEO | simservs::Rule::CONDITION_INTERNATIONAL;
  cdiv.forward_target = "sip:441316500818@cw-ngv.com";
  exp.cdiv_rules.push_back(cdiv);
  cdiv.conditions = simservs::Rule::CONDITION_NOT_REACHABLE;
  cdiv.forward_target = "sip:441316500819@cw-ngv.com";
  exp.cdiv_rules.push_back(cdiv);
  exp.inbound_cb_enabled = true;
  ss_cb_rule cb;
  cb.conditions = simservs::Rule::CONDITION_NO_ANSWER | simservs::Rule::CONDITION_NOT_REGISTERED;
  cb.allow_call = true;
  exp.inbound_cb_rules.push_back(cb);
  cb.conditions = simservs::Rule::CONDITION_MEDIA_AUDIO;  // unknown media types ignored
  cb.allow_call = false;
  exp.inbound_cb_rules.push_back(cb);
  // unknown conditions ignored
  cb.conditions = simservs::Rule::CONDITION_ROAMING | simservs::Rule::CONDITION_INTERNATIONAL_EXHC;
  cb.allow_call = true;
  exp.inbound_cb_rules.push_back(cb);
  exp.outbound_cb_enabled = false;
  expect_ss(exp, ss);
}

TEST_F(SimServsTest, CdivConstructor)
{
  std::string forward_target = "sip:1234567890@cw-ngv.com";
  simservs ss(forward_target, simservs::Rule::CONDITION_BUSY | simservs::Rule::CONDITION_NOT_REGISTERED, 21);
  ss_values exp;
  exp.oip_enabled = false;
  exp.oir_enabled = false;
  exp.oir_presentation_restricted = false;
  exp.cdiv_enabled = true;
  exp.cdiv_no_reply_timer = 21;
  ss_cdiv_rule cdiv;
  cdiv.forward_target = forward_target;
  cdiv.conditions = simservs::Rule::CONDITION_BUSY;
  exp.cdiv_rules.push_back(cdiv);
  cdiv.conditions = simservs::Rule::CONDITION_NOT_REGISTERED;
  exp.cdiv_rules.push_back(cdiv);
  exp.inbound_cb_enabled = false;
  exp.outbound_cb_enabled = false;
  expect_ss(exp, ss);
}

TEST_F(SimServsTest, CdivConstructorUnconditional)
{
  std::string forward_target = "sip:1234567891@cw-ngv.com";
  simservs ss(forward_target, 0, 22);
  ss_values exp;
  exp.oip_enabled = false;
  exp.oir_enabled = false;
  exp.oir_presentation_restricted = false;
  exp.cdiv_enabled = true;
  exp.cdiv_no_reply_timer = 22;
  ss_cdiv_rule cdiv;
  cdiv.forward_target = forward_target;
  cdiv.conditions = 0;
  exp.cdiv_rules.push_back(cdiv);
  exp.inbound_cb_enabled = false;
  exp.outbound_cb_enabled = false;
  expect_ss(exp, ss);
}
