#!/usr/bin/env python3
"""
Closest Facility POST API Wrapper for OSRM

This script provides a POST endpoint wrapper that accepts JSON payloads with
hundreds of facilities and thousands of query points, then converts them to
OSRM GET requests and returns concise results.

Usage:
    python closest_facility_post_wrapper.py --osrm-host localhost --osrm-port 4000 --port 8000

Then POST to: http://localhost:8000/closest_facility

JSON Payload Format:
{
  "facilities": [
    {"id": "hospital_a", "lon": 2.3522, "lat": 48.8566},
    {"id": "hospital_b", "lon": 2.3387, "lat": 48.8606}
  ],
  "query_points": [
    {"lon": 2.3200, "lat": 48.8400},
    {"lon": 2.3700, "lat": 48.8500}
  ],
  "annotations": "distance,duration"  // optional: "distance", "duration", or "distance,duration"
}

Response Format (concise):
{
  "code": "Ok",
  "results": [
    {
      "location": [2.320332, 48.839832],
      "distance": 4292.0,
      "duration": 736.3,
      "facility_id": "hospital_a"
    },
    ...
  ]
}
"""

import argparse
import json
import urllib.parse
import urllib.request
from http.server import HTTPServer, BaseHTTPRequestHandler
from typing import Dict, List, Any


class ClosestFacilityHandler(BaseHTTPRequestHandler):
    osrm_base_url = "http://localhost:4000"
    max_table_size = 100  # OSRM default limit (can be overridden)
    
    def do_POST(self):
        """Handle POST requests with JSON body"""
        if self.path != '/closest_facility':
            self.send_error(404, "Not Found")
            return
        
        try:
            # Read and parse JSON body
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length)
            data = json.loads(body.decode('utf-8'))
            
            # Validate required fields
            if 'facilities' not in data:
                self.send_json_response({
                    "code": "InvalidQuery",
                    "message": "Missing 'facilities' array"
                }, status=400)
                return
            
            if 'query_points' not in data:
                self.send_json_response({
                    "code": "InvalidQuery",
                    "message": "Missing 'query_points' array"
                }, status=400)
                return
            
            # Build OSRM GET URL
            facilities = data['facilities']
            query_points = data['query_points']
            annotations = data.get('annotations', 'distance,duration')
            
            # Validate facilities
            facility_ids = []
            for facility in facilities:
                if 'id' not in facility or 'lon' not in facility or 'lat' not in facility:
                    self.send_json_response({
                        "code": "InvalidQuery",
                        "message": "Each facility must have 'id', 'lon', and 'lat'"
                    }, status=400)
                    return
                facility_ids.append(facility['id'])
            
            # Validate query points
            for query in query_points:
                if 'lon' not in query or 'lat' not in query:
                    self.send_json_response({
                        "code": "InvalidQuery",
                        "message": "Each query_point must have 'lon' and 'lat'"
                    }, status=400)
                    return
            
            # Check if we need to batch
            num_facilities = len(facilities)
            num_queries = len(query_points)
            total_coords = num_facilities + num_queries
            
            # If total coordinates exceed the limit, we need to batch by queries
            # We keep all facilities in each batch (they're the destinations)
            # and split the query points across multiple batches
            if total_coords > self.max_table_size:
                print(f"Large request detected: {num_facilities} facilities × {num_queries} queries")
                print(f"Batching into multiple requests (limit: {self.max_table_size} coords per request)")
                self.send_json_response(self._process_batched_request(
                    facilities, query_points, annotations, facility_ids
                ))
                return
            
            # Small request - process normally
            coordinates = []
            
            # Add facilities
            for facility in facilities:
                coordinates.append(f"{facility['lon']},{facility['lat']}")
            
            # Add query points
            for query in query_points:
                coordinates.append(f"{query['lon']},{query['lat']}")
            
            # Build URL
            coords_str = ';'.join(coordinates)
            facility_ids_str = ','.join(facility_ids)
            
            osrm_url = (
                f"{self.osrm_base_url}/closest_facility/v1/car/{coords_str}"
                f"?facility_ids={facility_ids_str}&annotations={annotations}"
            )
            
            # Call OSRM
            with urllib.request.urlopen(osrm_url) as response:
                osrm_result = json.loads(response.read().decode('utf-8'))
            
            # Create concise response
            if osrm_result.get('code') != 'Ok':
                self.send_json_response(osrm_result, status=400)
                return
            
            concise_result = {
                "code": "Ok",
                "results": []
            }
            
            for result in osrm_result.get('results', []):
                concise_item = {
                    "location": result.get('location', {}).get('location', []),
                    "facility_id": result.get('closest_facility_id', '')
                }
                
                if 'distance' in result:
                    concise_item['distance'] = result['distance']
                if 'duration' in result:
                    concise_item['duration'] = result['duration']
                
                concise_result['results'].append(concise_item)
            
            self.send_json_response(concise_result)
            
        except json.JSONDecodeError as e:
            self.send_json_response({
                "code": "InvalidQuery",
                "message": f"Invalid JSON: {str(e)}"
            }, status=400)
        except Exception as e:
            self.send_json_response({
                "code": "Error",
                "message": str(e)
            }, status=500)
    
    def _process_batched_request(self, facilities, query_points, annotations, facility_ids):
        """
        Process large requests by batching query points
        All facilities are included in each batch
        """
        import time
        start_time = time.time()
        
        num_facilities = len(facilities)
        num_queries = len(query_points)
        
        # Calculate batch size: max_table_size - facilities = queries per batch
        queries_per_batch = self.max_table_size - num_facilities
        
        if queries_per_batch <= 0:
            return {
                "code": "Error",
                "message": f"Too many facilities ({num_facilities}). Maximum is {self.max_table_size - 1}"
            }
        
        print(f"Batching strategy:")
        print(f"  Facilities: {num_facilities} (in every batch)")
        print(f"  Query points: {num_queries}")
        print(f"  Queries per batch: {queries_per_batch}")
        print(f"  Total batches: {(num_queries + queries_per_batch - 1) // queries_per_batch}")
        
        all_results = []
        batch_num = 0
        
        # Process in batches
        for i in range(0, num_queries, queries_per_batch):
            batch_num += 1
            batch_queries = query_points[i:i + queries_per_batch]
            
            print(f"Processing batch {batch_num}: queries {i} to {i + len(batch_queries) - 1}")
            
            # Build coordinates for this batch
            coordinates = []
            
            # Add all facilities
            for facility in facilities:
                coordinates.append(f"{facility['lon']},{facility['lat']}")
            
            # Add this batch's query points
            for query in batch_queries:
                coordinates.append(f"{query['lon']},{query['lat']}")
            
            # Build URL
            coords_str = ';'.join(coordinates)
            facility_ids_str = ','.join(facility_ids)
            
            osrm_url = (
                f"{self.osrm_base_url}/closest_facility/v1/car/{coords_str}"
                f"?facility_ids={facility_ids_str}&annotations={annotations}"
            )
            
            try:
                # Call OSRM for this batch
                with urllib.request.urlopen(osrm_url) as response:
                    osrm_result = json.loads(response.read().decode('utf-8'))
                
                # Check for errors
                if osrm_result.get('code') != 'Ok':
                    return {
                        "code": "Error",
                        "message": f"Batch {batch_num} failed: {osrm_result.get('message', 'Unknown error')}"
                    }
                
                # Extract and add concise results from this batch
                for result in osrm_result.get('results', []):
                    concise_item = {
                        "location": result.get('location', {}).get('location', []),
                        "facility_id": result.get('closest_facility_id', '')
                    }
                    
                    if 'distance' in result:
                        concise_item['distance'] = result['distance']
                    if 'duration' in result:
                        concise_item['duration'] = result['duration']
                    
                    all_results.append(concise_item)
            
            except Exception as e:
                return {
                    "code": "Error",
                    "message": f"Batch {batch_num} failed: {str(e)}"
                }
        
        elapsed = time.time() - start_time
        print(f"✓ Batched request completed in {elapsed:.2f}s")
        print(f"  Total results: {len(all_results)}")
        print(f"  Average time per batch: {elapsed / batch_num:.2f}s")
        print(f"  Average time per query: {elapsed / num_queries * 1000:.1f}ms")
        
        return {
            "code": "Ok",
            "results": all_results,
            "metadata": {
                "total_queries": num_queries,
                "total_facilities": num_facilities,
                "batches_processed": batch_num,
                "processing_time_seconds": round(elapsed, 2)
            }
        }
    
    def do_GET(self):
        """Handle GET requests - return API documentation"""
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(b'''
            <html>
            <head><title>Closest Facility POST API</title></head>
            <body>
                <h1>Closest Facility POST API Wrapper</h1>
                <p>POST to /closest_facility with JSON body:</p>
                <pre>
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
                </pre>
                <p>Response will be concise:</p>
                <pre>
{
  "code": "Ok",
  "results": [
    {
      "location": [2.320332, 48.839832],
      "distance": 4292.0,
      "duration": 736.3,
      "facility_id": "facility_1"
    }
  ]
}
                </pre>
            </body>
            </html>
            ''')
        else:
            self.send_error(404)
    
    def send_json_response(self, data: Dict[str, Any], status: int = 200):
        """Send JSON response"""
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data, indent=2).encode('utf-8'))
    
    def log_message(self, format, *args):
        """Custom logging"""
        print(f"[{self.address_string()}] {format % args}")


