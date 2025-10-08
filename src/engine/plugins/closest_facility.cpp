#include "engine/plugins/closest_facility.hpp"
#include "engine/api/closest_facility_api.hpp"
#include "engine/api/closest_facility_parameters.hpp"
#include "util/coordinate_calculation.hpp"

#include <cstdlib>
#include <vector>
#include <algorithm>

#include <boost/assert.hpp>

namespace osrm::engine::plugins
{

ClosestFacilityPlugin::ClosestFacilityPlugin(const int max_locations_distance_table,
                                             const std::optional<double> default_radius)
    : BasePlugin(default_radius), max_locations_distance_table(max_locations_distance_table)
{
}

Status ClosestFacilityPlugin::HandleRequest(const RoutingAlgorithmsInterface &algorithms,
                                           const api::ClosestFacilityParameters &params,
                                           osrm::engine::api::ResultT &result) const
{
    if (!algorithms.HasManyToManySearch())
    {
        return Error("NotImplemented",
                     "Many to many search is not implemented for the chosen search algorithm.",
                     result);
    }

    BOOST_ASSERT(params.IsValid());

    if (!CheckAllCoordinates(params.coordinates))
    {
        return Error("InvalidOptions", "Coordinates are invalid", result);
    }

    if (!params.bearings.empty() && params.coordinates.size() != params.bearings.size())
    {
        return Error("InvalidOptions",
                     "Number of bearings does not match number of coordinates",
                     result);
    }

    const auto num_facilities = params.facility_indices.size();
    const auto num_queries = params.query_indices.size();

    if (max_locations_distance_table > 0 &&
        ((num_facilities * num_queries) >
         static_cast<std::size_t>(max_locations_distance_table * max_locations_distance_table)))
    {
        return Error("TooBig", "Too many coordinates for closest facility calculation", result);
    }

    if (!CheckAlgorithms(params, algorithms, result))
        return Status::Error;

    const auto &facade = algorithms.GetFacade();
    auto phantom_nodes = GetPhantomNodes(facade, params);

    if (phantom_nodes.size() != params.coordinates.size())
    {
        return Error(
            "NoSegment", MissingPhantomErrorMessage(phantom_nodes, params.coordinates), result);
    }

    auto snapped_phantoms = SnapPhantomNodes(std::move(phantom_nodes));

    bool request_distance =
        params.annotations & api::ClosestFacilityParameters::AnnotationsType::Distance;
    bool request_duration =
        params.annotations & api::ClosestFacilityParameters::AnnotationsType::Duration;

    // We need to compute distances from each query point to all facilities
    // Use ManyToManySearch with queries as sources and facilities as destinations
    auto result_tables_pair = algorithms.ManyToManySearch(
        snapped_phantoms, params.query_indices, params.facility_indices, request_distance);

    if ((request_duration && result_tables_pair.first.empty()) ||
        (request_distance && result_tables_pair.second.empty()))
    {
        return Error("NoTable", "No distance/duration table found", result);
    }

    // Convert flat arrays to 2D structure for easier processing
    std::vector<std::vector<EdgeDistance>> distance_table(num_queries);
    std::vector<std::vector<EdgeDuration>> duration_table(num_queries);

    for (std::size_t q = 0; q < num_queries; ++q)
    {
        distance_table[q].reserve(num_facilities);
        duration_table[q].reserve(num_facilities);

        for (std::size_t f = 0; f < num_facilities; ++f)
        {
            const auto table_index = q * num_facilities + f;
            if (request_distance && table_index < result_tables_pair.second.size())
            {
                distance_table[q].push_back(result_tables_pair.second[table_index]);
            }
            if (request_duration && table_index < result_tables_pair.first.size())
            {
                duration_table[q].push_back(result_tables_pair.first[table_index]);
            }
        }
    }

    api::ClosestFacilityAPI closest_facility_api{facade, params};
    closest_facility_api.MakeResponse(distance_table, duration_table, snapped_phantoms, result);

    return Status::Ok;
}

} // namespace osrm::engine::plugins
