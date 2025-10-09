#include "server/service/closest_facility_service.hpp"

#include "server/api/parameters_parser.hpp"
#include "engine/api/closest_facility_parameters.hpp"

#include "util/json_container.hpp"
#include "util/json_util.hpp"

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <sstream>
#include <future>
#include <vector>
#include <thread>
#include <algorithm>
#include <atomic>
#include <mutex>

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

    try
    {
        // Parse JSON body
        std::stringstream ss(json_body);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        // Create parameters
        engine::api::ClosestFacilityParameters parameters;

        // Parse facilities array
        auto facilities_opt = pt.get_child_optional("facilities");
        if (!facilities_opt)
        {
            json_result.values["code"] = "InvalidQuery";
            json_result.values["message"] = "Missing 'facilities' array in JSON body";
            return engine::Status::Error;
        }

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

            parameters.facility_ids.push_back(*id);
            parameters.facility_indices.push_back(parameters.coordinates.size());
            parameters.coordinates.push_back(
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

            parameters.query_indices.push_back(parameters.coordinates.size());
            parameters.coordinates.push_back(
                util::FloatCoordinate{util::FloatLongitude{*lon}, util::FloatLatitude{*lat}});
        }

        // Parse optional annotations
        auto annotations_str = pt.get_optional<std::string>("annotations");
        if (annotations_str)
        {
            if (*annotations_str == "distance")
            {
                parameters.annotations = engine::api::ClosestFacilityParameters::AnnotationsType::Distance;
            }
            else if (*annotations_str == "duration")
            {
                parameters.annotations = engine::api::ClosestFacilityParameters::AnnotationsType::Duration;
            }
            else if (*annotations_str == "distance,duration" || *annotations_str == "duration,distance")
            {
                parameters.annotations = engine::api::ClosestFacilityParameters::AnnotationsType::All;
            }
        }

        // Validate parameters
        if (!parameters.IsValid())
        {
            json_result.values["code"] = "InvalidOptions";
            json_result.values["message"] = getWrongOptionHelp(parameters);
            return engine::Status::Error;
        }

        const auto num_facilities = parameters.facility_indices.size();
        const auto num_queries = parameters.query_indices.size();
        const auto total_coordinates = num_facilities + num_queries;
        
        // Reduce batch size for better parallelism
        // Original limit was 5000, but we use smaller batches for parallelism
        const std::size_t MAX_TABLE_SIZE = 5000;  // Must be > max expected facilities
        const std::size_t MAX_PARALLEL_BATCHES = std::min(std::thread::hardware_concurrency(), 8u);
        
        if (total_coordinates > MAX_TABLE_SIZE)
        {
            // Check if facilities alone exceed the limit
            if (num_facilities >= MAX_TABLE_SIZE)
            {
                json_result.values["code"] = "InvalidOptions";
                json_result.values["message"] = "Too many facilities (" + std::to_string(num_facilities) + 
                    ") to fit in table size limit (" + std::to_string(MAX_TABLE_SIZE) + ")";
                return engine::Status::Error;
            }
            
            // Need to batch queries (keep all facilities in each batch)
            const std::size_t queries_per_batch = MAX_TABLE_SIZE - num_facilities;
            
            // Calculate number of batches
            const std::size_t num_batches = (num_queries + queries_per_batch - 1) / queries_per_batch;
            
            // Process batches in parallel
            util::json::Array all_results;
            all_results.values.resize(num_queries);  // Pre-allocate for thread safety
            
            // Use a mutex-protected vector for batch results since util::json::Object is complex
            std::mutex results_mutex;
            std::vector<std::future<std::pair<engine::Status, engine::api::ResultT>>> futures;
            
            // Process batches in parallel groups
            for (std::size_t batch_start = 0; batch_start < num_batches; batch_start += MAX_PARALLEL_BATCHES)
            {
                futures.clear();
                const std::size_t batch_end = std::min(batch_start + MAX_PARALLEL_BATCHES, num_batches);
                
                // Launch parallel batch processing
                for (std::size_t batch_idx = batch_start; batch_idx < batch_end; ++batch_idx)
                {
                    const std::size_t query_offset = batch_idx * queries_per_batch;
                    const std::size_t batch_size = std::min(queries_per_batch, num_queries - query_offset);
                    
                    // Launch async batch processing - capture by value to avoid race conditions
                    futures.push_back(std::async(std::launch::async, 
                        [this, parameters, num_facilities, query_offset, batch_size, batch_idx]() 
                        -> std::pair<engine::Status, engine::api::ResultT> {
                        
                        try {
                            // Create batch parameters
                            engine::api::ClosestFacilityParameters batch_params;
                            batch_params.annotations = parameters.annotations;
                            
                            // Add all facilities
                            for (std::size_t i = 0; i < num_facilities; ++i)
                            {
                                const auto fac_idx = parameters.facility_indices[i];
                                batch_params.coordinates.push_back(parameters.coordinates[fac_idx]);
                                batch_params.facility_indices.push_back(i);
                                batch_params.facility_ids.push_back(parameters.facility_ids[i]);
                            }
                            
                            // Add batch of queries
                            for (std::size_t i = 0; i < batch_size; ++i)
                            {
                                const auto query_idx = parameters.query_indices[query_offset + i];
                                batch_params.coordinates.push_back(parameters.coordinates[query_idx]);
                                batch_params.query_indices.push_back(num_facilities + i);
                            }
                            
                            // Process this batch
                            engine::api::ResultT batch_result;
                            auto status = BaseService::routing_machine.ClosestFacility(batch_params, batch_result);
                            
                            return {status, std::move(batch_result)};
                            
                        } catch (const std::exception &e) {
                            util::json::Object error_obj;
                            error_obj.values["code"] = "Exception";
                            error_obj.values["message"] = std::string("Exception in batch ") + 
                                std::to_string(batch_idx + 1) + ": " + e.what();
                            return {engine::Status::Error, error_obj};
                        }
                    }));
                }
                
                // Wait for this group of batches to complete and collect results
                for (std::size_t i = 0; i < futures.size(); ++i)
                {
                    const std::size_t batch_idx = batch_start + i;
                    const std::size_t query_offset = batch_idx * queries_per_batch;
                    const std::size_t batch_size = std::min(queries_per_batch, num_queries - query_offset);
                    
                    auto [status, batch_result] = futures[i].get();
                    
                    if (status != engine::Status::Ok)
                    {
                        std::string error_msg = "Error processing batch " + std::to_string(batch_idx + 1);
                        if (std::holds_alternative<util::json::Object>(batch_result))
                        {
                            auto &batch_obj = std::get<util::json::Object>(batch_result);
                            if (batch_obj.values.count("message"))
                            {
                                const auto &msg_value = batch_obj.values.at("message");
                                if (std::holds_alternative<util::json::String>(msg_value))
                                {
                                    error_msg += ": " + std::get<util::json::String>(msg_value).value;
                                }
                            }
                        }
                        json_result.values["code"] = "BatchError";
                        json_result.values["message"] = error_msg;
                        return engine::Status::Error;
                    }
                    
                    // Extract results from batch_result and add to all_results
                    // Apply the same concise format transformation as non-batched path
                    if (std::holds_alternative<util::json::Object>(batch_result))
                    {
                        auto &batch_obj = std::get<util::json::Object>(batch_result);
                        if (batch_obj.values.count("results"))
                        {
                            auto &batch_results = batch_obj.values.at("results");
                            if (std::holds_alternative<util::json::Array>(batch_results))
                            {
                                auto &batch_array = std::get<util::json::Array>(batch_results);
                                std::size_t result_idx = 0;
                                for (const auto &res_val : batch_array.values)
                                {
                                    if (result_idx < batch_size)
                                    {
                                        // Transform to concise format (same as non-batched path)
                                        const auto &res = std::get<util::json::Object>(res_val);
                                        util::json::Object concise_result;

                                        // Extract location
                                        if (res.values.count("location"))
                                        {
                                            const auto &loc = std::get<util::json::Object>(res.values.at("location"));
                                            if (loc.values.count("location"))
                                            {
                                                concise_result.values["location"] = loc.values.at("location");
                                            }
                                        }

                                        // Extract distance
                                        if (res.values.count("distance"))
                                        {
                                            concise_result.values["distance"] = res.values.at("distance");
                                        }

                                        // Extract duration
                                        if (res.values.count("duration"))
                                        {
                                            concise_result.values["duration"] = res.values.at("duration");
                                        }

                                        // Extract facility_id (rename from closest_facility_id)
                                        if (res.values.count("closest_facility_id"))
                                        {
                                            concise_result.values["facility_id"] = res.values.at("closest_facility_id");
                                        }

                                        all_results.values[query_offset + result_idx] = concise_result;
                                        ++result_idx;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Format final response
            json_result.values["code"] = "Ok";
            json_result.values["results"] = all_results;
            
            // Add metadata about batching
            util::json::Object metadata;
            metadata.values["total_facilities"] = util::json::Number(num_facilities);
            metadata.values["total_queries"] = util::json::Number(num_queries);
            metadata.values["batches_processed"] = util::json::Number(num_batches);
            metadata.values["batch_size"] = util::json::Number(queries_per_batch);
            metadata.values["parallel_batches"] = util::json::Number(MAX_PARALLEL_BATCHES);
            json_result.values["metadata"] = metadata;
            
            return engine::Status::Ok;
        }

        // No batching needed - process normally
        // Call the routing engine
        engine::api::ResultT engine_result;
        auto status = BaseService::routing_machine.ClosestFacility(parameters, engine_result);

        if (status != engine::Status::Ok)
        {
            // Copy error result - extract the object and reassign
            if (std::holds_alternative<util::json::Object>(engine_result))
            {
                json_result = std::get<util::json::Object>(engine_result);
            }
            return status;
        }

        // Extract the full result
        auto &full_result = std::get<util::json::Object>(engine_result);
        
        // Create concise response format
        json_result.values["code"] = "Ok";
        util::json::Array results_array;

        if (full_result.values.count("results"))
        {
            auto &results = std::get<util::json::Array>(full_result.values["results"]);
            
            for (const auto &res_val : results.values)
            {
                const auto &res = std::get<util::json::Object>(res_val);
                util::json::Object concise_result;

                // Extract location
                if (res.values.count("location"))
                {
                    const auto &loc = std::get<util::json::Object>(res.values.at("location"));
                    if (loc.values.count("location"))
                    {
                        concise_result.values["location"] = loc.values.at("location");
                    }
                }

                // Extract distance (if available)
                if (res.values.count("distance"))
                {
                    concise_result.values["distance"] = res.values.at("distance");
                }

                // Extract duration (if available)
                if (res.values.count("duration"))
                {
                    concise_result.values["duration"] = res.values.at("duration");
                }

                // Extract facility_id
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
