// Simple browser fallback Battleship game so users can play without WASM exports.
(function(){
  class FallbackGame {
    constructor(size=10){
      this.size = size;
      this.shipLengths = [5,4,3,3,2];
      this.reset();
    }
    reset(){
      this.playerBoard = new Int8Array(this.size*this.size).fill(0); // player's ships (AI shoots here)
      this.opponentBoard = new Int8Array(this.size*this.size).fill(0); // opponent's ships (player shoots here)
      this.playerView = new Int8Array(this.size*this.size).fill(0); // what opponent has hit/missed on player
      this.opponentView = new Int8Array(this.size*this.size).fill(0); // player's hits/misses on opponent
      this.h1 = new Float32Array(this.size*this.size).fill(0); // heatmaps
      this.h2 = new Float32Array(this.size*this.size).fill(0);
      this.playerTurn = true; // player starts
      this.placeShipsRandom(this.playerBoard);
      this.placeShipsRandom(this.opponentBoard);
      this.opponentShots = []; // list of indices shot by AI
      this.lastSunkCountP = 0;
    }
    placeShipsRandom(board){
      const s = this.size;
      const placeOne = (len)=>{
        for (let attempt=0; attempt<200; attempt++){
          const horiz = Math.random() < 0.5;
          const r = Math.floor(Math.random()*s);
          const c = Math.floor(Math.random()*s);
          let ok = true;
          const coords = [];
          for (let i=0;i<len;i++){
            const rr = r + (horiz?0:i);
            const cc = c + (horiz?i:0);
            if (rr<0||rr>=s||cc<0||cc>=s){ ok=false; break; }
            const idx = rr*s + cc;
            if (board[idx] !== 0) { ok=false; break; }
            coords.push(idx);
          }
          if (!ok) continue;
          for (const idx of coords) board[idx] = 2; // ship present, untouched
          return true;
        }
        return false;
      };
      for (const L of this.shipLengths) placeOne(L);
    }
    // returns snapshots usable by renderBoard: p1 (player board view), p2 (opponent board view), h1,h2
    getSnapshots(){
      // For consistency with WASM layout: value: 0 untouched, 1 hit, -1 miss
      const p1 = new Float32Array(this.size*this.size);
      const p2 = new Float32Array(this.size*this.size);
      for (let i=0;i<p1.length;i++){
        p1[i] = this.playerView[i];
        p2[i] = this.opponentView[i];
      }
      return { p1, p2, h1: this.h1, h2: this.h2 };
    }

    // player shoots at opponent
    makePlayerMove(row, col){
      const idx = row*this.size + col;
      if (!this.playerTurn) return 0;
      if (this.opponentView[idx] !== 0) return 0; // already shot
      if (this.opponentBoard[idx] === 2){
        this.opponentView[idx] = 1; // hit
        // check sunk
        const sunk = this.checkSunk(this.opponentBoard, this.opponentView, idx);
        if (sunk) return 3; // sunk
        return 2; // hit
      } else {
        this.opponentView[idx] = -1; // miss
        this.playerTurn = false;
        return 1; // miss
      }
    }

    // AI shoots at player: naive random with no repeats
    advanceAITurn(){
      const size = this.size*this.size;
      const choose = () => {
        const candidates = [];
        for (let i=0;i<size;i++) if (this.playerView[i] === 0) candidates.push(i);
        if (candidates.length === 0) return -1;
        return candidates[Math.floor(Math.random()*candidates.length)];
      };
      const idx = choose();
      if (idx < 0) return;
      if (this.playerBoard[idx] === 2){
        this.playerView[idx] = 1; // hit
        // AI can continue shooting (for simplicity AI stops after one shot)
      } else {
        this.playerView[idx] = -1; // miss
      }
      this.playerTurn = true;
    }

    isPlayerTurn(){ return this.playerTurn; }

    // rudimentary sunk detection: if contiguous ship cells are all hit
    checkSunk(board, view, idx){
      // find connected ship cells around idx (4-way)
      const s = this.size;
      const visited = new Set();
      const stack = [idx];
      let allHit = true;
      while (stack.length){
        const i = stack.pop(); if (visited.has(i)) continue; visited.add(i);
        if (board[i] !== 2) continue;
        if (view[i] !== 1) allHit = false;
        const r = Math.floor(i/s), c = i % s;
        const neighbors = [ [r-1,c],[r+1,c],[r,c-1],[r,c+1] ];
        for (const [nr,nc] of neighbors){ if (nr>=0&&nr<s&&nc>=0&&nc<s){ const ni = nr*s+nc; if (!visited.has(ni) && board[ni]===2) stack.push(ni); } }
      }
      return allHit;
    }
  }

  // expose a singleton
  window.fallbackGame = new FallbackGame(10);
  window._useFallback = false;

  // small helper to toggle fallback
  window.enableFallbackGame = function(enabled){
    window._useFallback = !!enabled;
    if (enabled){ window.fallbackGame.reset(); }
  };
})();
