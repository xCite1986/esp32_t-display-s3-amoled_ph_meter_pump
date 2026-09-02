// WebPage.h - Weboberflaeche als ein einziges HTML-Dokument im Flash.
// Bewusst ohne externe CSS/JS-Quellen, damit die Seite auch ohne Internet laeuft.
//
// Umlaute stehen als HTML-Entities (&auml; usw.). Das ueberlebt jede
// Encoding-Eigenheit der Toolchain, waehrend rohes UTF-8 in C-Stringliteralen
// je nach Editor und Compiler verstuemmelt ankommen kann.
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
:root{--bg:#000;--line:#333;--fg:#fff;--mut:#9a9a9a;
      --yel:#ffc61a;--org:#ff8a00;--grn:#4fd98a}
*{box-sizing:border-box;border-radius:0}
html,body{margin:0;padding:0}
body{background:var(--bg);color:var(--fg);
     font:15px/1.5 system-ui,Segoe UI,Roboto,sans-serif}
header{display:flex;justify-content:space-between;align-items:center;
       flex-wrap:wrap;gap:10px;padding:14px 18px;border-bottom:1px solid var(--line)}
h1{font-size:16px;margin:0;letter-spacing:.08em;text-transform:uppercase}
/* Trennlinien als 1px-Luecken im Raster: das bleibt richtig, egal an welcher
   Stelle eine Karte ueber beide Spalten geht. */
main{display:grid;grid-template-columns:1fr 1fr;width:100%;
     gap:1px;background:var(--line)}
.card{background:var(--bg);padding:16px 18px}
.wide{grid-column:1 / -1}
.card h2{font-size:12px;margin:0 0 12px;color:var(--yel);
         text-transform:uppercase;letter-spacing:.1em}
.big{font-size:52px;font-weight:600;line-height:1}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}
label{display:block;font-size:11px;color:var(--mut);margin-bottom:3px;
      text-transform:uppercase;letter-spacing:.06em}
input,select{width:100%;padding:9px;border:1px solid var(--line);
             background:#000;color:var(--fg);font-size:15px}
input:focus,select:focus{outline:0;border-color:var(--yel)}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--yel)}
button{padding:10px 16px;border:1px solid var(--yel);background:#000;
       color:var(--yel);font-size:14px;cursor:pointer;
       text-transform:uppercase;letter-spacing:.06em}
