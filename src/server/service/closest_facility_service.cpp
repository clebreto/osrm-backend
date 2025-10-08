#include "server/service/closest_facility_service.hpp"

#include "server/api/parameters_parser.hpp"
#include "engine/api/closest_facility_parameters.hpp"

#include "util/json_container.hpp"

#include <boost/format.hpp>

namespace osrm::server::service
{

namespace
{

const constexpr char PARAMETER_SIZE_MISMATCH_MSG[] =
    "Number of elements in %1% size %2% does not match coordinate size %3%";

template <typename ParamT>
bool constrainParamSize(const char *msg_template,
                       const char *name,
                       const ParamT &param,
                       const std::size_t target_size,
                       std::string &help)
{
    if (param.size() > 0 && param.size() != target_size)
    {
        help = (boost::format(msg_template) % name % param.size() % target_size).str();
        return true;
    }
    return false;
}

std::string getWrongOptionHelp(const engine::api::ClosestFacilityParameters &parameters)
{
    std::string help;

    const auto coord_size = parameters.coordinates.size();

    const bool param_size_mismatch =
        constrainParamSize(
            PARAMETER_SIZE_MISMATCH_MSG, "hints", parameters.hints, coord_size, help) ||
        constrainParamSize(
            PARAMETER_SIZE_MISMATCH_MSG, "bearings", parameters.bearings, coord_size, help) ||
        constrainParamSize(
            PARAMETER_SIZE_MISMATCH_MSG, "radiuses", parameters.radiuses, coord_size, help) ||
        constrainParamSize(
            PARAMETER_SIZE_MISMATCH_MSG, "approaches", parameters.approaches, coord_size, help);

    if (!param_size_mismatch)
    {
        if (parameters.facility_indices.empty())
        {
            help = "At least one facility coordinate is required.";
        }
        else if (parameters.query_indices.empty())
        {
            help = "At least one query coordinate is required.";
        }
        else if (parameters.facility_ids.size() != parameters.facility_indices.size())
        {
            help = "Number of facility IDs must match number of facility coordinates.";
        }
    }

    return help;
}

// Helper function to parse the coordinates and split them into facilities and queries
void processCoordinates(engine::api::ClosestFacilityParameters &parameters)
{
    // Expected format:
    // /closest_facility/v1/{profile}/{coordinates}?facility_ids=id1,id2,...
    // First N coordinates (matching number of facility_ids) are facilities
    // Remaining coordinates are query points
    
    const auto num_facilities = parameters.facility_ids.size();
    const auto total_coords = parameters.coordinates.size();
    
    if (num_facilities > total_coords)
    {
        return; // Invalid - will be caught by IsValid()
    }
    
    // First num_facilities coordinates are facilities
    parameters.facility_indices.clear();
    for (std::size_t i = 0; i < num_facilities; ++i)
    {
        parameters.facility_indices.push_back(i);
    }
    
    // Remaining coordinates are query points
    parameters.query_indices.clear();
    for (std::size_t i = num_facilities; i < total_coords; ++i)
    {
        parameters.query_indices.push_back(i);
    }
}

} // namespace

engine::Status ClosestFacilityService::RunQuery(std::size_t prefix_length,
                                               std::string &query,
                                               osrm::engine::api::ResultT &result)
{
    result = util::json::Object();
    auto &json_result = std::get<util::json::Object>(result);

    auto query_iterator = query.begin();
    auto parameters =
        api::parseParameters<engine::api::ClosestFacilityParameters>(query_iterator, query.end());
    if (!parameters || query_iterator != query.end())
    {
        const auto position = std::distance(query.begin(), query_iterator);
        json_result.values["code"] = "InvalidQuery";
        json_result.values["message"] =
            "Query string malformed close to position " + std::to_string(prefix_length + position);
        return engine::Status::Error;
    }
    BOOST_ASSERT(parameters);

    // Process coordinates to separate facilities from queries
    processCoordinates(*parameters);

    if (!parameters->IsValid())
    {
        json_result.values["code"] = "InvalidOptions";
        json_result.values["message"] = getWrongOptionHelp(*parameters);
        return engine::Status::Error;
    }
    BOOST_ASSERT(parameters->IsValid());

    if (parameters->format)
    {
        if (parameters->format == engine::api::BaseParameters::OutputFormatType::FLATBUFFERS)
        {
            result = flatbuffers::FlatBufferBuilder();
        }
    }
    
    return BaseService::routing_machine.ClosestFacility(*parameters, result);
}

} // namespace osrm::server::service
