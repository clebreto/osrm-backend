# Large-Scale Test Results: 3000 Facilities × 10000 Query Points

## Test Configuration

- **OSRM Server**: Île-de-France data, MLD algorithm, port 4000
- **OSRM Max Table Size**: 5000 coordinates per request
- **POST Wrapper**: Port 8000, automatic batching enabled
- **Dataset**: 3000 facilities, 10000 query points (realistic Île-de-France distribution)
- **Total File Size**: 534 KB (0.52 MB)

## Performance Results

### Summary Statistics

```
Total facilities: 3000
Total query points: 10000
Total results: 10000
Batches processed: 5
Batches per query: 2000 queries per batch
Processing time: 133.59 - 150.79 seconds (~2.3-2.5 minutes)
Average time per batch: ~30 seconds
Average time per query: ~13-15 milliseconds
```

### Batching Strategy

With 3000 facilities and a limit of 5000 coordinates:
- Queries per batch = 5000 - 3000 = 2000 queries
- Number of batches = ceil(10000 / 2000) = 5 batches
- Each batch processes: 3000 facilities × 2000 queries

### Performance Breakdown

| Metric | Value |
|--------|-------|
| **Total Processing Time** | 133-151 seconds |
| **Time per Batch** | ~27-30 seconds |
| **Time per Query** | ~13-15 ms |
| **Queries per Second** | ~66-75 queries/sec |
| **Response Size** | ~1.5 MB (concise format) |

### Comparison with Single Request

If we tried to process all 3000×10000 in one request:
- Total coordinates needed: 13000
- Default OSRM limit: 100
- **Would fail without batching!**

With batching:
- ✅ Automatic splitting into manageable chunks
- ✅ Respects OSRM limits
- ✅ Returns complete results
- ✅ Includes metadata (batches, timing)

## Sample Results

```json
{
  "code": "Ok",
  "results": [
    {
      "location": [2.323884, 48.716902],
      "facility_id": "facility_0092",
      "distance": 1234.6,
      "duration": 142
    },
    {
      "location": [2.693862, 48.740611],
      "facility_id": "facility_2065",
      "distance": 2069.2,
      "duration": 140.5
    },
    ...
  ],
  "metadata": {
    "total_queries": 10000,
    "total_facilities": 3000,
    "batches_processed": 5,
    "processing_time_seconds": 133.59
  }
}
```

## Command Used

```bash
# Start OSRM with increased limits
./build/osrm-routed --algorithm mld -p 4000 --max-table-size 5000 ile-de-france-latest.osrm

# Start POST wrapper with matching limits
python3 scripts/closest_facility_post_wrapper.py --osrm-port 4000 --port 8000 --max-table-size 5000

# Generate test data
python3 scripts/generate_large_test.py

# Run the test
time curl -X POST -H "Content-Type: application/json" \
  -d @test_large_scale.json \
  http://localhost:8000/closest_facility | jq .
```

## Scalability Analysis

### Current Performance

| Facilities | Queries | Batches | Est. Time | Status |
|------------|---------|---------|-----------|--------|
| 10 | 50 | 1 | ~100 ms | ✅ Tested |
| 50 | 500 | 10 | ~1.2 sec | ✅ Tested |
| 500 | 2000 | N/A | ~5-10 sec | ✅ Tested |
| **3000** | **10000** | **5** | **~2.5 min** | **✅ Tested** |

### Extrapolation for Larger Datasets

| Facilities | Queries | Batches* | Est. Time | Notes |
|------------|---------|----------|-----------|-------|
| 3000 | 50000 | 25 | ~12 minutes | Linear scaling |
| 3000 | 100000 | 50 | ~25 minutes | Practical limit |
| 5000 | 10000 | N/A | N/A | Need limit ≥15000 |

*Assuming max-table-size=5000

### Optimization Opportunities

1. **Parallel Batching**: Process multiple batches simultaneously
   - Current: Sequential processing
   - Potential: 2-4x speedup with parallel requests
   
2. **Increased OSRM Limits**: Higher max-table-size
   - Current: 5000
   - Recommended: 10000 for fewer batches
   - Trade-off: More memory usage

3. **Caching**: Cache facility-to-facility distances
   - If facilities are static
   - Reuse across multiple queries
   - Significant speedup for repeated requests

4. **Regional Batching**: Batch by geographic proximity
   - Group nearby queries
   - Reduce routing computation
   - Better cache locality

## Conclusion

**✅ Successfully demonstrated:**
- Processing 3000 facilities with 10000 query points
- Automatic batching working seamlessly
- Consistent ~13-15 ms per query
- Complete results with metadata
- Production-ready for healthcare access analysis

**Key Learnings:**
- Batching is essential for large datasets
- OSRM limits must be configured appropriately
- Performance scales linearly with batch count
- 2-3 minutes is reasonable for 10K queries

**Production Recommendations:**
- Use `--max-table-size 5000-10000` for OSRM
- Configure wrapper with matching limits
- Consider parallel batching for >10K queries
- Monitor memory usage on OSRM server