button:hover{background:var(--yel);color:#000}
button.p{background:var(--yel);color:#000;font-weight:700}
button.p:hover{background:var(--org);border-color:var(--org)}
button.d{background:var(--org);border-color:var(--org);color:#000;font-weight:700}
.kv{display:flex;justify-content:space-between;gap:12px;padding:5px 0;
    border-bottom:1px solid #1a1a1a;font-size:14px}
.kv:last-child{border-bottom:0}
.kv span:first-child{color:var(--mut)}
.pill{display:inline-block;padding:4px 12px;font-size:12px;font-weight:700;
      text-transform:uppercase;letter-spacing:.08em;border:1px solid}
.b-ok{color:var(--grn);border-color:var(--grn)}
.b-warn{color:var(--yel);border-color:var(--yel)}
.b-err{color:var(--org);border-color:var(--org)}
.bar{height:6px;background:#1a1a1a;overflow:hidden;margin-top:6px}
.bar i{display:block;height:100%;background:var(--yel)}
.hint{font-size:12px;color:var(--mut);margin:0 0 10px}
code{color:var(--yel)}
#chartwrap{position:relative}
#chartwrap svg{cursor:crosshair;touch-action:none}
#tip{position:absolute;display:none;pointer-events:none;z-index:5;
     background:#000;border:1px solid var(--yel);padding:7px 10px;
     font-size:12px;line-height:1.5;white-space:nowrap}
#tip .t{color:var(--yel);font-weight:700;margin-bottom:3px}
#tip .m{color:var(--mut)}
#tip .o{color:var(--org)}
#toast{position:fixed;left:0;right:0;bottom:0;background:#000;
       border-top:2px solid var(--yel);padding:12px 18px;display:none}
@media(max-width:820px){main{grid-template-columns:1fr}
  .big{font-size:44px}}
</style>
</head>
<body>
<header>
  <h1>pH-Minus-Dosieranlage</h1>
  <div class="row">
    <span id="st" class="pill b-warn">...</span>
    <button class="d" onclick="post('/api/estop')">Not-Halt</button>
  </div>
</header>

<main>
  <section class="card wide">
    <h2>Verlauf 7 Tage</h2>
    <div id="chartwrap"><div id="chart"></div><div id="tip"></div></div>
    <div class="row" style="margin-top:10px;font-size:12px;color:var(--mut)">
      <span><b style="color:var(--yel)">&#9472;&#9472;</b> pH (Stundenmittel)</span>
      <span><b style="color:#5a4a12">&#9608;</b> Schwankungsbreite</span>
      <span><b style="color:var(--org)">&#9608;</b> dosiert [ml/h]</span>
    </div>
    <div class="kv" style="margin-top:10px"><span>heute dosiert</span><b id="hd0">-</b></div>
    <div class="kv"><span>gestern</span><b id="hd1">-</b></div>
    <div class="kv"><span>Summe 7 Tage</span><b id="hd7">-</b></div>
  </section>

  <section class="card">
    <h2>Messwert</h2>
    <div class="big" id="ph">--.--</div>
    <div class="hint" id="phst" style="margin-top:6px">-</div>
    <div class="kv"><span>Mittel (Regelgr&ouml;&szlig;e)</span><b id="phavg">-</b></div>
    <div class="kv"><span>Sollwert</span><b id="sp">-</b></div>
    <div class="kv"><span>Spannung</span><b id="volt">-</b></div>
    <div class="kv"><span>ADC roh</span><b id="raw">-</b></div>
    <div class="kv"><span>Steilheit</span><b id="slope">-</b></div>
    <div class="kv"><span>Sperren</span><b id="locks">-</b></div>
    <div class="row" style="margin-top:14px">
      <button onclick="post('/api/i2c/scan')">I&sup2;C scannen</button>
    </div>
  </section>

  <section class="card">
    <h2>Dosierung</h2>
    <div class="kv"><span>Heute</span><b id="dt">-</b></div>
    <div class="bar"><i id="dbar" style="width:0%"></i></div>
    <div class="kv" style="margin-top:8px"><span>Dosierungen heute</span><b id="dc">-</b></div>
    <div class="kv"><span>Letzte Dosis</span><b id="dl">-</b></div>
    <div class="kv"><span>Restpause</span><b id="dp">-</b></div>
    <div class="kv"><span>Pumpe</span><b id="pu">-</b></div>
    <div class="row" style="margin-top:14px">
      <button class="p" id="autoBtn" onclick="toggleAuto()">Automatik</button>
      <input id="mdose" type="number" step="0.1" min="0.1" value="1.0" style="width:90px">
      <button onclick="post('/api/dose?ml='+v('mdose'))">Manuell dosieren</button>
      <button onclick="post('/api/stop')">Stopp</button>
      <button onclick="post('/api/clearfault')">Quittieren</button>
    </div>
  </section>

  <section class="card">
    <h2>pH-Kalibrierung (2-Punkt)</h2>
    <p class="hint">Sonde in die Pufferl&ouml;sung stellen, warten bis der Messwert
       steht, dann den Punkt speichern. Zwischen den Puffern sp&uuml;len.</p>
    <div class="grid">
      <div><label>Puffer A</label><input id="calA" type="number" step="0.01" value="7.00"></div>
      <div style="align-self:end"><button onclick="post('/api/cal?point=a&ph='+v('calA'))">Punkt A speichern</button></div>
      <div><label>Puffer B</label><input id="calB" type="number" step="0.01" value="4.00"></div>
      <div style="align-self:end"><button onclick="post('/api/cal?point=b&ph='+v('calB'))">Punkt B speichern</button></div>
    </div>
    <div class="kv" style="margin-top:12px"><span>Gespeichert</span><b id="calinfo">-</b></div>
    <div class="row" style="margin-top:12px"><button onclick="post('/api/cal/reset')">Kalibrierung verwerfen</button></div>
  </section>

  <section class="card">
    <h2>Pumpenkalibrierung</h2>
    <p class="hint">Schlauch in ein Messgef&auml;&szlig; legen, erst entl&uuml;ften, dann
       eine feste Schrittzahl fahren, gef&ouml;rderte Menge abmessen und eintragen.</p>
    <div class="grid">
      <div><label>Schritte fahren</label><input id="pst" type="number" value="16000"></div>
      <div style="align-self:end"><div class="row">
        <button onclick="post('/api/pump/run?steps='+v('pst')+'&dir=1')">Vorw&auml;rts</button>
        <button onclick="post('/api/pump/run?steps='+v('pst')+'&dir=0')">R&uuml;ckw&auml;rts</button>
      </div></div>
      <div><label>Gemessene Menge [ml]</label><input id="pml" type="number" step="0.01" value="10.0"></div>
      <div style="align-self:end"><button class="p" onclick="post('/api/pump/calc?steps='+v('pst')+'&ml='+v('pml'))">Schritte/ml berechnen</button></div>
    </div>
    <div class="kv" style="margin-top:12px"><span>Aktuell</span><b id="spml">-</b></div>
  </section>

  <section class="card">
    <h2>Regelung und Sicherheit</h2>
    <div class="grid">
      <div><label>pH-Sollwert</label><input data-k="sp" type="number" step="0.01"></div>
      <div><label>Totband [pH]</label><input data-k="db" type="number" step="0.01"></div>
      <div><label>Einzeldosis [ml]</label><input data-k="dose" type="number" step="0.1"></div>
      <div><label>max. Einzeldosis [ml]</label><input data-k="maxs" type="number" step="0.1"></div>
      <div><label>max. Tagesmenge [ml]</label><input data-k="maxd" type="number" step="1"></div>
      <div><label>Durchmischung [s]</label><input data-k="pause" type="number" step="60"></div>
      <div><label>Sperre unter pH</label><input data-k="phlock" type="number" step="0.05"></div>
      <div><label>unplausibel &uuml;ber pH</label><input data-k="phmax" type="number" step="0.1"></div>
    </div>
    <div class="row" style="margin-top:14px"><button class="p" onclick="saveCfg()">Speichern</button></div>
  </section>

  <section class="card">
    <h2>Pumpe und Sensor</h2>
    <div class="grid">
      <div><label>Schritte pro ml</label><input data-k="spml" type="number" step="1"></div>
      <div><label>Schritte pro Umdrehung</label><input data-k="sprev" type="number" step="100"></div>
      <div><label>Schrittrate [Schritte/s]</label><input data-k="srate" type="number" step="50"></div>
      <div><label>Beschleunigung [S/s&sup2;]</label><input data-k="sacc" type="number" step="100"></div>
      <div><label>Drehrichtung umkehren</label><input data-k="invdir" type="checkbox"></div>
      <div><label>Treiber dauerhaft bestromt</label><input data-k="hold" type="checkbox"></div>
      <div><label>Filterzeit [s]</label><input data-k="filt" type="number" step="5" min="1" max="300"></div>
      <div><label>Mittelung f&uuml;r Dosierung [s]</label><input data-k="avgs" type="number" step="60" min="60" max="3600"></div>
      <div><label>ADS1115 Messbereich (Reserve f&uuml;r St&ouml;rspitzen lassen)</label>
        <select data-k="gain">
          <option value="0">+/- 6.144 V</option>
          <option value="1">+/- 4.096 V</option>
          <option value="2">+/- 2.048 V</option>
          <option value="3">+/- 1.024 V</option>
          <option value="4">+/- 0.512 V</option>
          <option value="5">+/- 0.256 V</option>
        </select></div>
    </div>
    <div class="row" style="margin-top:14px"><button class="p" onclick="saveCfg()">Speichern</button></div>
  </section>

  <section class="card">
    <h2>Umw&auml;lzung (Home Assistant)</h2>
    <p class="hint">Wenn aktiv, fragt die Anlage <b>erst unmittelbar vor einer
       Dosierung</b> den Zustand einer Entit&auml;t ab
       (<code>GET /api/states/&lt;entity&gt;</code>) &mdash; nicht zyklisch.
       Zwei Abfragen liegen dadurch mindestens eine Durchmischungspause
       auseinander. Steht die Umw&auml;lzung, ist die Entit&auml;t
       <code>unavailable</code> oder antwortet Home Assistant nicht, wird nicht
       dosiert.</p>
    <p class="hint">Der Token gibt vollen API-Zugriff auf Home Assistant &mdash;
       am besten einen eigenen, nicht-administrativen Benutzer daf&uuml;r anlegen.
       Er wird nie zur&uuml;ckgeliefert; ein leeres Feld l&auml;sst ihn unver&auml;ndert.</p>
    <div class="grid">
      <div><label>Pr&uuml;fung aktiv</label><input data-k="circen" type="checkbox"></div>
      <div><label>Home Assistant Host:Port</label><input data-k="hahost" type="text" placeholder="192.168.0.10:8123"></div>
      <div><label>Entit&auml;t</label><input data-k="haent" type="text" placeholder="switch.poolpumpe"></div>
      <div><label>Zustand f&uuml;r &bdquo;l&auml;uft&ldquo;</label><input data-k="haon" type="text" placeholder="on"></div>
      <div style="grid-column:1/-1"><label>Long-Lived Access Token</label><input data-k="hatok" type="password" placeholder="unver&auml;ndert lassen" autocomplete="off"></div>
      <div><label>Antwort gilt als aktuell [s]</label><input data-k="circfr" type="number" step="10" min="10" max="3600"></div>
      <div><label>Neuversuch nach Fehler [s]</label><input data-k="circrt" type="number" step="15" min="15" max="3600"></div>
      <div><label>Neuversuch bei Umw&auml;lzung aus [s]</label><input data-k="circof" type="number" step="15" min="15" max="3600"></div>
    </div>
    <div class="kv" style="margin-top:12px"><span>Zustand</span><b id="circst">-</b></div>
    <div class="kv"><span>Entit&auml;t meldet</span><b id="circraw">-</b></div>
    <div class="kv"><span>Token hinterlegt</span><b id="circtok">-</b></div>
    <div class="kv"><span>Abfragen seit Start</span><b id="circn">-</b></div>
    <div class="row" style="margin-top:14px">
      <button class="p" onclick="saveCfg()">Speichern</button>
      <button onclick="post('/api/circ/test')">Jetzt testen</button>
    </div>
  </section>

  <section class="card">
    <h2>Anzeige</h2>
    <p class="hint">Nach der Standby-Zeit zeigt das Display nur noch den pH-Wert,
       ged&auml;mpft und regelm&auml;&szlig;ig versetzt &mdash; schwarze Pixel sind am AMOLED
       aus. Im Nachtfenster bleibt es dunkel und wacht auf Ber&uuml;hrung auf.</p>
    <div class="grid">
      <div><label>Standby nach [s]</label><input data-k="stby" type="number" step="15" min="15" max="3600"></div>
      <div><label>Versetzen alle [s]</label><input data-k="shft" type="number" step="30" min="30" max="3600"></div>
      <div><label>Anzeige 180&deg; gedreht</label><input data-k="rot180" type="checkbox"></div>
      <div><label>Nachtabschaltung</label><input data-k="nite" type="checkbox"></div>
      <div><label>Nacht von [Stunde]</label><input data-k="nfrom" type="number" step="1" min="0" max="23"></div>
      <div><label>Nacht bis [Stunde]</label><input data-k="nto" type="number" step="1" min="0" max="23"></div>
      <div><label>Umdrehungen pro Touch</label><input data-k="prevs" type="number" step="1" min="1" max="20"></div>
    </div>
    <div class="kv" style="margin-top:12px"><span>Zustand</span><b id="dispst">-</b></div>
    <div class="kv"><span>Freigabe entspricht</span><b id="revml">-</b></div>
    <div class="row" style="margin-top:14px"><button class="p" onclick="saveCfg()">Speichern</button></div>
  </section>

  <section class="card">
    <h2>Netzwerk</h2>
    <div class="grid">
      <div><label>WLAN SSID</label><input data-k="ssid" type="text"></div>
      <div><label>WLAN Passwort</label><input data-k="pass" type="password" placeholder="unver&auml;ndert lassen"></div>
      <div><label>Hostname</label><input data-k="host" type="text"></div>
      <div><label>Web-Benutzer</label><input data-k="wuser" type="text" placeholder="leer = kein Login"></div>
      <div><label>Web-Passwort</label><input data-k="wpass" type="password" placeholder="unver&auml;ndert lassen"></div>
    </div>
    <div class="row" style="margin-top:14px">
      <button class="p" onclick="saveCfg()">Speichern</button>
      <button onclick="post('/api/reboot')">Neustart</button>
    </div>
  </section>

  <section class="card wide">
    <h2>System</h2>
    <div class="grid">
      <div class="kv"><span>Firmware</span><b id="fw">-</b></div>
      <div class="kv"><span>Laufzeit</span><b id="up">-</b></div>
      <div class="kv"><span>Netz</span><b id="net">-</b></div>
      <div class="kv"><span>Uhrzeit</span><b id="clk">-</b></div>
      <div class="kv"><span>Gesamtmenge</span><b id="tot">-</b></div>
      <div class="kv"><span>Freier Heap</span><b id="heap">-</b></div>
    </div>
    <div class="row" style="margin-top:14px">
      <button onclick="post('/api/daily/reset')">Tagesz&auml;hler zur&uuml;cksetzen</button>
    </div>
  </section>
</main>

<div id="toast"></div>

<script>
var cfgLoaded = false;
function v(id){return document.getElementById(id).value;}
function el(id){return document.getElementById(id);}
function toast(m){var t=el('toast');t.textContent=m;t.style.display='block';
  clearTimeout(t._h);t._h=setTimeout(function(){t.style.display='none';},4000);}

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
    window._sp = s.cfg.sp;
    el('ph').textContent = s.phValid ? s.ph.toFixed(2) : '--.--';
    // Stabilitaet und Spanne nur zeigen, wenn ueberhaupt gemessen wird -
    // sonst steht neben "nicht erreichbar" ein Initialwert, der wie ein
    // zweites Problem aussieht.
    el('phst').textContent = s.phValid
      ? (s.phStatus + (s.stable?' – stabil':' – schwankt')
         + ' (Spanne ' + s.spread.toFixed(2) + ' pH)')
      : (s.raw !== 0
         ? (s.phStatus + ' – Spanne ' + s.spreadmV.toFixed(1) + ' mV'
            + (s.spreadmV <= 20 ? ' (ruhig genug zum Kalibrieren)'
                                : ' (noch zu unruhig, < 20 mV nötig)'))
         : s.phStatus);
    el('phavg').textContent = (s.phAvg === null || s.phAvg === undefined) ? '-'
      : s.avgOk ? s.phAvg.toFixed(2) + ' pH'
                : s.phAvg.toFixed(2) + ' pH (Fenster f\u00fcllt sich)';
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
        ? ('läuft – '+s.pump.ml.toFixed(2)+' / '+s.pump.target.toFixed(2)+' ml')
        : 'steht';
    el('autoBtn').textContent = s.auto ? 'Automatik aus' : 'Automatik ein';
    el('autoBtn').className = s.auto ? 'p' : '';

    el('calinfo').textContent = s.cal.ok
      ? ('pH '+s.cal.phA.toFixed(2)+' = '+s.cal.vA.toFixed(4)+' V  |  pH '
         +s.cal.phB.toFixed(2)+' = '+s.cal.vB.toFixed(4)+' V')
      : 'keine gültige Kalibrierung';
    el('spml').textContent = s.cfg.spml.toFixed(1)+' Schritte/ml  ('
      +(1000/s.cfg.spml).toFixed(1)+' µl/Schritt)';

    el('circst').textContent = s.circ;
    el('circraw').textContent = s.circRaw || '-';
    el('circtok').textContent = s.cfg.hatok ? 'ja' : 'nein';
    el('circn').textContent = s.circN;
    el('dispst').textContent = s.disp;
    el('revml').textContent = s.cfg.prevs+' Umdrehungen = '
      +(s.cfg.prevs*s.cfg.sprev/s.cfg.spml).toFixed(2)+' ml';

    el('fw').textContent = s.fw;
    el('up').textContent = fmtDur(s.up);
    el('net').textContent = s.wifi.mode+' '+s.wifi.ip+' ('+s.wifi.rssi+' dBm)';
    el('clk').textContent = s.time;
    el('tot').textContent = s.dose.total.toFixed(1)+' ml';
    el('heap').textContent = (s.heap/1024).toFixed(1)+' kB';

    if(!cfgLoaded){
      document.querySelectorAll('[data-k]').forEach(function(i){
        var k=i.dataset.k;
        if(k==='hatok') return;              // Token nie zurueckschreiben
        if(!(k in s.cfg)) return;
        if(i.type==='checkbox') i.checked = !!s.cfg[k];
        else i.value = s.cfg[k];
      });
      cfgLoaded = true;
    }
  }).catch(function(e){el('st').textContent='offline';el('st').className='pill b-err';});
}
// ---------------------------------------------------------------------------
// Verlaufschart. Handgezeichnetes SVG statt einer Bibliothek - die Seite muss
// ohne Internet funktionieren, und fuer 168 Stundenwerte lohnt kein Framework.
// ---------------------------------------------------------------------------
function drawChart(h){
  var W=1100, H=260, L=44, R=52, T=14, B=26;
  var n = h.ph.length;
  if(!n){ el('chart').innerHTML =
    '<p class="hint">Noch kein Verlauf &mdash; die Aufzeichnung braucht eine g\u00fcltige Uhrzeit (NTP).</p>';
    return; }

  // pH-Skala aus den Daten, mit etwas Luft und sinnvollem Minimum
  var lo=99, hi=-99;
  for(var i=0;i<n;i++){
    if(h.min[i]!==null){ lo=Math.min(lo,h.min[i]); hi=Math.max(hi,h.max[i]); }
  }
  if(lo>hi){ lo=6.8; hi=7.6; }
  var pad=Math.max(0.15,(hi-lo)*0.15); lo-=pad; hi+=pad;

  var mlMax=0; for(var i=0;i<n;i++) mlMax=Math.max(mlMax,h.ml[i]);
  if(mlMax<=0) mlMax=1;

  var x=function(i){ return L+(W-L-R)*i/Math.max(1,n-1); };
  var y=function(v){ return T+(H-T-B)*(1-(v-lo)/(hi-lo)); };
  var yb=function(v){ return (H-B)-(H-T-B)*0.35*(v/mlMax); };

  var o='<svg viewBox="0 0 '+W+' '+H+'" style="width:100%;height:auto;display:block">';

  // Tagesraster: alle 24 Werte eine Linie
  for(var i=0;i<n;i+=24){
    o+='<line x1="'+x(i)+'" y1="'+T+'" x2="'+x(i)+'" y2="'+(H-B)+'" stroke="#222"/>';
    var d=new Date((h.first+i)*3600*1000);
    o+='<text x="'+x(i)+'" y="'+(H-8)+'" fill="#9a9a9a" font-size="11" text-anchor="middle">'
      +d.getDate()+'.'+(d.getMonth()+1)+'</text>';
  }
  // pH-Achse
  for(var k=0;k<=4;k++){
    var v=lo+(hi-lo)*k/4;
    o+='<line x1="'+L+'" y1="'+y(v)+'" x2="'+(W-R)+'" y2="'+y(v)+'" stroke="#1a1a1a"/>';
    o+='<text x="'+(L-6)+'" y="'+(y(v)+4)+'" fill="#9a9a9a" font-size="11" text-anchor="end">'
      +v.toFixed(1)+'</text>';
  }
  // Sollwert
  if(window._sp!==undefined && window._sp>lo && window._sp<hi){
    o+='<line x1="'+L+'" y1="'+y(window._sp)+'" x2="'+(W-R)+'" y2="'+y(window._sp)
      +'" stroke="#4fd98a" stroke-dasharray="5 4"/>';
  }

  // Dosierbalken
  var bw=Math.max(1,(W-L-R)/n*0.7);
  for(var i=0;i<n;i++){
    if(h.ml[i]>0)
      o+='<rect x="'+(x(i)-bw/2)+'" y="'+yb(h.ml[i])+'" width="'+bw
        +'" height="'+((H-B)-yb(h.ml[i]))+'" fill="#ff8a00"/>';
  }
  // ml-Achse rechts
  o+='<text x="'+(W-R+6)+'" y="'+(H-B)+'" fill="#ff8a00" font-size="11">0</text>';
  o+='<text x="'+(W-R+6)+'" y="'+(yb(mlMax)+4)+'" fill="#ff8a00" font-size="11">'
    +mlMax.toFixed(1)+' ml</text>';

  // Schwankungsband und Linie, Luecken werden nicht ueberbrueckt
  var band='', line='', open=false;
  for(var i=0;i<n;i++){
    if(h.ph[i]===null){ open=false; continue; }
    line += (open?'L':'M')+x(i)+' '+y(h.ph[i])+' ';
    open=true;
    if(h.max[i]-h.min[i]>0.005)
      band+='<rect x="'+(x(i)-bw/2)+'" y="'+y(h.max[i])+'" width="'+bw
        +'" height="'+Math.max(1,y(h.min[i])-y(h.max[i]))+'" fill="#5a4a12"/>';
  }
  o+=band;
  o+='<path d="'+line+'" fill="none" stroke="#ffc61a" stroke-width="2"/>';
  // Fadenkreuz, wird beim Zeigen verschoben statt neu gezeichnet
  o+='<line id="cur" x1="0" y1="'+T+'" x2="0" y2="'+(H-B)
    +'" stroke="#fff" stroke-opacity="0.45" style="display:none"/>';
  o+='<circle id="curdot" r="4" fill="#ffc61a" style="display:none"/>';
  o+='</svg>';
  el('chart').innerHTML=o;

  // Geometrie für die Zeigerauswertung merken
  _cg = {n:n, W:W, L:L, R:R, T:T, B:B, H:H, first:h.first, y:y, yb:yb, x:x};
  _hist = h;
  bindChartPointer();

  // Tagessummen
  var d0=0,d1=0,d7=0;
  for(var i=0;i<n;i++){
    d7+=h.ml[i];
    if(i>=n-24) d0+=h.ml[i];
    else if(i>=n-48) d1+=h.ml[i];
  }
  el('hd0').textContent=d0.toFixed(1)+' ml';
  el('hd1').textContent=d1.toFixed(1)+' ml';
  el('hd7').textContent=d7.toFixed(1)+' ml';
}

