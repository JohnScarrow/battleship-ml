// tunerWorker.js - scaffold for running WASM-based tuning in a WebWorker
// Usage (from main thread):
// const worker = new Worker('tunerWorker.js', { type: 'module' });
// worker.postMessage({ cmd: 'start', params: { alphaRange: [0.65,0.85,0.05], placeRange: [1.0,2.0,0.5], ... } });
// worker will postMessage({ type: 'progress', pct, row, total }) and finally postMessage({ type: 'done', results })

self.onmessage = async (ev) => {
  const msg = ev.data;
  if (!msg || !msg.cmd) return;

  if (msg.cmd === 'start') {
    postMessage({ type: 'log', message: 'tunerWorker starting...' });
    try {
      // Load WASM module (ES6 module build)
      importScripts(); // no-op placeholder for compatibility
      const createModule = (await import('./dist/battleship.js')).default;
      const mod = await createModule({});

      // Wrap C functions
      const startTournament = mod.cwrap('startTournament', null, ['number','number']);
      const tickTournament   = mod.cwrap('tickTournament', 'string', []);
      const isTournamentDone = mod.cwrap('isTournamentDone', 'number', []);
      const setAIWeightsWasm = mod.cwrap('setAIWeightsFromArray', null, ['number']);
      const getAIWeightsWasm = mod.cwrap('getAIWeightsToArray', null, ['number']);

      // Helper to set AI weights via WASM array buffer
      function setWeights(obj) {
        const ptr = mod._malloc(16 * 4);
        try {
          // read existing
          getAIWeightsWasm(ptr);
          const view = new Float32Array(mod.HEAPF32.buffer, ptr, 16);
          // update fields provided in obj
          if (typeof obj.globalAlphaEarly === 'number') view[0] = obj.globalAlphaEarly;
          if (typeof obj.globalAlphaLate === 'number') view[1] = obj.globalAlphaLate;
          if (typeof obj.liveDecayFactor === 'number') view[2] = obj.liveDecayFactor;
          if (typeof obj.tacticalLiveBonus === 'number') view[3] = obj.tacticalLiveBonus;
          if (typeof obj.parityBonus === 'number') view[4] = obj.parityBonus;
          if (typeof obj.parityPenalty === 'number') view[5] = obj.parityPenalty;
          if (typeof obj.adjHitBonus === 'number') view[6] = obj.adjHitBonus;
          if (typeof obj.adjLineBonus === 'number') view[7] = obj.adjLineBonus;
          if (typeof obj.diagHitBonus === 'number') view[8] = obj.diagHitBonus;
          if (typeof obj.fitScoreNearAdjFactor === 'number') view[9] = obj.fitScoreNearAdjFactor;
          if (typeof obj.fitScoreBaseFactor === 'number') view[10] = obj.fitScoreBaseFactor;
          if (typeof obj.noFitPenalty === 'number') view[11] = obj.noFitPenalty;
          if (typeof obj.placementHitMultiplier === 'number') view[12] = obj.placementHitMultiplier;
          if (typeof obj.mcIterations === 'number') view[13] = obj.mcIterations;
          if (typeof obj.mcBlendRatio === 'number') view[14] = obj.mcBlendRatio;
          if (typeof obj.mcBlendThresholdCells === 'number') view[15] = obj.mcBlendThresholdCells;
          setAIWeightsWasm(ptr);
        } finally { mod._free(ptr); }
      }

      // parse range helper (start:step:end or single)
      function parseRange(spec, defStart, defStep, defEnd) {
        if (!spec) return [defStart];
        if (spec.indexOf(':') === -1) return [parseFloat(spec)];
        const parts = spec.split(':').map(Number);
        const out = [];
        for (let v = parts[0]; v <= parts[2] + 1e-9; v += parts[1]) out.push(Number(v.toFixed(6)));
        return out;
      }

      // read params
      const params = msg.params || {};
      const games = params.games || 200;
      const alphaRange = parseRange(params.alpha || '', 0.75, 0.05, 0.85);
      const placeRange = parseRange(params.place || '', 1.0, 0.5, 2.0);
      const adjRange = parseRange(params.adj || '', 0.2, 0.2, 0.6);
      const mcRange = parseRange(params.mc || '', 0.0, 0.5, 0.5);

      // iterate combos and run tournaments inside worker
      const totalCombos = alphaRange.length * placeRange.length * adjRange.length * mcRange.length;
      let comboIndex = 0;
      const results = [];

      for (const alpha of alphaRange) {
        for (const place of placeRange) {
          for (const adj of adjRange) {
            for (const mc of mcRange) {
              comboIndex++;
              // set weights
              setWeights({ globalAlphaEarly: alpha, placementHitMultiplier: place, adjHitBonus: adj, mcBlendRatio: mc });

              // start tournament
              startTournament(3, games);
              // tick until done
              let lastMsg = null;
              while (!isTournamentDone()) {
                const m = tickTournament();
                if (m) lastMsg = m;
              }
              // parse average shots from final message
              const fm = lastMsg || '';
              let p1avg = NaN, p2avg = NaN;
              try {
                const m1 = /P1 avg shots:\s*([0-9]+\.?[0-9]*)/.exec(fm);
                const m2 = /P2 avg shots:\s*([0-9]+\.?[0-9]*)/.exec(fm);
                if (m1) p1avg = parseFloat(m1[1]);
                if (m2) p2avg = parseFloat(m2[1]);
              } catch(e) {}

              results.push({ alpha, place, adj, mc, p1avg, p2avg });
              postMessage({ type: 'progress', comboIndex, totalCombos, last: { alpha, place, adj, mc, p1avg, p2avg } });
            }
          }
        }
      }

      postMessage({ type: 'done', results });
    } catch (err) {
      postMessage({ type: 'error', message: String(err) });
    }
  }
};
