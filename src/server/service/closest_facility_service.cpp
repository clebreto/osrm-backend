#include "server/service/closest_facility_service.hpp"

#include "server/api/parameters_parser.hpp"
#include "engine/api/closest_facility_parameters.hpp"

#include "util/json_container.hpp"
#include "util/json_util.hpp"
#include "util/log.hpp"

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <sstream>

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

// New: Handle POST requests with JSON body for bulk operations
engine::Status ClosestFacilityService::RunQueryJSON(const std::string &json_body,
                                                   osrm::engine::api::ResultT &result)
{
    result = util::json::Object();
    auto &json_result = std::get<util::json::Object>(result);

    util::Log(logDEBUG) << "[closest_facility] POST request, JSON body size: " << json_body.size() << " bytes";

    try
    {
        // Parse JSON body
        std::stringstream ss(json_body);
        boost::property_tree::ptree pt;
        
        util::Log(logDEBUG) << "[closest_facility] Parsing JSON...";
        boost::property_tree::read_json(ss, pt);
        util::Log(logDEBUG) << "[closest_facility] JSON parsed successfully";

        // Parse facilities array
        auto facilities_opt = pt.get_child_optional("facilities");
        if (!facilities_opt)
        {
            json_result.values["code"] = "InvalidQuery";
            json_result.values["message"] = "Missing 'facilities' array in JSON body";
            return engine::Status::Error;
        }

        std::vector<std::string> facility_ids;
        std::vector<util::FloatCoordinate> facility_coords;
        
        for (const auto &facility : *facilities_opt)
        {
            const auto &fac = facility.second;
            auto id = fac.get_optional<std::string>("id");
            auto lon = fac.get_optional<double>("lon");
            auto lat = fac.get_optional<double>("lat");

            if (!id || !lon || !lat)
            {
                json_result.values["code"] = "InvalidQuery";
                json_result.values["message"] =
                    "Each facility must have 'id', 'lon', and 'lat' fields";
                return engine::Status::Error;
            }

            facility_ids.push_back(*id);
            facility_coords.push_back(
                util::FloatCoordinate{util::FloatLongitude{*lon}, util::FloatLatitude{*lat}});
        }

        // Parse query_points array
        auto queries_opt = pt.get_child_optional("query_points");
        if (!queries_opt)
        {
            json_result.values["code"] = "InvalidQuery";
            json_result.values["message"] = "Missing 'query_points' array in JSON body";
            return engine::Status::Error;
        }

        std::vector<util::FloatCoordinate> query_coords;
        for (const auto &query_point : *queries_opt)
        {
            const auto &qp = query_point.second;
            auto lon = qp.get_optional<double>("lon");
            auto lat = qp.get_optional<double>("lat");

            if (!lon || !lat)
            {
                json_result.values["code"] = "InvalidQuery";
                json_result.values["message"] =
                    "Each query_point must have 'lon' and 'lat' fields";
                return engine::Status::Error;
            }

            query_coords.push_back(
                util::FloatCoordinate{util::FloatLongitude{*lon}, util::FloatLatitude{*lat}});
        }

        // Parse optional annotations
        engine::api::ClosestFacilityParameters::AnnotationsType annotations =
            engine::api::ClosestFacilityParameters::AnnotationsType::All;
        auto annotations_str = pt.get_optional<std::string>("annotations");
        if (annotations_str)
        {
            if (*annotations_str == "distance")
            {
                annotations = engine::api::ClosestFacilityParameters::AnnotationsType::Distance;
            }
            else if (*annotations_str == "duration")
            {
                annotations = engine::api::ClosestFacilityParameters::AnnotationsType::Duration;
            }
        }

        const std::size_t num_facilities = facility_coords.size();
        const std::size_t num_queries = query_coords.size();
        
        util::Log(logDEBUG) << "[closest_facility] Facilities: " << num_facilities 
                           << ", Queries: " << num_queries;
        
        // Get max table size from routing machine config
        // Typically max_table_size limits the total coordinates
        // For simplicity, we'll batch if num_facilities + num_queries exceeds a threshold
        // The actual limit check is: num_facilities * num_queries <= max_table_size^2
        // We'll use a conservative estimate: batch queries to keep under the limit
        
        // Assuming max_table_size = 5000, max_coords = 5000
        // We need: num_facilities * batch_size <= 5000^2
        // So: batch_size <= 5000^2 / num_facilities
        const std::size_t max_coords = 5000; // Should match server's --max-table-size
        const std::size_t max_cells = max_coords * max_coords;
        
        // Calculate how many queries we can process per batch
        std::size_t queries_per_batch = max_cells / num_facilities;
        if (queries_per_batch > num_queries)
        {
            queries_per_batch = num_queries;
        }
        
        util::Log(logDEBUG) << "[closest_facility] Cells needed: " << (num_facilities * num_queries)
                           << ", Max cells: " << max_cells
                           << ", Queries per batch: " << queries_per_batch;
        
        // If we can do it all in one batch, do so
        if (num_facilities * num_queries <= max_cells)
        {
            util::Log(logDEBUG) << "[closest_facility] Single batch processing";
            // Single batch processing
            engine::api::ClosestFacilityParameters parameters;
            parameters.annotations = annotations;
            
            // Add all facilities
            for (std::size_t i = 0; i < num_facilities; ++i)
            {
                parameters.facility_ids.push_back(facility_ids[i]);
                parameters.facility_indices.push_back(parameters.coordinates.size());
                parameters.coordinates.push_back(facility_coords[i]);
            }
            
            // Add all queries
            for (const auto &coord : query_coords)
            {
                parameters.query_indices.push_back(parameters.coordinates.size());
                parameters.coordinates.push_back(coord);
            }
            
            if (!parameters.IsValid())
            {
                json_result.values["code"] = "InvalidOptions";
                json_result.values["message"] = getWrongOptionHelp(parameters);
                return engine::Status::Error;
            }
            
            engine::api::ResultT engine_result;
            auto status = BaseService::routing_machine.ClosestFacility(parameters, engine_result);
            
            if (status != engine::Status::Ok)
            {
                if (std::holds_alternative<util::json::Object>(engine_result))
                {
                    json_result = std::get<util::json::Object>(engine_result);
                }
                return status;
            }
            
            auto &full_result = std::get<util::json::Object>(engine_result);
            json_result.values["code"] = "Ok";
            util::json::Array results_array;
            
            if (full_result.values.count("results"))
            {
                auto &results = std::get<util::json::Array>(full_result.values["results"]);
                for (const auto &res_val : results.values)
                {
                    const auto &res = std::get<util::json::Object>(res_val);
                    util::json::Object concise_result;
                    
                    if (res.values.count("location"))
                    {
                        const auto &loc = std::get<util::json::Object>(res.values.at("location"));
                        if (loc.values.count("location"))
                        {
                            concise_result.values["location"] = loc.values.at("location");
                        }
                    }
                    
                    if (res.values.count("distance"))
                    {
                        concise_result.values["distance"] = res.values.at("distance");
                    }
                    
                    if (res.values.count("duration"))
                    {
                        concise_result.values["duration"] = res.values.at("duration");
                    }
                    
                    if (res.values.count("closest_facility_id"))
                    {
                        concise_result.values["facility_id"] = res.values.at("closest_facility_id");
                    }
                    
                    results_array.values.push_back(concise_result);
                }
            }
            
            json_result.values["results"] = results_array;
            return engine::Status::Ok;
        }
        else
        {
            // Multi-batch processing
            util::Log(logDEBUG) << "[closest_facility] Multi-batch processing";
            util::json::Array all_results;
            std::size_t batch_count = 0;
            
            for (std::size_t query_offset = 0; query_offset < num_queries; query_offset += queries_per_batch)
            {
                std::size_t batch_query_count = std::min(queries_per_batch, num_queries - query_offset);
                
                util::Log(logDEBUG) << "[closest_facility] Processing batch " << (batch_count + 1) 
                                   << ": queries " << query_offset << " to " 
                                   << (query_offset + batch_query_count);
                
                engine::api::ClosestFacilityParameters parameters;
                parameters.annotations = annotations;
                
                // Add all facilities to each batch
                for (std::size_t i = 0; i < num_facilities; ++i)
                {
                    parameters.facility_ids.push_back(facility_ids[i]);
                    parameters.facility_indices.push_back(parameters.coordinates.size());
                    parameters.coordinates.push_back(facility_coords[i]);
                }
                
                // Add batch of queries
                for (std::size_t i = 0; i < batch_query_count; ++i)
                {
                    parameters.query_indices.push_back(parameters.coordinates.size());
                    parameters.coordinates.push_back(query_coords[query_offset + i]);
                }
                
                if (!parameters.IsValid())
                {
                    json_result.values["code"] = "InvalidOptions";
                    json_result.values["message"] = getWrongOptionHelp(parameters);
                    return engine::Status::Error;
                }
                
                engine::api::ResultT engine_result;
                auto status = BaseService::routing_machine.ClosestFacility(parameters, engine_result);
                
                if (status != engine::Status::Ok)
                {
                    if (std::holds_alternative<util::json::Object>(engine_result))
                    {
                        json_result = std::get<util::json::Object>(engine_result);
                    }
                    return status;
                }
                
                auto &batch_result = std::get<util::json::Object>(engine_result);
                if (batch_result.values.count("results"))
                {
                    auto &results = std::get<util::json::Array>(batch_result.values["results"]);
                    for (const auto &res_val : results.values)
                    {
                        const auto &res = std::get<util::json::Object>(res_val);
                        util::json::Object concise_result;
                        
                        if (res.values.count("location"))
                        {
                            const auto &loc = std::get<util::json::Object>(res.values.at("location"));
                            if (loc.values.count("location"))
                            {
                                concise_result.values["location"] = loc.values.at("location");
                            }
                        }
                        
                        if (res.values.count("distance"))
                        {
                            concise_result.values["distance"] = res.values.at("distance");
                        }
                        
                        if (res.values.count("duration"))
                        {
                            concise_result.values["duration"] = res.values.at("duration");
                        }
                        
                        if (res.values.count("closest_facility_id"))
                        {
                            concise_result.values["facility_id"] = res.values.at("closest_facility_id");
                        }
                        
                        all_results.values.push_back(concise_result);
                    }
                }
                
                batch_count++;
            }
            
            json_result.values["code"] = "Ok";
            json_result.values["results"] = all_results;
            
            // Add metadata about batching
            util::json::Object metadata;
            metadata.values["total_facilities"] = util::json::Number(num_facilities);
            metadata.values["total_queries"] = util::json::Number(num_queries);
            metadata.values["batches_processed"] = util::json::Number(batch_count);
            json_result.values["metadata"] = metadata;
            
            return engine::Status::Ok;
        }
    }
    catch (const boost::property_tree::json_parser_error &e)
    {
        json_result.values["code"] = "InvalidQuery";
        json_result.values["message"] = std::string("JSON parsing error: ") + e.what();
        return engine::Status::Error;
    }
    catch (const std::exception &e)
    {
        json_result.values["code"] = "InvalidQuery";
        json_result.values["message"] = std::string("Error processing request: ") + e.what();
        return engine::Status::Error;
    }
}

} // namespace osrm::server::service
