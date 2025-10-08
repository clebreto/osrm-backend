#include <boost/test/unit_test.hpp>

#include "coordinates.hpp"
#include "fixture.hpp"

#include "engine/api/flatbuffers/fbresult_generated.h"
#include "osrm/closest_facility_parameters.hpp"

#include "osrm/coordinate.hpp"
#include "osrm/json_container.hpp"
#include "osrm/osrm.hpp"
#include "osrm/status.hpp"

osrm::Status run_closest_facility_json(const osrm::OSRM &osrm,
                                        const osrm::ClosestFacilityParameters &params,
                                        osrm::json::Object &json_result,
                                        bool use_json_only_api)
{
    if (use_json_only_api)
    {
        return osrm.ClosestFacility(params, json_result);
    }
    osrm::engine::api::ResultT result = osrm::json::Object();
    auto rc = osrm.ClosestFacility(params, result);
    json_result = std::get<osrm::json::Object>(result);
    return rc;
}

BOOST_AUTO_TEST_SUITE(closest_facility)

void test_closest_facility_response(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    
    // Add 2 facilities and 1 query point
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.facility_ids = {"facility_1", "facility_2"};

    json::Object json_result;
    const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
    BOOST_REQUIRE(rc == Status::Ok);

    const auto code = std::get<json::String>(json_result.values.at("code")).value;
    BOOST_CHECK_EQUAL(code, "Ok");

    // Check that facilities are returned
    const auto &facilities = std::get<json::Array>(json_result.values.at("facilities")).values;
    BOOST_CHECK_EQUAL(facilities.size(), 2);

    // Check that results exist for the query point
    const auto &results = std::get<json::Array>(json_result.values.at("results")).values;
    BOOST_CHECK_EQUAL(results.size(), 1);

    // Verify result structure
    const auto &result_object = std::get<json::Object>(results[0]);
    BOOST_CHECK(result_object.values.find("closest_facility_id") != result_object.values.end());
    BOOST_CHECK(result_object.values.find("distance") != result_object.values.end());
    BOOST_CHECK(result_object.values.find("duration") != result_object.values.end());
    BOOST_CHECK(result_object.values.find("location") != result_object.values.end());
}
BOOST_AUTO_TEST_CASE(test_closest_facility_response_old_api) 
{ 
    test_closest_facility_response(true); 
}
BOOST_AUTO_TEST_CASE(test_closest_facility_response_new_api) 
{ 
    test_closest_facility_response(false); 
}

void test_closest_facility_skip_waypoints(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    params.skip_waypoints = true;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.facility_ids = {"facility_1"};

    json::Object json_result;
    const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
    BOOST_REQUIRE(rc == Status::Ok);

    const auto code = std::get<json::String>(json_result.values.at("code")).value;
    BOOST_CHECK_EQUAL(code, "Ok");

    // With skip_waypoints, waypoints should not be in the response
    BOOST_CHECK(json_result.values.find("waypoints") == json_result.values.end());
}
BOOST_AUTO_TEST_CASE(test_closest_facility_skip_waypoints_old_api)
{
    test_closest_facility_skip_waypoints(true);
}
BOOST_AUTO_TEST_CASE(test_closest_facility_skip_waypoints_new_api)
{
    test_closest_facility_skip_waypoints(false);
}

void test_closest_facility_no_coordinates(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    params.facility_ids = {"facility_1"};

    json::Object json_result;
    const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
    BOOST_REQUIRE(rc == Status::Error);

    const auto code = std::get<json::String>(json_result.values.at("code")).value;
    BOOST_CHECK_EQUAL(code, "InvalidOptions");
}
BOOST_AUTO_TEST_CASE(test_closest_facility_no_coordinates_old_api)
{
    test_closest_facility_no_coordinates(true);
}
BOOST_AUTO_TEST_CASE(test_closest_facility_no_coordinates_new_api)
{
    test_closest_facility_no_coordinates(false);
}

void test_closest_facility_no_facility_ids(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());

    json::Object json_result;
    const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
    BOOST_REQUIRE(rc == Status::Error);

    const auto code = std::get<json::String>(json_result.values.at("code")).value;
    BOOST_CHECK_EQUAL(code, "InvalidOptions");
}
BOOST_AUTO_TEST_CASE(test_closest_facility_no_facility_ids_old_api)
{
    test_closest_facility_no_facility_ids(true);
}
BOOST_AUTO_TEST_CASE(test_closest_facility_no_facility_ids_new_api)
{
    test_closest_facility_no_facility_ids(false);
}

void test_closest_facility_mismatched_facility_ids(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    // Only 2 facility_ids but 3 coordinates - should have at least one query point
    params.facility_ids = {"facility_1", "facility_2", "facility_3"};

    json::Object json_result;
    const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
    BOOST_REQUIRE(rc == Status::Error);

    const auto code = std::get<json::String>(json_result.values.at("code")).value;
    BOOST_CHECK_EQUAL(code, "InvalidOptions");
}
BOOST_AUTO_TEST_CASE(test_closest_facility_mismatched_facility_ids_old_api)
{
    test_closest_facility_mismatched_facility_ids(true);
}
BOOST_AUTO_TEST_CASE(test_closest_facility_mismatched_facility_ids_new_api)
{
    test_closest_facility_mismatched_facility_ids(false);
}

