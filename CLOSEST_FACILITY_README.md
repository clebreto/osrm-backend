# Closest Facility API Implementation for OSRM

## Overview

This implementation adds a new `/closest_facility` API endpoint to OSRM-backend that finds the closest facility from a set of facilities for each query point. This is useful for scenarios like finding the nearest hospital, fire station, or service center from multiple locations.

## Files Added/Modified

### New Files Created

#### API Layer
- `include/engine/api/closest_facility_parameters.hpp` - Parameter structure for closest facility requests
- `include/engine/api/closest_facility_api.hpp` - API response formatting

#### Engine Layer  
- `include/engine/plugins/closest_facility.hpp` - Plugin interface
- `src/engine/plugins/closest_facility.cpp` - Core routing logic implementation

#### Server Layer
- `include/server/api/closest_facility_parameter_grammar.hpp` - URL parameter parsing grammar
- `include/server/service/closest_facility_service.hpp` - Service interface
- `src/server/service/closest_facility_service.cpp` - HTTP service implementation

#### Documentation
- `docs/closest_facility.md` - API documentation and usage examples
- `pixi.toml` - Pixi environment configuration for building

### Modified Files

- `include/osrm/osrm.hpp` - Added ClosestFacility method declarations
- `include/osrm/osrm_fwd.hpp` - Added forward declaration for ClosestFacilityParameters
- `src/osrm/osrm.cpp` - Implemented ClosestFacility methods
- `include/engine/engine.hpp` - Added ClosestFacility to engine interface and implementation
- `src/server/service_handler.cpp` - Registered the new service
- `src/server/api/parameters_parser.cpp` - Added parameter parsing support
- `CMakeLists.txt` - Fixed compilation warning for MICROTAR

## API Usage

### Request Format

```
GET /closest_facility/v1/{profile}/{coordinates}?facility_ids={ids}&annotations={annotations}
```

**Parameters:**
- `profile`: Travel mode (e.g., `car`, `foot`, `bicycle`)
- `coordinates`: Semicolon-separated list of `lon,lat` pairs
  - First N coordinates are facilities (N = number of facility_ids)
  - Remaining coordinates are query points
- `facility_ids` (required): Comma-separated facility identifiers
- `annotations` (optional): `distance`, `duration`, or `distance,duration` (default: `distance`)

### Example Request

```bash
# Example using Paris coordinates
curl "http://localhost:4000/closest_facility/v1/car/2.3522,48.8566;2.3387,48.8606;2.3488,48.8738;2.3200,48.8400;2.3700,48.8500?facility_ids=hospital_pitie_salpetriere,hospital_saint_louis,hospital_bichat&annotations=distance,duration"
```

This example:
- Uses 3 facilities (first 3 coordinates): Pitié-Salpêtrière, Saint-Louis, Bichat hospitals in Paris
- Finds the closest facility for 2 query points (last 2 coordinates)

### Response Format

```json
{
  "code": "Ok",
  "results": [
    {
      "location": {
        "name": "",
        "location": [13.385983, 52.496891]
      },
      "closest_facility_id": "hospital_a",
      "distance": 1234.5,
      "duration": 180.2
    }
  ],
  "facilities": [
    {
      "id": "hospital_a",
      "location": {
        "name": "",
        "location": [13.388860, 52.517037]
      }
    }
  ],
  "data_version": "..."
}
```

## Building with Pixi

### Prerequisites

Install Pixi (if not already installed):
```bash
curl -fsSL https://pixi.sh/install.sh | bash
```

### Build Steps

1. **Install dependencies:**
```bash
pixi install
```

2. **Configure the build:**
```bash
pixi run configure
```

3. **Build OSRM:**
```bash
pixi run build
```

The executables will be in the `build/` directory.

### Prepare Data

```bash
# Download OSM data (France - complete national coverage)
wget http://download.geofabrik.de/europe/france-latest.osm.pbf

# Extract routing data
./build/osrm-extract -p profiles/car.lua france-latest.osm.pbf

# Partition the graph (for MLD algorithm)
./build/osrm-partition france-latest.osrm

# Customize the graph
./build/osrm-customize france-latest.osrm
```

**Note:** Processing France takes more time and resources (~3.7 GB). For testing, you can use a smaller regional extract like:
- Monaco: `http://download.geofabrik.de/europe/monaco-latest.osm.pbf` (tiny, perfect for testing)
- Île-de-France (Paris): `http://download.geofabrik.de/europe/france/ile-de-france-latest.osm.pbf`
- Other regions: See http://download.geofabrik.de/europe/france.html

