/*

Copyright (c) 2025, Project OSRM contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef CLOSEST_FACILITY_PARAMETERS_GRAMMAR_HPP
#define CLOSEST_FACILITY_PARAMETERS_GRAMMAR_HPP

#include "server/api/base_parameters_grammar.hpp"
#include "engine/api/closest_facility_parameters.hpp"

#include <boost/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

namespace osrm::server::api
{

namespace
{
namespace ph = boost::phoenix;
namespace qi = boost::spirit::qi;
} // namespace

template <typename Iterator = std::string::iterator,
          typename Signature = void(engine::api::ClosestFacilityParameters &)>
struct ClosestFacilityParametersGrammar : public BaseParametersGrammar<Iterator, Signature>
{
    using BaseGrammar = BaseParametersGrammar<Iterator, Signature>;

    ClosestFacilityParametersGrammar() : ClosestFacilityParametersGrammar(root_rule)
    {
        // Parse facility IDs: facility_ids=id1,id2,id3
        facility_id = qi::as_string[+(qi::char_ - ',' - '&')];
        facility_ids_rule =
            qi::lit("facility_ids=") >
            (facility_id % ',')[ph::bind(&engine::api::ClosestFacilityParameters::facility_ids,
                                        qi::_r1) = qi::_1];

        // The base query_rule from BaseGrammar will parse coordinates
        // We'll process them after parsing to split into facilities and queries
        
        facility_rule = facility_ids_rule(qi::_r1);

        root_rule = BaseGrammar::query_rule(qi::_r1) > BaseGrammar::format_rule(qi::_r1) >
                    -('?' > (facility_rule(qi::_r1) | base_rule(qi::_r1)) % '&');
    }

    ClosestFacilityParametersGrammar(qi::rule<Iterator, Signature> &root_rule_)
        : BaseGrammar(root_rule_)
    {
        using AnnotationsType = engine::api::ClosestFacilityParameters::AnnotationsType;

        annotations.add("distance", AnnotationsType::Distance)("duration",
                                                               AnnotationsType::Duration);

        annotations_list = annotations[qi::_val |= qi::_1] % ',';

        base_rule = BaseGrammar::base_rule(qi::_r1) |
                    (qi::lit("annotations=") >
                     annotations_list
                         [ph::bind(&engine::api::ClosestFacilityParameters::annotations, qi::_r1) =
                              qi::_1]);
    }

  protected:
    qi::rule<Iterator, Signature> base_rule;

  private:
    qi::rule<Iterator, Signature> root_rule;
    qi::rule<Iterator, Signature> facility_rule;
    qi::rule<Iterator, Signature> facility_ids_rule;
    qi::rule<Iterator, std::string()> facility_id;
    qi::symbols<char, engine::api::ClosestFacilityParameters::AnnotationsType> annotations;
    qi::rule<Iterator, engine::api::ClosestFacilityParameters::AnnotationsType()> annotations_list;
};

} // namespace osrm::server::api

#endif // CLOSEST_FACILITY_PARAMETERS_GRAMMAR_HPP