def main():
    parser = argparse.ArgumentParser(description='Closest Facility POST API Wrapper')
    parser.add_argument('--osrm-host', default='localhost', help='OSRM server host')
    parser.add_argument('--osrm-port', type=int, default=4000, help='OSRM server port')
    parser.add_argument('--port', type=int, default=8000, help='Wrapper server port')
    parser.add_argument('--host', default='0.0.0.0', help='Wrapper server host')
    parser.add_argument('--max-table-size', type=int, default=100, 
                       help='Maximum OSRM table size (total coordinates per request)')
    
    args = parser.parse_args()
    
    # Set OSRM base URL and max table size
    ClosestFacilityHandler.osrm_base_url = f"http://{args.osrm_host}:{args.osrm_port}"
    ClosestFacilityHandler.max_table_size = args.max_table_size
    
    # Start server
    server = HTTPServer((args.host, args.port), ClosestFacilityHandler)
    print(f"Closest Facility POST API Wrapper running on http://{args.host}:{args.port}")
    print(f"Forwarding to OSRM at {ClosestFacilityHandler.osrm_base_url}")
    print(f"Max table size: {ClosestFacilityHandler.max_table_size} coordinates per request")
    print(f"Automatic batching enabled for large requests")
    print("Send POST requests to http://<host>:<port>/closest_facility")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == '__main__':
    main()