### Run the Server

```bash
./build/osrm-routed --algorithm mld -p 4000 ile-de-france-latest.osrm
```

The server will start on port 4000 (port 5000 is typically used by macOS AirPlay Receiver).

## API Usage - GET Endpoint (URL-based)

For small numbers of facilities and query points, use the GET endpoint.

## Testing the API

### Basic Test

```bash
# Test with 2 facilities and 1 query point
curl "http://localhost:4000/closest_facility/v1/car/13.388860,52.517037;13.397634,52.529407;13.385983,52.496891?facility_ids=facility_1,facility_2&annotations=distance,duration"
```

### Healthcare Access Analysis Example

```bash
# Find closest hospital from multiple locations in Paris
curl "http://localhost:4000/closest_facility/v1/car/2.3522,48.8566;2.3387,48.8606;2.3488,48.8738;2.3200,48.8400;2.3700,48.8500?facility_ids=hospital_pitie_salpetriere,hospital_saint_louis,hospital_bichat&annotations=distance"
```

### Emergency Services Example

```bash
# Find travel time from fire station to multiple emergency sites in Paris
curl "http://localhost:4000/closest_facility/v1/car/2.3522,48.8566;2.3387,48.8606;2.3488,48.8738?facility_ids=fire_station_paris_1&annotations=duration"
```

## API Usage - POST Endpoint (JSON-based for Bulk Operations)

**For large datasets with hundreds of facilities and thousands of query points**, use the POST endpoint with JSON payload.

### Starting the POST Wrapper

```bash
# Start the POST API wrapper (requires Python 3)
python3 scripts/closest_facility_post_wrapper.py --osrm-port 4000 --port 8000
```

### POST Request Format

```json
{
  "facilities": [
    {"id": "facility_1", "lon": 2.3522, "lat": 48.8566},
    {"id": "facility_2", "lon": 2.3387, "lat": 48.8606}
  ],
  "query_points": [
    {"lon": 2.3200, "lat": 48.8400},
    {"lon": 2.3700, "lat": 48.8500}
  ],
  "annotations": "distance,duration"
}
```

### POST Example

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{
    "facilities": [
      {"id": "hospital_pitie", "lon": 2.3522, "lat": 48.8566},
      {"id": "hospital_stlouis", "lon": 2.3387, "lat": 48.8606}
    ],
    "query_points": [
      {"lon": 2.3200, "lat": 48.8400},
      {"lon": 2.3700, "lat": 48.8500}
    ],
    "annotations": "distance,duration"
  }' \
  http://localhost:8000/closest_facility
