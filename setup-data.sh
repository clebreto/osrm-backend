#!/bin/bash
set -e

# OSRM Data Setup Script for France
# Downloads and processes OSM data for France routing

REGION="france"
MAP_URL="http://download.geofabrik.de/europe/france-latest.osm.pbf"
MAP_FILE="${REGION}-latest.osm.pbf"
OSRM_FILE="${REGION}-latest.osrm"

echo "=========================================="
echo "OSRM Data Setup - France (Full)"
echo "=========================================="
echo ""
echo "⚠️  WARNING: This requires ~16GB RAM for processing"
echo "⚠️  Download size: ~4GB, Processing time: ~15-20 minutes"
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "❌ Error: build directory not found"
    echo "Please run 'pixi run osrm-build' first to compile OSRM"
    exit 1
fi

# Check if osrm-extract executable exists
if [ ! -f "build/osrm-extract" ]; then
    echo "❌ Error: osrm-extract not found in build/"
    echo "Please run 'pixi run osrm-build' first to compile OSRM"
    exit 1
fi

# Download map data if not exists
if [ ! -f "$MAP_FILE" ]; then
    echo "📥 Downloading France OSM data (~4GB)..."
    echo "Source: $MAP_URL"
    curl -L --progress-bar -o "$MAP_FILE" "$MAP_URL"
    echo "✅ Download complete!"
else
    echo "✅ Map file already exists: $MAP_FILE"
fi

# Extract road network
if [ ! -f "$OSRM_FILE" ]; then
    echo ""
    echo "🔧 Step 1/3: Extracting road network from OSM data..."
    echo "This will take 10-15 minutes..."
    echo "⚠️  Make sure you have at least 16GB free RAM"
    echo "ℹ️  If the process is killed, close other applications to free memory"
    
    if ! ./build/osrm-extract -p profiles/car.lua "$MAP_FILE"; then
        echo ""
        echo "❌ Extraction failed!"
        echo ""
        echo "This is usually caused by insufficient memory (needs ~16GB RAM)."
        echo "To fix this:"
        echo "  1. Use the lighter Île-de-France dataset: pixi run osrm-data-idf"
        echo "  2. Or use Docker instead: docker-compose up"
        echo "  3. Close memory-intensive applications and try again"
        exit 1
    fi
    echo "✅ Extraction complete!"
else
    echo "✅ OSRM file already exists, skipping extraction"
fi

# Partition the graph
if [ ! -f "${OSRM_FILE}.partition" ]; then
    echo ""
    echo "🔧 Step 2/3: Partitioning the graph..."
    echo "This will take 3-5 minutes..."
    ./build/osrm-partition "$OSRM_FILE"
    echo "✅ Partitioning complete!"
else
    echo "✅ Partition file already exists, skipping"
fi

# Customize for routing
if [ ! -f "${OSRM_FILE}.cells" ]; then
    echo ""
    echo "🔧 Step 3/3: Customizing for routing..."
    echo "This will take 2-3 minutes..."
    ./build/osrm-customize "$OSRM_FILE"
    echo "✅ Customization complete!"
else
    echo "✅ Customization files already exist, skipping"
fi

echo ""
echo "=========================================="
echo "✅ OSRM Data Setup Complete!"
echo "=========================================="
echo ""
echo "Generated files:"
ls -lh ${REGION}-latest.osrm* 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "To start the OSRM routing server, run:"
echo "  pixi run osrm-backend"
echo ""
echo "Or manually:"
echo "  ./build/osrm-routed --algorithm mld $OSRM_FILE -p 5000 --max-table-size 10000"
echo ""
