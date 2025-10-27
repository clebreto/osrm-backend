#!/bin/bash
set -e

# OSRM Data Setup Script for Île-de-France
# Downloads and processes OSM data for Île-de-France region routing

REGION="ile-de-france"
MAP_URL="http://download.geofabrik.de/europe/france/ile-de-france-latest.osm.pbf"
MAP_FILE="${REGION}-latest.osm.pbf"
OSRM_FILE="${REGION}-latest.osrm"

echo "=========================================="
echo "OSRM Data Setup - Île-de-France"
echo "=========================================="
echo ""
echo "ℹ️  Lighter version for development"
echo "ℹ️  Download size: ~300MB, Processing time: ~2-3 minutes"
echo "ℹ️  RAM required: ~2GB"
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
    echo "📥 Downloading Île-de-France OSM data (~300MB)..."
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
    echo "This will take 1-2 minutes..."
    echo "ℹ️  If the process is killed, close other applications to free memory"
    
    if ! ./build/osrm-extract -p profiles/car.lua "$MAP_FILE"; then
        echo ""
        echo "❌ Extraction failed!"
        echo ""
        echo "This is usually caused by insufficient memory."
        echo "To fix this:"
        echo "  1. Close memory-intensive applications"
        echo "  2. Try again with: pixi run osrm-data-idf"
        echo "  3. Or use Docker instead: docker-compose up"
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
    echo "This will take ~30 seconds..."
    ./build/osrm-partition "$OSRM_FILE"
    echo "✅ Partitioning complete!"
else
    echo "✅ Partition file already exists, skipping"
fi

# Customize for routing
if [ ! -f "${OSRM_FILE}.cells" ]; then
    echo ""
    echo "🔧 Step 3/3: Customizing for routing..."
    echo "This will take ~30 seconds..."
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
echo "  pixi run osrm-backend-idf"
echo ""
echo "Or manually:"
echo "  ./build/osrm-routed --algorithm mld $OSRM_FILE -p 5000 --max-table-size 10000"
echo ""