var _hist=null, _cg=null;

// Stundenwert unter dem Zeiger bestimmen. Das SVG skaliert über width:100%,
// deshalb wird die Pixelposition erst zurück in Diagrammkoordinaten gerechnet.
function chartIndexAt(clientX, svg){
  var r = svg.getBoundingClientRect();
  var sx = (clientX - r.left) * (_cg.W / r.width);
  var span = (_cg.W - _cg.L - _cg.R) / Math.max(1, _cg.n - 1);
  var i = Math.round((sx - _cg.L) / span);
  return Math.max(0, Math.min(_cg.n - 1, i));
}

var WD = ['So','Mo','Di','Mi','Do','Fr','Sa'];

function showTip(clientX){
  if(!_cg || !_hist) return;
  var wrap = el('chartwrap'), svg = wrap.querySelector('svg'), tip = el('tip');
  if(!svg) return;
  var i = chartIndexAt(clientX, svg);
  var h = _hist, d = new Date((h.first + i) * 3600000);
  var ph = h.ph[i], mn = h.min[i], mx = h.max[i], ml = h.ml[i];

  var html = '<div class="t">' + WD[d.getDay()] + ' ' + d.getDate() + '.'
           + (d.getMonth()+1) + '. &middot; '
           + ('0'+d.getHours()).slice(-2) + ':00&ndash;'
           + ('0'+d.getHours()).slice(-2) + ':59</div>';
  if(ph === null){
    html += '<div class="m">kein Messwert</div>';
  } else {
    html += 'pH <b>' + ph.toFixed(2) + '</b>';
    if(mx - mn > 0.005)
      html += ' <span class="m">(' + mn.toFixed(2) + ' &ndash; '
            + mx.toFixed(2) + ')</span>';
    html += '<br>';
  }
  html += '<span class="o">' + (ml > 0 ? ml.toFixed(1) + ' ml dosiert'
                                       : 'nicht dosiert') + '</span>';
  tip.innerHTML = html;

  // Fadenkreuz mitführen
  var r = svg.getBoundingClientRect(), sc = r.width / _cg.W;
  var cx = _cg.x(i);
  var cur = svg.querySelector('#cur'), dot = svg.querySelector('#curdot');
  cur.setAttribute('x1', cx); cur.setAttribute('x2', cx);
  cur.style.display = '';
  if(ph === null){ dot.style.display = 'none'; }
  else {
    dot.setAttribute('cx', cx); dot.setAttribute('cy', _cg.y(ph));
    dot.style.display = '';
  }

  // Kasten links vom Zeiger, wenn er sonst rechts hinausliefe
  tip.style.display = 'block';
  var px = cx * sc, w = tip.offsetWidth;
  tip.style.left = (px + w + 14 > r.width ? px - w - 12 : px + 12) + 'px';
  tip.style.top  = '8px';
}