```

### POST Response (Concise Format)

```json
{
  "code": "Ok",
  "results": [
    {
      "location": [2.320332, 48.839832],
      "distance": 4292.0,
      "duration": 736.3,
      "facility_id": "hospital_stlouis"
    },
    {
      "location": [2.369848, 48.849989],
      "distance": 2536.8,
      "duration": 434.0,
      "facility_id": "hospital_pitie"
    }
  ]
}
```

### GET vs POST Comparison

| Feature | GET Endpoint | POST Endpoint |
|---------|-------------|---------------|
| **Best for** | Small queries | Bulk operations |
| **Max practical size** | ~20 facilities, ~100 queries | 100+ facilities, 1000+ queries |
| **Request format** | URL parameters | JSON body |
| **Response format** | Full (with metadata) | Concise (minimal) |
| **Response size** | ~1 KB/result | ~150 bytes/result |

## Implementation Details

### Architecture

The implementation follows OSRM's plugin architecture:

1. **ClosestFacilityParameters**: Defines request parameters including facility IDs and coordinate indices
2. **ClosestFacilityAPI**: Formats the response with closest facility information
3. **ClosestFacilityPlugin**: Handles the routing logic using ManyToMany search
4. **ClosestFacilityService**: Processes HTTP requests and validates parameters

### Algorithm

The implementation uses OSRM's existing many-to-many routing algorithm:

1. Parse and validate input parameters
2. Snap coordinates to road network
3. Execute many-to-many search (queries as sources, facilities as destinations)
4. Find minimum distance/duration for each query point
5. Format and return results

### Performance

- Uses the same efficient many-to-many algorithm as the Table service
- Performance scales with the product of (number of query points × number of facilities)
- Respects `max_locations_distance_table` configuration limit

## Configuration Options

The service respects the following OSRM configuration options:

- `max_locations_distance_table`: Maximum allowed coordinates (default: 100×100)
- `default_radius`: Default search radius for snapping coordinates (meters)

## Error Handling

The API returns appropriate error codes:

- `InvalidQuery`: Malformed URL or parameters
- `InvalidOptions`: Invalid parameter combinations (e.g., mismatched facility_ids count)
- `NoSegment`: Cannot snap coordinates to road network
- `TooBig`: Too many coordinates for the configured limits
- `NotImplemented`: Service not available with current algorithm

## Future Enhancements

Potential improvements:

1. Add FlatBuffers support for binary response format
2. Support for additional routing preferences (avoid tolls, ferries, etc.)
3. Batch processing for very large facility sets
4. Caching of facility-to-facility matrices for repeated queries

## Testing

### Test Suite

Comprehensive tests have been added for the Closest Facility API at three levels:

#### 1. Engine-Level Tests (`unit_tests/engine/closest_facility_api.cpp`)

Tests parameter validation and structure:
- Parameter validation with various combinations
- Facility count verification
- Annotations type handling
- Skip waypoints functionality
- Out-of-range index detection
- Overlapping index detection
- Multiple facilities and queries scenarios

Run with:
```bash
cd build && make engine-tests && ./unit_tests/engine-tests
```

#### 2. Server-Level Tests (`unit_tests/server/parameters_parser.cpp`)

Tests URL parsing and parameter extraction:
- Invalid URL formats and coordinates
- Invalid parameter values (annotations, radiuses, bearings, etc.)
- Valid URL parsing with single/multiple facilities
- Annotation type parsing (distance, duration, both)
- Skip waypoints and generate_hints parameters

Run with:
```bash
cd build && make server-tests && ./unit_tests/server-tests
```

#### 3. Library-Level Integration Tests (`unit_tests/library/closest_facility.cpp`)

Tests the complete API flow with routing data:
- Basic closest facility response
- Skip waypoints functionality
- Missing/invalid parameters (no coordinates, no facility_ids, mismatches)
- Multiple facilities and query points
- Annotation filtering (distance, duration, all)
- FlatBuffers serialization support

*Note: Library tests require test data generation (see below)*

Run with:
```bash
cd build && make library-tests && ./unit_tests/library-tests
```

### Building Tests

Build all test executables:
```bash
cd build
make tests
```

This compiles all test suites but does not run them.

### Generating Test Data (for library tests)

Library tests require preprocessed OSM data. Generate it with:

```bash
cd test/data
make
```

This will download Monaco OSM data and preprocess it for testing.

### Running Individual Test Suites

After building, run specific test suites:

```bash
# Engine tests (parameter validation)
./unit_tests/engine-tests

# Server tests (URL parsing)
./unit_tests/server-tests

# Library tests (full integration - requires test data)
./unit_tests/library-tests
```

### Test Coverage

The test suite covers:
- ✅ Parameter validation and edge cases
- ✅ URL parsing and query string handling
- ✅ Invalid input rejection
- ✅ Multiple facilities and queries
- ✅ Annotation types
- ✅ Skip waypoints functionality
- ✅ FlatBuffers serialization
- ✅ Error handling

## Troubleshooting

### Build Issues

**Problem**: `TBB not found` error
**Solution**: Make sure `tbb-devel` is installed: `pixi install`

**Problem**: LTO compilation errors
**Solution**: The pixi.toml already disables LTO with `-DENABLE_LTO=OFF`

**Problem**: C compiler warnings with C++ flags
**Solution**: Already fixed in CMakeLists.txt with `target_no_warning(MICROTAR unused-command-line-argument)`

### Runtime Issues

**Problem**: `Service not found` error
**Solution**: Ensure the URL uses `/closest_facility/v1/...` with correct spelling

**Problem**: `InvalidOptions` - facility_ids mismatch
**Solution**: Number of facility_ids must match the number of facility coordinates (first N coordinates)

**Problem**: `NoSegment` errors
**Solution**: Coordinates may be too far from the road network. Try increasing the search radius or verifying coordinates.

## License

This implementation follows the same BSD 2-Clause License as OSRM-backend.

## Contact

For issues or questions, please refer to the main OSRM documentation or community channels.
