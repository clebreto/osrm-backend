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

#ifndef ENGINE_API_CLOSEST_FACILITY_PARAMETERS_HPP
#define ENGINE_API_CLOSEST_FACILITY_PARAMETERS_HPP

#include "engine/api/base_parameters.hpp"
#include "util/coordinate.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace osrm::engine::api
{

/**
 * Parameters specific to the OSRM Closest Facility service.
 *
 * Holds member attributes:
 *  - facility_ids: identifiers for each facility (e.g., hospital IDs)
 *  - facility_coordinates: coordinates of facilities (hospitals)
 *  - query_coordinates: coordinates of points to query (where to find the closest facility)
 *
 * The base coordinates field will contain all coordinates (facilities + queries) combined,
 * and facility_indices/query_indices will indicate which are which.
 */
struct ClosestFacilityParameters : public BaseParameters
{
    std::vector<std::string> facility_ids;
    std::vector<std::size_t> facility_indices;
    std::vector<std::size_t> query_indices;

    enum class AnnotationsType
    {
        Distance = 0x01,
        Duration = 0x02,
        All = Distance | Duration
    };

    AnnotationsType annotations = AnnotationsType::Distance;

    ClosestFacilityParameters() = default;

    template <typename... Args>
    ClosestFacilityParameters(std::vector<std::string> facility_ids_,
                             std::vector<std::size_t> facility_indices_,
                             std::vector<std::size_t> query_indices_,
                             Args &&...args_)
        : BaseParameters{std::forward<Args>(args_)...}, facility_ids{std::move(facility_ids_)},
          facility_indices{std::move(facility_indices_)}, query_indices{std::move(query_indices_)}
    {
    }

    bool IsValid() const
    {
        if (!BaseParameters::IsValid())
            return false;

        // Need at least one facility and one query point
        if (facility_indices.empty() || query_indices.empty())
            return false;

        // Number of facility IDs must match number of facility coordinates
        if (facility_ids.size() != facility_indices.size())
            return false;

        // All indices must be valid
        const auto not_in_range = [this](const std::size_t x) { return x >= coordinates.size(); };

        if (std::any_of(begin(facility_indices), end(facility_indices), not_in_range))
            return false;

        if (std::any_of(begin(query_indices), end(query_indices), not_in_range))
            return false;

        // Check for overlap between facility and query indices
        for (const auto &fac_idx : facility_indices)
        {
            if (std::find(query_indices.begin(), query_indices.end(), fac_idx) !=
                query_indices.end())
                return false;
        }

        return true;
    }
};

inline bool operator&(ClosestFacilityParameters::AnnotationsType lhs,
                     ClosestFacilityParameters::AnnotationsType rhs)
{
    return static_cast<bool>(
        static_cast<std::underlying_type_t<ClosestFacilityParameters::AnnotationsType>>(lhs) &
        static_cast<std::underlying_type_t<ClosestFacilityParameters::AnnotationsType>>(rhs));
}

inline ClosestFacilityParameters::AnnotationsType
operator|(ClosestFacilityParameters::AnnotationsType lhs,
          ClosestFacilityParameters::AnnotationsType rhs)
{
    return (ClosestFacilityParameters::AnnotationsType)(
        static_cast<std::underlying_type_t<ClosestFacilityParameters::AnnotationsType>>(lhs) |
        static_cast<std::underlying_type_t<ClosestFacilityParameters::AnnotationsType>>(rhs));
}

inline ClosestFacilityParameters::AnnotationsType &
operator|=(ClosestFacilityParameters::AnnotationsType &lhs,
           ClosestFacilityParameters::AnnotationsType rhs)
{
    return lhs = lhs | rhs;
}

} // namespace osrm::engine::api

#endif // ENGINE_API_CLOSEST_FACILITY_PARAMETERS_HPP
