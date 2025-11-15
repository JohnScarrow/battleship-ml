# WebAssembly Integration Fixes

## Issues Fixed

### 1. Missing EXPORTED_FUNCTIONS in build.sh
**Problem:** The C functions weren't being explicitly exported by Emscripten.

**Fix:** Added `-s EXPORTED_FUNCTIONS='["_startTournament","_tickTournament","_isTournamentDone","_getBoardSnapshot","_getHeatmapSnapshot"]'` to the emcc command.

### 2. Missing Error Handling in JavaScript
**Problem:** No try/catch blocks, making debugging difficult.

**Fix:** Added error handling to:
- Module loading
- Function wrapping
- Game loop execution
- Board rendering

### 3. Potential Memory Access Issues
**Problem:** Float32Array creation from HEAPF32 might have alignment issues.

**Fix:** Ensured proper byte-aligned access using `new Float32Array(mod.HEAPF32.buffer, ptr, 100)`.

### 4. Missing Null Checks
**Problem:** `tickTournament()` might return null/empty strings.

**Fix:** Added `if (msg)` check before appending to log.

## Files Modified

1. **build.sh**
   - Added EXPORTED_FUNCTIONS list
   - Made executable with chmod +x

2. **index.html**
   - Added comprehensive error handling
   - Added WASM module load verification
   - Added try/catch in renderBoard()
   - Added try/catch in game loop
   - Added completion message when tournament ends

3. **README.md**
   - Added build instructions
   - Added deployment instructions
   - Added architecture documentation

## Testing

Three test files created:

1. **test.html** - Simple function call tests
2. **diagnostic.html** - Detailed step-by-step diagnostic
3. **index.html** - Full game demo

## How to Verify

```bash
# 1. Build
./build.sh

# 2. Serve
python3 -m http.server 8000

# 3. Test in browser
# Open: http://localhost:8000/diagnostic.html
# Then: http://localhost:8000/
```

## Expected Behavior

1. Canvas should show a 10×10 grid
2. Green squares = hits
3. Red squares = misses
4. Blue gradient overlay = AI probability heatmap
5. Log should show move-by-move play
6. Speed should ramp up over time (2 → 8 → 20 → 60 ticks/sec)

## Common Issues

### "Module is not defined"
- Ensure you're serving over HTTP (not file://)
- Check browser console for CORS errors

### Canvas is blank
- Check browser console for JavaScript errors
- Run diagnostic.html first to verify WASM loads

### No log messages
- Verify tickTournament() returns non-null strings
- Check that Tournament::tick() implementation is correct

## Next Steps

If issues persist:
1. Open browser DevTools (F12)
2. Check Console tab for errors
3. Check Network tab to verify .wasm file loads
4. Run diagnostic.html for detailed test output