function hideTip(){
  var wrap = el('chartwrap'); if(!wrap) return;
  var svg = wrap.querySelector('svg');
  el('tip').style.display = 'none';
  if(svg){
    var c = svg.querySelector('#cur'), d = svg.querySelector('#curdot');
    if(c) c.style.display = 'none';
    if(d) d.style.display = 'none';
  }
}

// Wird nach jedem Neuzeichnen aufgerufen - das SVG ist dann ein neues Element.
function bindChartPointer(){
  var wrap = el('chartwrap'), svg = wrap.querySelector('svg');
  if(!svg) return;
  svg.addEventListener('mousemove', function(e){ showTip(e.clientX); });
  svg.addEventListener('mouseleave', hideTip);
  // Auf dem Tablet: Finger ziehen zeigt genauso
  svg.addEventListener('touchstart', function(e){
    showTip(e.touches[0].clientX); e.preventDefault(); }, {passive:false});
  svg.addEventListener('touchmove', function(e){
    showTip(e.touches[0].clientX); e.preventDefault(); }, {passive:false});
  svg.addEventListener('touchend', hideTip);
}

function loadChart(){
  // Nicht neu zeichnen, solange jemand im Chart liest - das SVG wird
  // dabei ersetzt und der Zeiger verloere seinen Bezugspunkt.
  if(el('tip') && el('tip').style.display === 'block') return;
  fetch('/api/history').then(function(r){return r.json();})
   .then(drawChart).catch(function(){});
}

refresh();
setInterval(refresh, 2000);
loadChart();
setInterval(loadChart, 300000);   // Verlauf aendert sich stuendlich - alle 5 min reicht
</script>
</body>
</html>
)rawliteral";
