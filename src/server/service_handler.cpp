#include "server/service_handler.hpp"

#include "server/service/match_service.hpp"
#include "server/service/nearest_service.hpp"
#include "server/service/route_service.hpp"
#include "server/service/table_service.hpp"
#include "server/service/tile_service.hpp"
#include "server/service/trip_service.hpp"
#include "server/service/closest_facility_service.hpp"

#include "server/api/parsed_url.hpp"
#include "util/json_util.hpp"

namespace osrm::server
{
ServiceHandler::ServiceHandler(osrm::EngineConfig &config) : routing_machine(config)
{
    service_map["route"] = std::make_unique<service::RouteService>(routing_machine);
    service_map["table"] = std::make_unique<service::TableService>(routing_machine);
    service_map["nearest"] = std::make_unique<service::NearestService>(routing_machine);
    service_map["trip"] = std::make_unique<service::TripService>(routing_machine);
    service_map["match"] = std::make_unique<service::MatchService>(routing_machine);
    service_map["tile"] = std::make_unique<service::TileService>(routing_machine);
    service_map["closest_facility"] = std::make_unique<service::ClosestFacilityService>(routing_machine);
}

engine::Status ServiceHandler::RunQuery(api::ParsedURL parsed_url,
                                        osrm::engine::api::ResultT &result)
{
    const auto &service_iter = service_map.find(parsed_url.service);
    if (service_iter == service_map.end())
    {
        result = util::json::Object();
        auto &json_result = std::get<util::json::Object>(result);
        json_result.values["code"] = "InvalidService";
        json_result.values["message"] = "Service " + parsed_url.service + " not found!";
        return engine::Status::Error;
    }
    auto &service = service_iter->second;

    if (service->GetVersion() != parsed_url.version)
    {
        result = util::json::Object();
        auto &json_result = std::get<util::json::Object>(result);
        json_result.values["code"] = "InvalidVersion";
        json_result.values["message"] = "Service " + parsed_url.service + " not found!";
        return engine::Status::Error;
    }

    return service->RunQuery(parsed_url.prefix_length, parsed_url.query, result);
}

engine::Status ServiceHandler::RunQueryJSON(api::ParsedURL parsed_url,
                                           const std::string &json_body,
                                           osrm::engine::api::ResultT &result)
{
    const auto &service_iter = service_map.find(parsed_url.service);
    if (service_iter == service_map.end())
    {
        result = util::json::Object();
        auto &json_result = std::get<util::json::Object>(result);
        json_result.values["code"] = "InvalidService";
        json_result.values["message"] = "Service " + parsed_url.service + " not found!";
        return engine::Status::Error;
    }
    auto &service = service_iter->second;

    if (service->GetVersion() != parsed_url.version)
    {
        result = util::json::Object();
        auto &json_result = std::get<util::json::Object>(result);
        json_result.values["code"] = "InvalidVersion";
        json_result.values["message"] = "Service " + parsed_url.service + " not found!";
        return engine::Status::Error;
    }

    // For now, only closest_facility supports POST with JSON body
    if (parsed_url.service == "closest_facility")
    {
        auto *cf_service = dynamic_cast<service::ClosestFacilityService*>(service.get());
        if (cf_service)
        {
            return cf_service->RunQueryJSON(json_body, result);
        }
    }

    // Service doesn't support POST with JSON
    result = util::json::Object();
    auto &json_result = std::get<util::json::Object>(result);
    json_result.values["code"] = "InvalidRequest";
    json_result.values["message"] = "Service " + parsed_url.service + " does not support POST with JSON body";
    return engine::Status::Error;
}
} // namespace osrm::server
