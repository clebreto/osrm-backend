# Quick Start: Closest Facility POST API

## Start Services (2 commands)

```bash
# Terminal 1: OSRM Server
./build/osrm-routed --algorithm mld -p 4000 ile-de-france-latest.osrm

# Terminal 2: POST Wrapper  
python3 scripts/closest_facility_post_wrapper.py --osrm-port 4000 --port 8000
```

## Test It (1 command)

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"facilities":[{"id":"h1","lon":2.35,"lat":48.85}],"query_points":[{"lon":2.33,"lat":48.84}],"annotations":"distance,duration"}' \
  http://localhost:8000/closest_facility | jq
```

## Your Use Case: Bulk Hospital Access

```python
import requests
import json

# Your data
facilities = []
for i, hospital in enumerate(your_hospitals):
    facilities.append({
        "id": hospital.id,
        "lon": hospital.longitude,
        "lat": hospital.latitude
    })

query_points = []
for patient in your_patients:
    query_points.append({
        "lon": patient.longitude,
        "lat": patient.latitude
    })

# Make request
response = requests.post(
    'http://localhost:8000/closest_facility',
    json={
        'facilities': facilities,
        'query_points': query_points,
        'annotations': 'distance,duration'
    }
)

# Process results
results = response.json()['results']
for i, result in enumerate(results):
    print(f"Patient {i}: Closest facility is {result['facility_id']} "
          f"at {result['distance']/1000:.1f}km ({result['duration']/60:.1f}min)")
```

## Response Format

```json
{
  "code": "Ok",
  "results": [
    {
      "location": [lon, lat],
      "distance": meters,
      "duration": seconds,
      "facility_id": "your_id"
    }
  ]
}
```

## Limits

- Default: 100 facilities + queries total
- Increase: `./build/osrm-routed --max-table-size 10000 ...`
- Recommended: Batch if >1000 queries

## GET vs POST

**Use GET for:**
- Testing/demos
- <20 facilities
- <100 queries

**Use POST for:**
- Production
- 100+ facilities
- 1000+ queries
- Smaller responses needed
