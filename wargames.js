// WarGames easter egg module
(function(){
  const header = document.querySelector('h1');
  if (!header) return;

  // Listen for double-click on header or secret key sequence
  header.addEventListener('dblclick', showOverlay);

  const secret = 'wargames';
  let buf = '';
  window.addEventListener('keydown', (e) => {
    buf += e.key.toLowerCase();
    if (buf.length > 16) buf = buf.slice(-16);
    if (buf.endsWith(secret)) showOverlay();
  });

  function showOverlay(){
    if (document.getElementById('wg-overlay')) return; // already open
    const overlay = document.createElement('div'); overlay.id = 'wg-overlay'; overlay.className = 'wg-overlay';
    const modal = document.createElement('div'); modal.className = 'wg-modal';
    const title = document.createElement('div'); title.className = 'wg-title'; title.textContent = 'WarGames Detected';
    const text = document.createElement('div'); text.className = 'wg-text wg-caret'; text.textContent = '';
    const actions = document.createElement('div'); actions.className = 'wg-actions';
    const yes = document.createElement('button'); yes.className = 'wg-btn'; yes.textContent = 'Shall we play a game?';
    const no = document.createElement('button'); no.className = 'wg-btn'; no.textContent = 'No thanks';
    actions.appendChild(no); actions.appendChild(yes);
    modal.appendChild(title); modal.appendChild(text); modal.appendChild(actions); overlay.appendChild(modal);
    document.body.appendChild(overlay);

    // typing effect
    const msg = "Shall we play a game?";
    let i = 0;
    const t = setInterval(()=>{
      text.textContent = msg.slice(0, i);
      i++;
      if (i > msg.length) { clearInterval(t); text.classList.remove('wg-caret'); }
    }, 45);

    no.onclick = ()=>{ overlay.remove(); };
    yes.onclick = ()=>{
      overlay.remove(); activateWarGames();
    };
  }

  function activateWarGames(){
    document.documentElement.classList.add('wargames');
    addToggle();
    playMelody();
    // minor UI hint in log
    try { const log = document.getElementById('log'); if (log) { log.textContent += '\n[WarGames theme enabled]\n'; log.scrollTop = log.scrollHeight; } } catch(e){}
  }

  function deactivateWarGames(){
    document.documentElement.classList.remove('wargames');
    const t = document.getElementById('wg-toggle'); if (t) t.remove();
    try { const log = document.getElementById('log'); if (log) { log.textContent += '\n[WarGames theme disabled]\n'; log.scrollTop = log.scrollHeight; } } catch(e){}
  }

  function addToggle(){
    if (document.getElementById('wg-toggle')) return;
    const btn = document.createElement('button'); btn.id = 'wg-toggle'; btn.className = 'wg-toggle'; btn.textContent = 'WARGAMES: ON';
    btn.onclick = ()=>{ if (document.documentElement.classList.contains('wargames')){ deactivateWarGames(); btn.textContent='WARGAMES: OFF'; } else { activateWarGames(); btn.textContent='WARGAMES: ON'; } };
    document.body.appendChild(btn);
  }

  // Simple melody via WebAudio
  function playMelody(){
    try {
      const ctx = new (window.AudioContext || window.webkitAudioContext)();
      const melody = [440, 440, 523, 659, 880];
      let t = ctx.currentTime;
      melody.forEach((f, i)=>{
        const o = ctx.createOscillator(); const g = ctx.createGain();
        o.type = 'sine'; o.frequency.value = f;
        g.gain.value = 0.0025 + 0.025 * Math.max(0, 1 - i*0.12);
        o.connect(g); g.connect(ctx.destination);
        o.start(t + i*0.09); o.stop(t + i*0.09 + 0.09);
      });
    } catch (e) { console.warn('Audio not available:', e); }
  }
})();