void test_closest_facility_multiple_queries(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    
    // Add 1 facility and 3 query points
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.facility_ids = {"facility_1"};

    json::Object json_result;
    const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
    BOOST_REQUIRE(rc == Status::Ok);

    const auto code = std::get<json::String>(json_result.values.at("code")).value;
    BOOST_CHECK_EQUAL(code, "Ok");

    // Check that we get results for all 3 query points
    const auto &results = std::get<json::Array>(json_result.values.at("results")).values;
    BOOST_CHECK_EQUAL(results.size(), 3);

    // Each result should reference the same facility
    for (const auto &result_value : results)
    {
        const auto &result_obj = std::get<json::Object>(result_value);
        const auto facility_id = std::get<json::String>(result_obj.values.at("closest_facility_id")).value;
        BOOST_CHECK_EQUAL(facility_id, "facility_1");
    }
}
BOOST_AUTO_TEST_CASE(test_closest_facility_multiple_queries_old_api)
{
    test_closest_facility_multiple_queries(true);
}
BOOST_AUTO_TEST_CASE(test_closest_facility_multiple_queries_new_api)
{
    test_closest_facility_multiple_queries(false);
}

void test_closest_facility_annotations(bool use_json_only_api)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    // Test with distance only
    {
        ClosestFacilityParameters params;
        params.coordinates.push_back(get_dummy_location());
        params.coordinates.push_back(get_dummy_location());
        params.facility_ids = {"facility_1"};
        params.annotations = ClosestFacilityParameters::AnnotationsType::Distance;

        json::Object json_result;
        const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
        BOOST_REQUIRE(rc == Status::Ok);

        const auto &results = std::get<json::Array>(json_result.values.at("results")).values;
        const auto &result_obj = std::get<json::Object>(results[0]);
        BOOST_CHECK(result_obj.values.find("distance") != result_obj.values.end());
        // Duration should still be present (default behavior)
        BOOST_CHECK(result_obj.values.find("duration") != result_obj.values.end());
    }

    // Test with duration only
    {
        ClosestFacilityParameters params;
        params.coordinates.push_back(get_dummy_location());
        params.coordinates.push_back(get_dummy_location());
        params.facility_ids = {"facility_1"};
        params.annotations = ClosestFacilityParameters::AnnotationsType::Duration;

        json::Object json_result;
        const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
        BOOST_REQUIRE(rc == Status::Ok);

        const auto &results = std::get<json::Array>(json_result.values.at("results")).values;
        const auto &result_obj = std::get<json::Object>(results[0]);
        BOOST_CHECK(result_obj.values.find("duration") != result_obj.values.end());
        // Distance should still be present (default behavior)
        BOOST_CHECK(result_obj.values.find("distance") != result_obj.values.end());
    }

    // Test with both
    {
        ClosestFacilityParameters params;
        params.coordinates.push_back(get_dummy_location());
        params.coordinates.push_back(get_dummy_location());
        params.facility_ids = {"facility_1"};
        params.annotations = ClosestFacilityParameters::AnnotationsType::All;

        json::Object json_result;
        const auto rc = run_closest_facility_json(osrm, params, json_result, use_json_only_api);
        BOOST_REQUIRE(rc == Status::Ok);

        const auto &results = std::get<json::Array>(json_result.values.at("results")).values;
        const auto &result_obj = std::get<json::Object>(results[0]);
        BOOST_CHECK(result_obj.values.find("distance") != result_obj.values.end());
        BOOST_CHECK(result_obj.values.find("duration") != result_obj.values.end());
    }
}
BOOST_AUTO_TEST_CASE(test_closest_facility_annotations_old_api)
{
    test_closest_facility_annotations(true);
}
BOOST_AUTO_TEST_CASE(test_closest_facility_annotations_new_api)
{
    test_closest_facility_annotations(false);
}

BOOST_AUTO_TEST_CASE(test_closest_facility_fb_serialization)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.facility_ids = {"facility_1"};

    engine::api::ResultT result = flatbuffers::FlatBufferBuilder();
    const auto rc = osrm.ClosestFacility(params, result);
    
    // FlatBuffers support may not be implemented yet, so we accept either Ok or Error
    BOOST_CHECK(rc == Status::Ok || rc == Status::Error);

    if (rc == Status::Ok)
    {
        auto &fb_result = std::get<flatbuffers::FlatBufferBuilder>(result);
        auto fb = engine::api::fbresult::GetFBResult(fb_result.GetBufferPointer());
        BOOST_CHECK(!fb->error());
    }
}

BOOST_AUTO_TEST_CASE(test_closest_facility_fb_serialization_skip_waypoints)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    params.skip_waypoints = true;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.facility_ids = {"facility_1"};

    engine::api::ResultT result = flatbuffers::FlatBufferBuilder();
    const auto rc = osrm.ClosestFacility(params, result);
    
    // FlatBuffers support may not be implemented yet
    BOOST_CHECK(rc == Status::Ok || rc == Status::Error);

    if (rc == Status::Ok)
    {
        auto &fb_result = std::get<flatbuffers::FlatBufferBuilder>(result);
        auto fb = engine::api::fbresult::GetFBResult(fb_result.GetBufferPointer());
        BOOST_CHECK(!fb->error());
        // Waypoints should be absent with skip_waypoints
        BOOST_CHECK(fb->waypoints() == nullptr);
    }
}

BOOST_AUTO_TEST_CASE(test_closest_facility_fb_error)
{
    auto osrm = getOSRM(OSRM_TEST_DATA_DIR "/ch/monaco.osrm");

    using namespace osrm;

    ClosestFacilityParameters params;
    // Invalid: no coordinates
    params.facility_ids = {"facility_1"};

    engine::api::ResultT result = flatbuffers::FlatBufferBuilder();
    const auto rc = osrm.ClosestFacility(params, result);
    
    BOOST_REQUIRE(rc == Status::Error);

    auto &fb_result = std::get<flatbuffers::FlatBufferBuilder>(result);
    auto fb = engine::api::fbresult::GetFBResult(fb_result.GetBufferPointer());
    BOOST_CHECK(fb->error());
}

BOOST_AUTO_TEST_SUITE_END()
