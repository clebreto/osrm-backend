# Closest Facility API

## Overview

The Closest Facility service finds the closest facility from a set of facilities for each query point. This is useful for scenarios like:
- Finding the nearest hospital for multiple locations
- Finding the closest fire station to various emergency sites
- Determining the nearest service center for customer locations

## API Endpoint

```
GET /closest_facility/v1/{profile}/{coordinates}?facility_ids={ids}
```

## Parameters

### Path Parameters

- **profile**: The travel mode (e.g., `car`, `foot`, `bicycle`)
- **coordinates**: A semicolon-separated list of coordinates in the format `{longitude},{latitude}`
  - The first N coordinates (matching the number of facility_ids) are treated as facility locations
  - The remaining coordinates are treated as query points

### Query Parameters

- **facility_ids** (required): Comma-separated list of facility identifiers (e.g., `hospital1,hospital2,hospital3`)
  - The number of IDs must match the number of facility coordinates
- **annotations** (optional): What to include in the response
  - `distance`: Include distances (default)
  - `duration`: Include travel durations
  - `distance,duration`: Include both

## Request Format

The coordinates should be provided in the following order:
1. First N coordinates: Facility locations (N = number of facility_ids)
2. Remaining coordinates: Query points to find the closest facility for

### Example Request

```
GET /closest_facility/v1/car/13.388860,52.517037;13.397634,52.529407;13.428555,52.523219;13.385983,52.496891;13.409485,52.525339?facility_ids=hospital_a,hospital_b,hospital_c&annotations=distance,duration
```

In this example:
- First 3 coordinates are facilities (matching 3 facility_ids)
- Last 2 coordinates are query points

## Response Format

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
    },
    {
      "location": {
        "name": "",
        "location": [13.409485, 52.525339]
      },
      "closest_facility_id": "hospital_c",
      "distance": 890.3,
      "duration": 120.5
    }
  ],
  "facilities": [
    {
      "id": "hospital_a",
      "location": {
        "name": "",
        "location": [13.388860, 52.517037]
      }
    },
    {
      "id": "hospital_b",
      "location": {
        "name": "",
        "location": [13.397634, 52.529407]
      }
    },
    {
      "id": "hospital_c",
      "location": {
        "name": "",
        "location": [13.428555, 52.523219]
      }
    }
  ],
  "data_version": "..."
}
```

## Response Fields

### results (array)
Array of results for each query point, containing:

- **location**: The snapped location of the query point
  - **location**: [longitude, latitude] coordinate pair
  - **name**: Street name (if available)
  
- **closest_facility_id**: The ID of the closest facility
- **distance** (optional): Distance to the closest facility in meters
- **duration** (optional): Travel time to the closest facility in seconds

### facilities (array)
Information about each facility:

- **id**: The facility identifier provided in the request
- **location**: The snapped location of the facility
  - **location**: [longitude, latitude] coordinate pair
  - **name**: Street name (if available)

## Use Cases

### Healthcare Access Analysis
```bash
curl "http://localhost:5000/closest_facility/v1/car/13.388860,52.517037;13.397634,52.529407;13.385983,52.496891;13.409485,52.525339?facility_ids=hospital_1,hospital_2&annotations=distance,duration"
```

This finds the closest hospital from 2 hospitals for 2 query locations.

### Emergency Services Planning
```bash
curl "http://localhost:5000/closest_facility/v1/car/7.416351,43.731205;7.420363,43.736189;7.424525,43.740125?facility_ids=fire_station_1&annotations=duration"
```

This determines travel time from a single fire station to multiple emergency sites.

## Error Codes

- `InvalidQuery`: Malformed query string
- `InvalidOptions`: Invalid parameters (e.g., mismatched number of facility_ids and coordinates)
- `NoSegment`: Unable to snap coordinates to road network
- `TooBig`: Too many coordinates (exceeds `max_locations_distance_table` limit)
- `NotImplemented`: Service not available with the current routing algorithm

## Building with Closest Facility Support

The Closest Facility service is built into OSRM by default. No special compilation flags are needed.

To compile OSRM with pixi:

```bash
# Install pixi if you haven't already
# curl -fsSL https://pixi.sh/install.sh | bash

# Configure and build
pixi run configure
pixi run build

# The binaries will be in the build directory
./build/osrm-extract -p profiles/car.lua your-data.osm.pbf
./build/osrm-partition your-data.osrm
./build/osrm-customize your-data.osrm
./build/osrm-routed --algorithm mld your-data.osrm
```

## Implementation Notes

- The service uses the same many-to-many routing algorithm as the Table service
- Coordinates are snapped to the nearest road segment
- The algorithm finds the optimal route according to the selected profile
- Distance and duration calculations respect one-way streets, turn restrictions, and other routing constraints
