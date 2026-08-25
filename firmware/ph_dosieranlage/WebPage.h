// WebPage.h - Weboberflaeche als ein einziges HTML-Dokument im Flash.
// Bewusst ohne externe CSS/JS-Quellen, damit die Seite auch ohne Internet laeuft.
#pragma once

#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>pH-Minus-Dosieranlage</title>
<style>
:root{--bg:#11151c;--card:#1b212b;--line:#2c3542;--fg:#e8edf4;--mut:#93a1b5;
      --acc:#4ea1ff;--ok:#3fbf7f;--warn:#e2b93b;--err:#e05c5c}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,Segoe UI,Roboto,sans-serif}
header{padding:14px 16px;background:var(--card);border-bottom:1px solid var(--line);
       display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px}
h1{font-size:17px;margin:0}
main{max-width:900px;margin:0 auto;padding:16px;display:grid;gap:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}
.card h2{font-size:14px;margin:0 0 10px;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.big{font-size:44px;font-weight:600;line-height:1.1}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px}
label{display:block;font-size:12px;color:var(--mut);margin-bottom:3px}
input,select{width:100%;padding:8px;border-radius:7px;border:1px solid var(--line);
             background:#131922;color:var(--fg);font-size:14px}
input[type=checkbox]{width:auto}
button{padding:9px 14px;border-radius:7px;border:1px solid var(--line);background:#25303f;
       color:var(--fg);font-size:14px;cursor:pointer}
button:hover{border-color:var(--acc)}
button.p{background:var(--acc);border-color:var(--acc);color:#06121f;font-weight:600}
button.d{background:var(--err);border-color:var(--err);color:#fff;font-weight:600}
.kv{display:flex;justify-content:space-between;gap:10px;padding:4px 0;border-bottom:1px solid #232b36;font-size:14px}
.kv:last-child{border-bottom:0}
.kv span:first-child{color:var(--mut)}
.pill{display:inline-block;padding:3px 10px;border-radius:20px;font-size:12px;font-weight:600}
.b-ok{background:rgba(63,191,127,.16);color:var(--ok)}
.b-warn{background:rgba(226,185,59,.16);color:var(--warn)}
.b-err{background:rgba(224,92,92,.16);color:var(--err)}
.bar{height:8px;background:#131922;border-radius:5px;overflow:hidden;margin-top:6px}
.bar i{display:block;height:100%;background:var(--acc)}
.hint{font-size:12px;color:var(--mut);margin:6px 0 0}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#25303f;
       border:1px solid var(--line);padding:10px 16px;border-radius:8px;display:none;max-width:92%}
</style>
</head>
<body>
<header>
  <h1>pH-Minus-Dosieranlage</h1>
  <div class="row">
    <span id="st" class="pill b-warn">...</span>
    <button class="d" onclick="post('/api/estop')">NOT-HALT</button>
  </div>
</header>

<main>
  <section class="card">
    <h2>Messwert</h2>
    <div class="row" style="justify-content:space-between;align-items:flex-end">
      <div><div class="big" id="ph">--.--</div><div class="hint" id="phst">-</div></div>
      <div style="min-width:200px">
        <div class="kv"><span>Sollwert</span><b id="sp">-</b></div>
        <div class="kv"><span>Spannung</span><b id="volt">-</b></div>
        <div class="kv"><span>ADC roh</span><b id="raw">-</b></div>
        <div class="kv"><span>Steilheit</span><b id="slope">-</b></div>
      </div>
    </div>
    <div class="kv" style="margin-top:8px"><span>Sperren</span><b id="locks">-</b></div>
  </section>

  <section class="card">
    <h2>Dosierung</h2>
    <div class="kv"><span>Heute</span><b id="dt">-</b></div>
    <div class="bar"><i id="dbar" style="width:0%"></i></div>
    <div class="kv" style="margin-top:8px"><span>Dosierungen heute</span><b id="dc">-</b></div>
    <div class="kv"><span>Letzte Dosis</span><b id="dl">-</b></div>
    <div class="kv"><span>Restpause</span><b id="dp">-</b></div>
    <div class="kv"><span>Pumpe</span><b id="pu">-</b></div>
    <div class="row" style="margin-top:12px">
      <button class="p" id="autoBtn" onclick="toggleAuto()">Automatik</button>
      <input id="mdose" type="number" step="0.1" min="0.1" value="1.0" style="width:100px">
      <button onclick="post('/api/dose?ml='+v('mdose'))">Manuell dosieren</button>
      <button onclick="post('/api/stop')">Stopp</button>
      <button onclick="post('/api/clearfault')">Quittieren</button>
    </div>
  </section>

  <section class="card">
    <h2>pH-Kalibrierung (2-Punkt)</h2>
    <p class="hint">Sonde in die Pufferloesung stellen, warten bis der Messwert steht,
       dann den Punkt speichern. Zwischen den Puffern die Sonde spuelen.</p>
    <div class="grid">
      <div><label>Puffer A (typisch 7.00)</label><input id="calA" type="number" step="0.01" value="7.00"></div>
      <div style="align-self:end"><button onclick="post('/api/cal?point=a&ph='+v('calA'))">Punkt A speichern</button></div>
      <div><label>Puffer B (typisch 4.00)</label><input id="calB" type="number" step="0.01" value="4.00"></div>
      <div style="align-self:end"><button onclick="post('/api/cal?point=b&ph='+v('calB'))">Punkt B speichern</button></div>
    </div>
    <div class="kv" style="margin-top:10px"><span>Gespeichert</span><b id="calinfo">-</b></div>
    <div class="row" style="margin-top:10px"><button onclick="post('/api/cal/reset')">Kalibrierung verwerfen</button></div>
  </section>

  <section class="card">
    <h2>Pumpenkalibrierung</h2>
    <p class="hint">Schlauch in ein Messgefaess legen, erst entlueften, dann eine feste
       Schrittzahl fahren, gefoerderte Menge abmessen und eintragen.</p>
    <div class="grid">
      <div><label>Schritte fahren</label><input id="pst" type="number" value="16000"></div>
      <div style="align-self:end"><div class="row">
        <button onclick="post('/api/pump/run?steps='+v('pst')+'&dir=1')">Vorwaerts</button>
        <button onclick="post('/api/pump/run?steps='+v('pst')+'&dir=0')">Rueckwaerts</button>
      </div></div>
      <div><label>Gemessene Menge [ml]</label><input id="pml" type="number" step="0.01" value="10.0"></div>
      <div style="align-self:end"><button class="p" onclick="post('/api/pump/calc?steps='+v('pst')+'&ml='+v('pml'))">Schritte/ml berechnen</button></div>
    </div>
    <div class="kv" style="margin-top:10px"><span>Aktuell</span><b id="spml">-</b></div>
  </section>

  <section class="card">
    <h2>Regelung &amp; Sicherheit</h2>
    <div class="grid">
      <div><label>pH-Sollwert</label><input data-k="sp" type="number" step="0.01"></div>
      <div><label>Totband [pH]</label><input data-k="db" type="number" step="0.01"></div>
      <div><label>Einzeldosis [ml]</label><input data-k="dose" type="number" step="0.1"></div>
      <div><label>max. Einzeldosis [ml]</label><input data-k="maxs" type="number" step="0.1"></div>
      <div><label>max. Tagesmenge [ml]</label><input data-k="maxd" type="number" step="1"></div>
      <div><label>Pause/Durchmischung [s]</label><input data-k="pause" type="number" step="10"></div>
      <div><label>Sperre unter pH</label><input data-k="phlock" type="number" step="0.05"></div>
      <div><label>unplausibel ueber pH</label><input data-k="phmax" type="number" step="0.1"></div>
      <div><label>Umwaelzung erforderlich</label><input data-k="flowreq" type="checkbox"></div>
      <div><label>Umwaelz-Eingang invertieren</label><input data-k="flowinv" type="checkbox"></div>
    </div>
    <div class="row" style="margin-top:12px"><button class="p" onclick="saveCfg()">Speichern</button></div>
  </section>

  <section class="card">
    <h2>Pumpe &amp; Sensor</h2>
    <div class="grid">
      <div><label>Schritte pro ml</label><input data-k="spml" type="number" step="1"></div>
      <div><label>Schrittrate [Schritte/s]</label><input data-k="srate" type="number" step="50"></div>
      <div><label>Beschleunigung [S/s^2]</label><input data-k="sacc" type="number" step="100"></div>
      <div><label>Drehrichtung umkehren</label><input data-k="invdir" type="checkbox"></div>
      <div><label>Treiber dauerhaft bestromt</label><input data-k="hold" type="checkbox"></div>
      <div><label>ADS1115 Messbereich</label>
        <select data-k="gain">
          <option value="0">+/- 6.144 V</option>
          <option value="1">+/- 4.096 V</option>
          <option value="2">+/- 2.048 V</option>
          <option value="3">+/- 1.024 V</option>
          <option value="4">+/- 0.512 V</option>
          <option value="5">+/- 0.256 V</option>
        </select></div>
    </div>
    <div class="row" style="margin-top:12px"><button class="p" onclick="saveCfg()">Speichern</button></div>
  </section>

  <section class="card">
    <h2>Netzwerk</h2>
    <div class="grid">
      <div><label>WLAN SSID</label><input data-k="ssid" type="text"></div>
      <div><label>WLAN Passwort</label><input data-k="pass" type="password" placeholder="unveraendert lassen"></div>
      <div><label>Hostname</label><input data-k="host" type="text"></div>
      <div><label>Web-Benutzer (leer = kein Login)</label><input data-k="wuser" type="text"></div>
      <div><label>Web-Passwort</label><input data-k="wpass" type="password" placeholder="unveraendert lassen"></div>
    </div>
    <div class="row" style="margin-top:12px">
      <button class="p" onclick="saveCfg()">Speichern</button>
      <button onclick="post('/api/reboot')">Neustart</button>
    </div>
  </section>

  <section class="card">
    <h2>System</h2>
    <div class="kv"><span>Firmware</span><b id="fw">-</b></div>
    <div class="kv"><span>Laufzeit</span><b id="up">-</b></div>
    <div class="kv"><span>Netz</span><b id="net">-</b></div>
    <div class="kv"><span>Uhrzeit</span><b id="clk">-</b></div>
    <div class="kv"><span>Umwaelzung</span><b id="flow">-</b></div>
    <div class="kv"><span>Gesamtmenge</span><b id="tot">-</b></div>
    <div class="kv"><span>Freier Heap</span><b id="heap">-</b></div>
    <div class="row" style="margin-top:12px">
      <button onclick="post('/api/daily/reset')">Tageszaehler zuruecksetzen</button>
    </div>
  </section>
</main>

<div id="toast"></div>

<script>
var cfgLoaded = false;
function v(id){return document.getElementById(id).value;}
function el(id){return document.getElementById(id);}
function toast(m){var t=el('toast');t.textContent=m;t.style.display='block';
  clearTimeout(t._h);t._h=setTimeout(function(){t.style.display='none';},3200);}

function post(url){
  fetch(url,{method:'POST'}).then(function(r){return r.json();})
   .then(function(j){toast(j.ok?(j.msg||'OK'):('Abgelehnt: '+(j.msg||'Fehler')));refresh();})
   .catch(function(e){toast('Verbindungsfehler');});
}

function saveCfg(){
  var q=[];
  document.querySelectorAll('[data-k]').forEach(function(i){
    var val = (i.type==='checkbox') ? (i.checked?'1':'0') : i.value;
    if((i.dataset.k==='pass'||i.dataset.k==='wpass') && val==='') return;
    q.push(encodeURIComponent(i.dataset.k)+'='+encodeURIComponent(val));
  });
  fetch('/api/settings?'+q.join('&'),{method:'POST'})
   .then(function(r){return r.json();})
   .then(function(j){toast(j.ok?'Gespeichert':('Fehler: '+j.msg));cfgLoaded=false;refresh();});
}

function toggleAuto(){post('/api/auto?on='+(window._auto?'0':'1'));}

function fmtDur(s){
  var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  return (d?d+'d ':'')+h+'h '+m+'m';
}

function refresh(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(s){
    window._auto = s.auto;
    el('ph').textContent = s.phValid ? s.ph.toFixed(2) : '--.--';
    el('phst').textContent = s.phStatus + (s.stable?' - stabil':' - schwankt') +
                             ' (Spanne ' + s.spread.toFixed(2) + ')';
    el('sp').textContent = s.cfg.sp.toFixed(2);
    el('volt').textContent = s.volt.toFixed(4)+' V';
    el('raw').textContent = s.raw;
    el('slope').textContent = (s.slope !== null && isFinite(s.slope))
                              ? s.slope.toFixed(1)+' mV/pH' : '-';
    el('locks').textContent = s.locks;

    var b = el('st');
    b.textContent = s.state;
    b.className = 'pill ' + (s.state==='Stoerung'||s.state==='Gesperrt' ? 'b-err'
                          : (s.state==='Bereit' ? 'b-ok' : 'b-warn'));

    el('dt').textContent = s.dose.today.toFixed(2)+' / '+s.cfg.maxd.toFixed(0)+' ml';
    el('dbar').style.width = Math.min(100, s.dose.today/s.cfg.maxd*100)+'%';
    el('dc').textContent = s.dose.count;
    el('dl').textContent = s.dose.last.toFixed(2)+' ml';
    el('dp').textContent = s.dose.pauseS+' s';
    el('pu').textContent = s.pump.run
        ? ('laeuft - '+s.pump.ml.toFixed(2)+' / '+s.pump.target.toFixed(2)+' ml')
        : 'steht';
    el('autoBtn').textContent = s.auto ? 'Automatik AUS' : 'Automatik EIN';
    el('autoBtn').className = s.auto ? 'p' : '';

    el('calinfo').textContent = s.cal.ok
      ? ('pH '+s.cal.phA.toFixed(2)+' = '+s.cal.vA.toFixed(4)+' V  |  pH '
         +s.cal.phB.toFixed(2)+' = '+s.cal.vB.toFixed(4)+' V')
      : 'keine gueltige Kalibrierung';
    el('spml').textContent = s.cfg.spml.toFixed(1)+' Schritte/ml  ('
      +(1000/s.cfg.spml).toFixed(1)+' ul/Schritt)';

    el('fw').textContent = s.fw;
    el('up').textContent = fmtDur(s.up);
    el('net').textContent = s.wifi.mode+' '+s.wifi.ip+' ('+s.wifi.rssi+' dBm)';
    el('clk').textContent = s.time;
    el('flow').textContent = s.flow ? 'OK' : 'fehlt';
    el('tot').textContent = s.dose.total.toFixed(1)+' ml';
    el('heap').textContent = (s.heap/1024).toFixed(1)+' kB';

    if(!cfgLoaded){
      document.querySelectorAll('[data-k]').forEach(function(i){
        var k=i.dataset.k;
        if(!(k in s.cfg)) return;
        if(i.type==='checkbox') i.checked = !!s.cfg[k];
        else i.value = s.cfg[k];
      });
      cfgLoaded = true;
    }
  }).catch(function(e){el('st').textContent='offline';el('st').className='pill b-err';});
}
refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawliteral";
