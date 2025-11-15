#!/usr/bin/env bash
# Quick verification script

echo "🔍 Verifying Battleship ML WebAssembly build..."
echo ""

# Check if dist exists
if [ ! -d "dist" ]; then
  echo "❌ dist/ directory not found"
  echo "   Run: ./build.sh"
  exit 1
fi

# Check if wasm files exist
if [ ! -f "dist/battleship.js" ] || [ ! -f "dist/battleship.wasm" ]; then
  echo "❌ WASM files not found in dist/"
  echo "   Run: ./build.sh"
  exit 1
fi

echo "✅ WASM files found:"
ls -lh dist/battleship.*

echo ""
echo "🔍 Checking source files..."
REQUIRED_FILES=(
  "src/battleship.cpp"
  "src/battleship.h"
  "src/MLforAI.cpp"
  "src/MLforAI.h"
  "src/Tournament.cpp"
  "src/Tournament.h"
  "src/wasm_exports.cpp"
  "src/wasm_exports.h"
)

for file in "${REQUIRED_FILES[@]}"; do
  if [ ! -f "$file" ]; then
    echo "❌ Missing: $file"
    exit 1
  fi
done
echo "✅ All source files present"

echo ""
echo "🔍 Checking HTML files..."
HTML_FILES=("index.html" "test.html" "diagnostic.html")
for file in "${HTML_FILES[@]}"; do
  if [ ! -f "$file" ]; then
    echo "⚠️  Missing: $file"
  else
    echo "✅ Found: $file"
  fi
done

echo ""
echo "🔍 Verifying WASM exports..."
if grep -q "startTournament" dist/battleship.js && \
   grep -q "tickTournament" dist/battleship.js && \
   grep -q "getBoardSnapshot" dist/battleship.js; then
  echo "✅ All required functions exported"
else
  echo "❌ Some functions missing from exports"
  exit 1
fi

echo ""
echo "✅ Build verification complete!"
echo ""
echo "📝 Next steps:"
echo "   1. Start server: python3 -m http.server 8000"
echo "   2. Open browser: http://localhost:8000/diagnostic.html"
echo "   3. Then try: http://localhost:8000/"
echo ""
