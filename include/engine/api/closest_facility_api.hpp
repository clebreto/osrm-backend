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

#ifndef ENGINE_API_CLOSEST_FACILITY_HPP
#define ENGINE_API_CLOSEST_FACILITY_HPP

#include "engine/api/base_api.hpp"
#include "engine/api/base_result.hpp"
#include "engine/api/closest_facility_parameters.hpp"
#include "engine/datafacade/datafacade_base.hpp"
#include "util/json_container.hpp"
#include "util/integer_range.hpp"

#include <boost/range/algorithm/transform.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace osrm::engine::api
{

class ClosestFacilityAPI final : public BaseAPI
{
  public:
    struct QueryResult
    {
        std::string closest_facility_id;
        double distance;
        double duration;
    };

    ClosestFacilityAPI(const datafacade::BaseDataFacade &facade_,
                      const ClosestFacilityParameters &parameters_)
        : BaseAPI(facade_, parameters_), parameters(parameters_)
    {
    }

    void MakeResponse(const std::vector<std::vector<EdgeDistance>> &distance_table,
                     const std::vector<std::vector<EdgeDuration>> &duration_table,
                     const std::vector<PhantomNodeCandidates> &candidates,
                     osrm::engine::api::ResultT &response) const
    {
        if (std::holds_alternative<flatbuffers::FlatBufferBuilder>(response))
        {
            auto &fb_result = std::get<flatbuffers::FlatBufferBuilder>(response);
            MakeResponse(distance_table, duration_table, candidates, fb_result);
        }
        else
        {
            auto &json_result = std::get<util::json::Object>(response);
            MakeResponse(distance_table, duration_table, candidates, json_result);
        }
    }

    void MakeResponse(const std::vector<std::vector<EdgeDistance>> & /*distance_table*/,
                     const std::vector<std::vector<EdgeDuration>> & /*duration_table*/,
                     const std::vector<PhantomNodeCandidates> & /*candidates*/,
                     flatbuffers::FlatBufferBuilder & /*fb_result*/) const
    {
        // Flatbuffer implementation can be added later if needed
        // For now, this is a placeholder
    }

    void MakeResponse(const std::vector<std::vector<EdgeDistance>> &distance_table,
                     const std::vector<std::vector<EdgeDuration>> &duration_table,
                     const std::vector<PhantomNodeCandidates> &candidates,
                     util::json::Object &response) const
    {
        util::json::Array results_array;
        results_array.values.reserve(parameters.query_indices.size());

        bool include_distance =
            parameters.annotations & ClosestFacilityParameters::AnnotationsType::Distance;
        bool include_duration =
            parameters.annotations & ClosestFacilityParameters::AnnotationsType::Duration;

        // For each query point
        for (std::size_t q = 0; q < parameters.query_indices.size(); ++q)
        {
            double min_distance = std::numeric_limits<double>::max();
            double min_duration = std::numeric_limits<double>::max();
            std::string closest_id;
            std::size_t closest_idx = 0;

            // Find the closest facility
            for (std::size_t f = 0; f < parameters.facility_indices.size(); ++f)
            {
                if (include_distance && q < distance_table.size() &&
                    f < distance_table[q].size())
                {
                    const auto distance = distance_table[q][f];
                    if (distance != INVALID_EDGE_DISTANCE)
                    {
                        const double dist_value = from_alias<double>(distance);
                        if (dist_value < min_distance)
                        {
                            min_distance = dist_value;
                            closest_id = parameters.facility_ids[f];
                            closest_idx = f;
                        }
                    }
                }

                if (include_duration && q < duration_table.size() && f < duration_table[q].size())
                {
                    const auto duration = duration_table[q][f];
                    if (duration != MAXIMAL_EDGE_DURATION && closest_idx == f)
                    {
                        const double dur_value = from_alias<double>(duration) / 10.0;
                        min_duration = dur_value;
                    }
                }
            }

            util::json::Object result_obj;
            result_obj.values["closest_facility_id"] = closest_id;

            if (include_distance && min_distance != std::numeric_limits<double>::max())
            {
                result_obj.values["distance"] =
                    util::json::Number(std::round(min_distance * 10) / 10.);
            }
            else if (include_distance)
            {
                result_obj.values["distance"] = util::json::Null();
            }

            if (include_duration && min_duration != std::numeric_limits<double>::max())
            {
                result_obj.values["duration"] = util::json::Number(min_duration);
            }
            else if (include_duration)
            {
                result_obj.values["duration"] = util::json::Null();
            }

            // Add query point location
            const auto query_idx = parameters.query_indices[q];
            result_obj.values["location"] = MakeWaypoint(candidates[query_idx]);

            results_array.values.push_back(util::json::Value{result_obj});
        }

        response.values.emplace("results", results_array);

        // Add facility information
        if (!parameters.skip_waypoints)
        {
            util::json::Array facilities_array;
            facilities_array.values.reserve(parameters.facility_indices.size());

            for (std::size_t f = 0; f < parameters.facility_indices.size(); ++f)
            {
                util::json::Object facility_obj;
                facility_obj.values["id"] = parameters.facility_ids[f];
                const auto fac_idx = parameters.facility_indices[f];
                facility_obj.values["location"] = MakeWaypoint(candidates[fac_idx]);
                facilities_array.values.push_back(util::json::Value{facility_obj});
            }

            response.values.emplace("facilities", facilities_array);
        }

        response.values.emplace("code", "Ok");
        auto data_timestamp = facade.GetTimestamp();
        if (!data_timestamp.empty())
        {
            response.values.emplace("data_version", data_timestamp);
        }
    }

  private:
    const ClosestFacilityParameters &parameters;
};

} // namespace osrm::engine::api

#endif // ENGINE_API_CLOSEST_FACILITY_HPP
