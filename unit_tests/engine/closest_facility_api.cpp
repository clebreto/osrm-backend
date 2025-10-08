#include "engine/api/closest_facility_api.hpp"
#include "engine/api/closest_facility_parameters.hpp"

#include <boost/test/unit_test.hpp>

// Disable printing for AnnotationsType enum to avoid compilation issues
BOOST_TEST_DONT_PRINT_LOG_VALUE(osrm::engine::api::ClosestFacilityParameters::AnnotationsType)

BOOST_AUTO_TEST_SUITE(closest_facility_api)

using namespace osrm::engine::api;

BOOST_AUTO_TEST_CASE(closest_facility_parameters_validation)
{
    ClosestFacilityParameters params;
    
    // Empty parameters should be invalid
    BOOST_CHECK(!params.IsValid());
    
    // Only facility_ids should be invalid
    params.facility_ids = {"facility_1"};
    BOOST_CHECK(!params.IsValid());
    
    // Only coordinates should be invalid
    params.facility_ids.clear();
    params.coordinates.push_back({osrm::util::FloatLongitude{1.0}, osrm::util::FloatLatitude{2.0}});
    BOOST_CHECK(!params.IsValid());
    
    // With coordinates and facility_ids but no indices - invalid
    params.facility_ids = {"facility_1"};
    BOOST_CHECK(!params.IsValid());
    
    // Add facility and query indices for proper setup
    params.facility_indices = {0};
    params.query_indices = {}; // No queries - invalid
    BOOST_CHECK(!params.IsValid());
    
    // Add a query coordinate and index
    params.coordinates.push_back({osrm::util::FloatLongitude{3.0}, osrm::util::FloatLatitude{4.0}});
    params.query_indices = {1};
    // Now should be valid: 1 facility, 1 query
    BOOST_CHECK(params.IsValid());
}

BOOST_AUTO_TEST_CASE(closest_facility_parameters_facility_count)
{
    ClosestFacilityParameters params;
    
    // Test facility_ids size
    BOOST_CHECK_EQUAL(params.facility_ids.size(), 0);
    
    params.facility_ids = {"f1"};
    BOOST_CHECK_EQUAL(params.facility_ids.size(), 1);
    
    params.facility_ids = {"f1", "f2", "f3"};
    BOOST_CHECK_EQUAL(params.facility_ids.size(), 3);
}

BOOST_AUTO_TEST_CASE(closest_facility_parameters_annotations)
{
    ClosestFacilityParameters params;
    
    // Default should be distance
    BOOST_CHECK(params.annotations == ClosestFacilityParameters::AnnotationsType::Distance);
    
    // Test setting annotations
    params.annotations = ClosestFacilityParameters::AnnotationsType::Duration;
    BOOST_CHECK(params.annotations == ClosestFacilityParameters::AnnotationsType::Duration);
    
    params.annotations = ClosestFacilityParameters::AnnotationsType::All;
    BOOST_CHECK(params.annotations == ClosestFacilityParameters::AnnotationsType::All);
}

BOOST_AUTO_TEST_CASE(closest_facility_parameters_skip_waypoints)
{
    ClosestFacilityParameters params;
    
    // Default should be false
    BOOST_CHECK_EQUAL(params.skip_waypoints, false);
    
    // Test setting skip_waypoints
    params.skip_waypoints = true;
    BOOST_CHECK_EQUAL(params.skip_waypoints, true);
}

BOOST_AUTO_TEST_CASE(closest_facility_empty_facility_ids)
{
    ClosestFacilityParameters params;
    
    params.coordinates.push_back({osrm::util::FloatLongitude{1.0}, osrm::util::FloatLatitude{2.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{3.0}, osrm::util::FloatLatitude{4.0}});
    params.query_indices = {0, 1};
    
    // Empty facility_ids should be invalid
    BOOST_CHECK(!params.IsValid());
}

BOOST_AUTO_TEST_CASE(closest_facility_mismatched_facility_ids)
{
    ClosestFacilityParameters params;
    
    // Add 3 coordinates
    params.coordinates.push_back({osrm::util::FloatLongitude{1.0}, osrm::util::FloatLatitude{2.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{3.0}, osrm::util::FloatLatitude{4.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{5.0}, osrm::util::FloatLatitude{6.0}});
    
    // Add 2 facility_ids but 3 facility_indices (mismatch)
    params.facility_ids = {"f1", "f2"};
    params.facility_indices = {0, 1, 2};
    
    // Should be invalid - facility_ids count doesn't match facility_indices count
    BOOST_CHECK(!params.IsValid());
}

BOOST_AUTO_TEST_CASE(closest_facility_out_of_range_indices)
{
    ClosestFacilityParameters params;
    
    // Add 2 coordinates
    params.coordinates.push_back({osrm::util::FloatLongitude{1.0}, osrm::util::FloatLatitude{2.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{3.0}, osrm::util::FloatLatitude{4.0}});
    
    params.facility_ids = {"f1"};
    params.facility_indices = {0};
    params.query_indices = {5}; // Out of range!
    
    // Should be invalid
    BOOST_CHECK(!params.IsValid());
}

BOOST_AUTO_TEST_CASE(closest_facility_overlapping_indices)
{
    ClosestFacilityParameters params;
    
    // Add 3 coordinates
    params.coordinates.push_back({osrm::util::FloatLongitude{1.0}, osrm::util::FloatLatitude{2.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{3.0}, osrm::util::FloatLatitude{4.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{5.0}, osrm::util::FloatLatitude{6.0}});
    
    params.facility_ids = {"f1"};
    params.facility_indices = {0};
    params.query_indices = {0, 1}; // Index 0 appears in both!
    
    // Should be invalid - can't use same coordinate as both facility and query
    BOOST_CHECK(!params.IsValid());
}

BOOST_AUTO_TEST_CASE(closest_facility_multiple_facilities_and_queries)
{
    ClosestFacilityParameters params;
    
    // Add 5 coordinates: 3 facilities + 2 queries
    params.coordinates.push_back({osrm::util::FloatLongitude{1.0}, osrm::util::FloatLatitude{2.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{3.0}, osrm::util::FloatLatitude{4.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{5.0}, osrm::util::FloatLatitude{6.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{7.0}, osrm::util::FloatLatitude{8.0}});
    params.coordinates.push_back({osrm::util::FloatLongitude{9.0}, osrm::util::FloatLatitude{10.0}});
    
    params.facility_ids = {"hospital_a", "hospital_b", "hospital_c"};
    params.facility_indices = {0, 1, 2};
    params.query_indices = {3, 4};
    
    // Should be valid
    BOOST_CHECK(params.IsValid());
    BOOST_CHECK_EQUAL(params.facility_ids.size(), 3);
    BOOST_CHECK_EQUAL(params.query_indices.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
