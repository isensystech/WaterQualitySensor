#pragma once
// Captive-portal page. Kept in a header so the Arduino .ino preprocessor does NOT
// inject #line/prototype markers into the raw string literal (which truncates it).
static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<style>
:root{--accent:#19c3c3}
body{font-family:system-ui;margin:0;padding:16px;background:#0c1020;color:#e8eaf0}
h2{margin:.2em 0;color:var(--accent)}h3{margin:.3em 0}
.c{background:#161b30;border-radius:12px;padding:14px;margin:10px 0}
label{display:block;font-size:13px;color:#9aa3c0;margin:8px 0 2px}
input,select,textarea{width:100%;box-sizing:border-box;padding:10px;border-radius:8px;border:1px solid #2a3252;background:#0c1020;color:#fff;font-size:16px}
input[type=range]{accent-color:var(--accent);padding:6px 0;height:30px}
.dimv{color:var(--accent);font-weight:700}
button{width:100%;padding:14px;border:0;border-radius:10px;font-size:16px;margin-top:10px;color:#fff}
.b1{background:var(--accent)}.b2{background:#185fa5}.bk{background:#2a3252}.bd{background:#7a2230}
.ok{color:#5dcaa5}.warn{color:#efb02a}
.tg{display:grid;grid-template-columns:52px 1fr 1fr 1fr 1fr;gap:4px;align-items:center;margin:4px 0}
.t{padding:8px 4px;font-size:14px;text-align:center}.hint{font-size:12px;color:#9aa3c0;margin:4px 0}
.view{display:none}.view.on{display:block}.menu button{text-align:left}
.lg{display:grid;grid-template-columns:24px 1fr auto auto;gap:8px;align-items:center;padding:6px 0;border-bottom:1px solid #2a3252;font-size:14px}
.lg a{color:var(--accent);text-decoration:none;font-weight:600}
.ver{font-size:12px;color:#9aa3c0;margin-bottom:6px}
details{background:#161b30;border:1px solid #2a3252;border-radius:12px;margin:10px 0;overflow:hidden}
details[open]{border-color:var(--accent)}
summary{list-style:none;cursor:pointer;padding:14px;font-weight:600;display:flex;align-items:center}
summary::-webkit-details-marker{display:none}
summary::after{content:"+";margin-left:auto;color:var(--accent);font-weight:700}
details[open] summary::after{content:"\2013"}
.db{padding:0 14px 14px;font-size:14px;line-height:1.5;color:#cdd3e6}
.db p{margin:8px 0}.db ul{margin:8px 0;padding-left:18px}.db li{margin:4px 0}
.db table{width:100%;border-collapse:collapse;font-size:13px;margin:8px 0}
.db th,.db td{text-align:left;padding:6px 8px;border-bottom:1px solid #2a3252;vertical-align:top}
.db th{color:#9aa3c0}
code,.k{background:#0c1020;border:1px solid #2a3252;border-radius:5px;padding:1px 6px;font-family:ui-monospace,monospace;font-size:13px;color:var(--accent)}
.danger{border-left:3px solid #ff3b30;background:rgba(255,59,48,.10);padding:10px 12px;border-radius:0 8px 8px 0;margin:10px 0}
.pill{font-family:ui-monospace,monospace;font-size:12px;padding:1px 6px;border-radius:4px;border:1px solid #2a3252}
.pg{color:#27c93f}.pr{color:#ff3b30}.py{color:#ffa400}
.sp{display:flex;gap:10px;margin:6px 0}.sp b{color:var(--accent);min-width:92px}
.swatches{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin:8px 0 2px}
.sw{margin:0;padding:14px 6px;border:2px solid #2a3252;border-radius:10px;background:#0c1020;color:#cdd3e6;font-size:13px;cursor:pointer;display:flex;flex-direction:column;align-items:center;gap:8px}
.sw::before{content:"";width:26px;height:26px;border-radius:50%;background:var(--sw);border:2px solid rgba(255,255,255,.25)}
.sw.sel{border-color:var(--sw);color:#fff;box-shadow:0 0 0 1px var(--sw)}
.dimrow{display:flex;align-items:center;gap:10px}
.dimrow input[type=range]{flex:1}
.dimrow input[type=number]{width:88px;flex:none;text-align:center}
.unit{color:#9aa3c0;font-size:14px}
.sens{background:#161b30;border:2px solid #2a3252;border-radius:12px;padding:14px;margin:10px 0}
.sens.found{border-color:#27c93f}.sens.absent{border-color:#ff3b30}
.sens input[type=checkbox]{width:auto;margin-right:8px;vertical-align:middle}
.shead{display:flex;align-items:center;justify-content:space-between;gap:8px}
.shead h3{margin:0}
.det{font-size:12px;font-weight:700;white-space:nowrap}
.okd{color:#27c93f}.badd{color:#ff3b30}
</style></head><body>
<h2>WaterQuality Logger</h2><div class=ver id=ver></div>

<div class="view on" id=v_home>
<div class=c id=synccard style="display:none">
<b class=warn>Clock needs a sync</b>
<div class=hint id=synmsg>The logger's time is approximate &mdash; sync it from this phone for accurate timestamps.</div>
<button class=b2 onclick=sync()>Re-sync from this phone</button></div>
<div class="c menu">
<button class=b1 onclick="go('mission')">START MISSION</button>
<button class=b1 onclick="go('settings')">SETTINGS</button>
<button class=b1 onclick="go('logs')">DOWNLOAD LOGS</button>
<button class=b1 onclick="go('help')">HELP</button>
<button class=b1 onclick="go('about')">ABOUT</button>
</div>
<div id=msg></div></div>

<div class=view id=v_mission>
<div class=c><h3>Mission details</h3>
<label>Mission</label><input id=mission>
<label>Operator</label><input id=op>
<label>Site</label><input id=site>
<label>Notes</label><textarea id=notes rows=2></textarea></div>
<div class=c><h3>Location</h3>
<label>Coordinates or a Maps link</label><input id=gps onchange=gpsChanged() placeholder='28.10855 N, 80.67847 W'>
<p class=hint>Accepts 28.10855&deg; N, 80.67847&deg; W &bull; 27.99,-80.58 &bull; or a Maps link. Sets weather automatically (needs internet).</p>
<button class=b2 onclick=geo()>Try phone GPS (only over https)</button>
<label>Weather (auto if online, else type)</label><input id=wx placeholder="e.g. clear 26C">
<button class=b2 onclick=wx()>Try auto weather</button></div>
<div class=c><h3>Water type</h3>
<select id=wt><option>ocean</option><option>estuary</option><option>lake</option><option>river</option><option>pool</option></select></div>
<button class=b1 onclick=start()>Save &amp; start mission</button>
<button class=bk onclick="go('home')">Back</button></div>

<div class=view id=v_settings>

<div class=c><h3>Display colour</h3>
<p class=hint>Tap a colour to theme the device screen and this page.</p>
<div class=swatches id=swatches>
<button type=button class=sw data-i=0 style="--sw:#19c3c3" onclick=pickAccent(0)>Teal</button>
<button type=button class=sw data-i=1 style="--sw:#ffa400" onclick=pickAccent(1)>Orange</button>
<button type=button class=sw data-i=2 style="--sw:#27c93f" onclick=pickAccent(2)>Green</button>
<button type=button class=sw data-i=3 style="--sw:#ff3b30" onclick=pickAccent(3)>Red</button>
</div>
<input type=hidden id=accent value=0></div>

<div class=c><h3>Screen dimming</h3>
<label>Fade the display off after this many minutes idle:</label>
<div class=dimrow>
<input id=dim type=range min=1 max=240 step=1 value=10 oninput="syncDim('r')">
<input id=dimn type=number min=1 max=240 step=1 value=10 oninput="syncDim('n')"><span class=unit>min</span>
</div>
<p class=hint>Saves battery on long dives. A press wakes the screen &mdash; that first wake-press won't drop a marker or flip the page.</p></div>

<div class=c><h3>Warning thresholds</h3>
<p class=hint>Blank = no alarm for that bound. Warn = amber tile, alarm = solid red.</p>
<div class=tg style="color:#9aa3c0;font-size:11px"><span></span><span>warn lo</span><span>warn hi</span><span>alm lo</span><span>alm hi</span></div>
<div class=tg><span>Temp C</span><input class=t id=temp_wlo type=number step=any inputmode=decimal><input class=t id=temp_whi type=number step=any inputmode=decimal><input class=t id=temp_alo type=number step=any inputmode=decimal><input class=t id=temp_ahi type=number step=any inputmode=decimal></div>
<div class=tg><span>pH</span><input class=t id=ph_wlo type=number step=any inputmode=decimal><input class=t id=ph_whi type=number step=any inputmode=decimal><input class=t id=ph_alo type=number step=any inputmode=decimal><input class=t id=ph_ahi type=number step=any inputmode=decimal></div>
<div class=tg><span>ORP mV</span><input class=t id=orp_wlo type=number step=any inputmode=decimal><input class=t id=orp_whi type=number step=any inputmode=decimal><input class=t id=orp_alo type=number step=any inputmode=decimal><input class=t id=orp_ahi type=number step=any inputmode=decimal></div>
<div class=tg><span>EC mS</span><input class=t id=ec_wlo type=number step=any inputmode=decimal><input class=t id=ec_whi type=number step=any inputmode=decimal><input class=t id=ec_alo type=number step=any inputmode=decimal><input class=t id=ec_ahi type=number step=any inputmode=decimal></div>
<div class=tg><span>Sal PSU</span><input class=t id=sal_wlo type=number step=any inputmode=decimal><input class=t id=sal_whi type=number step=any inputmode=decimal><input class=t id=sal_alo type=number step=any inputmode=decimal><input class=t id=sal_ahi type=number step=any inputmode=decimal></div>
<div class=tg><span>Depth m</span><input class=t id=depth_wlo type=number step=any inputmode=decimal><input class=t id=depth_whi type=number step=any inputmode=decimal><input class=t id=depth_alo type=number step=any inputmode=decimal><input class=t id=depth_ahi type=number step=any inputmode=decimal></div>
<div class=tg><span>Fluor</span><input class=t id=cyc_wlo type=number step=any inputmode=decimal><input class=t id=cyc_whi type=number step=any inputmode=decimal><input class=t id=cyc_alo type=number step=any inputmode=decimal><input class=t id=cyc_ahi type=number step=any inputmode=decimal></div></div>

<div class=c><h3>Sensors</h3>
<p class=hint>Sensors the logger detects on its I2C bus are enabled automatically. Untick one you're not using and it's left out (screen shows &mdash;, log column blank). A <span class=okd>green</span> frame means detected, <span class=badd>red</span> means not found. Plug a sensor in, then Re-scan.</p>
<button class=b2 onclick=scanSensors()>Re-scan sensors</button></div>

<div class=sens id=sens_poet>
<div class=shead><h3>POET multiparameter</h3><span class="det" id=det_poet>&hellip;</span></div>
<label><input type=checkbox id=poet_en>Enabled &mdash; pH / ORP / EC / salinity (0x1F)</label></div>

<div class=sens id=sens_bar30>
<div class=shead><h3>BAR30 depth</h3><span class="det" id=det_bar30>&hellip;</span></div>
<label><input type=checkbox id=bar30_en>Enabled &mdash; pressure / depth / temperature (0x76)</label></div>

<div class=sens id=sens_cels>
<div class=shead><h3>Blue Robotics Celsius</h3><span class="det" id=det_cels>&hellip;</span></div>
<label><input type=checkbox id=cels_en>Enabled &mdash; high-accuracy temperature (0x77)</label></div>

<div class=sens id=sens_cyc>
<div class=shead><h3>Cyclops fluorometer</h3><span class="det" id=det_cyc>&hellip;</span></div>
<label><input type=checkbox id=cyc_en>Enabled &mdash; 0-5 V via ADS1015 ADC (0x48)</label>
<label>Units label</label><input id=cyc_u placeholder="ug/L, NTU, ppb...">
<label>Calibration standard concentration</label><input id=cyc_s type=number step=any inputmode=decimal placeholder="e.g. 100">
<p class=hint>Set the standard's value, then run the device CAL menu &rarr; "Cyclops (2-pt)" (blank, then standard).</p></div>

<div class=c><h3>Device</h3>
<div id=ts class=warn>checking...</div>
<button class=b2 onclick=sync()>Re-sync time from this phone</button>
<button class=b2 onclick=cal()>Enter calibration mode (on device)</button>
<p class=hint>Calibration switches the logger into its on-device wizard and turns Wi-Fi off.</p></div>

<div class=c><h3>Firmware update</h3>
<p class=hint>Current firmware: <b id=fwver>&hellip;</b></p>
<label>Firmware file (.bin)</label>
<input type=file id=fwfile accept=".bin,application/octet-stream">
<button class=b2 onclick=otaUpload()>Update firmware from file&hellip;</button>
<div id=otawrap style="display:none;margin-top:10px">
<div style="background:#0c1020;border:1px solid #2a3252;border-radius:6px;height:14px;overflow:hidden">
<div id=otafill style="height:100%;width:0%;background:var(--accent);transition:width .2s"></div></div>
<div class=hint id=otamsg></div></div>
<p class=hint>Download the firmware <code>.bin</code> to this phone first (needs internet), then upload it here over the logger's Wi-Fi. The logger checks the file, flashes itself and reboots &mdash; rejoin <code>WaterQuality-Logger</code> after about 10&nbsp;seconds. <b>Don't power the logger off during the update.</b></p></div>

<button class=b1 onclick=saveSettings()>Save settings</button>
<button class=bk onclick="go('home')">Back</button></div>

<div class=view id=v_logs>
<div class=c><h3>Dive logs</h3>
<div id=loglist class=hint>loading...</div>
<button class=b2 onclick=dlSel()>Download selected</button>
<button class=b2 onclick="location.href='/api/logall'">Download all (combined)</button>
<button class=bd onclick=clearLogs()>Clear logs (delete all)</button>
<p class=hint>"Download selected" saves each file individually (your phone may ask to allow multiple downloads). "Download all" streams every dive into one combined .csv. "Clear logs" permanently deletes every dive file from the card.</p></div>
<button class=bk onclick="go('home')">Back</button></div>

<div class=view id=v_help>
<h3>Help &amp; Field Guide</h3>
<p class=hint>Tap a topic to open it. New to the logger? Start at the top.</p>

<details open><summary>&#9889; Quick start &mdash; first two minutes</summary><div class=db>
<ul>
<li>Power on. The screen shows the firmware version, then <span class=k>SD</span> status and the Wi-Fi name and address.</li>
<li>On the surface the logger makes its own Wi-Fi: join <code>WaterQuality-Logger</code>, then open <code>192.168.4.1</code> in a <b>normal browser tab</b> &mdash; not the pop-up that may appear on its own.</li>
<li>Use <b>START MISSION</b> to record where you are and what you're measuring. The clock syncs from your phone automatically (a prompt appears on the home screen if it needs attention).</li>
<li>Put the logger in the water. It detects the water and <b>starts recording on its own</b> &mdash; no buttons needed.</li>
<li>When you surface and lift it out, recording <b>stops on its own</b>. Come back here to download your data.</li>
</ul>
<p>The only control on the logger itself is the <b>push button</b> (see the next topic).</p>
</div></details>

<details><summary>&#127913; The push button</summary><div class=db>
<p>There is one control: the push button. It does different things depending on how long you hold it.</p>
<table>
<tr><th>Action</th><th>What happens</th></tr>
<tr><td><b>Quick press</b> (tap)</td><td>Drops a <b>marker</b> (POI &mdash; point of interest) into the log at this exact moment. A blue <span class=k>POI #</span> badge confirms it.</td></tr>
<tr><td><b>Hold &amp; release</b></td><td>Flips between the two screens (<b>DIVE</b> and <b>WATER</b>).</td></tr>
<tr><td><b>Hold during power-on</b></td><td>Enters <b>Calibration</b> mode (see the Calibration topic).</td></tr>
</table>
<p>If you tap to mark a point but the logger isn't recording yet, it shows <span class=k>NOT LOGGING</span> so you know nothing was saved.</p>
</div></details>

<details><summary>&#128250; Reading the screen</summary><div class=db>
<p>Hold-and-release flips between two screens:</p>
<ul>
<li><b>DIVE screen</b> &mdash; big <b>depth</b>, plus how fast you're going up or down (<span class=k>UP</span>/<span class=k>DOWN</span>/<span class=k>HOLD</span> in m/min), temperature, pH and salinity.</li>
<li><b>WATER screen</b> &mdash; the full set: temperature, pH, ORP, conductivity (EC), salinity and depth (or the fluorometer if fitted).</li>
</ul>
<p><b>Tile colours</b> show whether a reading is in range &mdash; using the bands you set under <b>Settings</b>:</p>
<ul>
<li><b>Outlined</b> = normal / in range.</li>
<li><b>Amber ring</b> = warning, close to a limit.</li>
<li><b>Solid red</b> = alarm, outside your limit.</li>
</ul>
<p>A reading shows <code>--</code> when there's nothing to show yet &mdash; usually the logger isn't in water, or that sensor isn't connected or is switched off under <b>Settings</b>.</p>
<p><b>The status line</b> along the bottom is a quick health check:</p>
<p><span class="pill pg">SD</span> card OK &bull; <span class="pill pr">SD!</span> card problem, data may not be saving.</p>
<p><span class="pill pg">LOG</span> recording &bull; <span class="pill py">IDLE</span> not recording.</p>
<p><span class="pill pg">t</span> clock set &bull; <span class="pill py">t~</span> approximate &bull; <span class="pill pr">t!</span> no time yet (sync from your phone).</p>
<p><span class="pill pg">POI:</span> markers this dive &bull; <span class="pill pg">n:</span> readings taken.</p>
<p><span class="pill pg">WET</span> in water &bull; <span class="pill pr">AIR</span> out of water.</p>
</div></details>

<details><summary>&#128268; Sensors &mdash; turn them on or off</summary><div class=db>
<p>The logger carries its sensors on one shared cable, and you choose which are active under <b>SETTINGS &rarr; Sensors</b>. Each one has its own card:</p>
<ul>
<li><b>POET</b> &mdash; pH, ORP, conductivity and salinity.</li>
<li><b>BAR30</b> &mdash; depth, pressure and temperature.</li>
<li><b>Celsius</b> &mdash; an optional high-accuracy temperature probe. When it's fitted, it becomes the temperature shown on screen.</li>
<li><b>Cyclops</b> &mdash; the optional fluorometer.</li>
</ul>
<p><b>It sets itself up.</b> On power-up the logger checks the cable and <b>switches on every sensor it finds</b>, so usually there's nothing to do. Each card shows a <span class=okd>green</span> frame when that sensor is detected and a <span class=badd>red</span> frame when it isn't &mdash; a quick way to confirm everything's plugged in.</p>
<p><b>To leave a sensor out</b> &mdash; you're not using it, or you want to silence a faulty one &mdash; untick its box and <b>Save settings</b>. That sensor then shows <code>--</code> on screen and its columns are left blank in the log; nothing else is affected.</p>
<p><b>Just plugged one in?</b> Tap <b>Re-scan sensors</b>: its frame turns green, tick it if needed, then Save. No reboot required.</p>
<p>The boot <span class=k>SENSORS</span> screen mirrors this: <span class="pill pg">OK</span> detected &amp; on &bull; <span class="pill pr">FAILED</span> on but not found &bull; <span class="pill">off</span> switched off.</p>
</div></details>

<details><summary>&#127754; During a dive</summary><div class=db>
<ul>
<li><b>Recording is automatic.</b> The logger senses when it's underwater, starts a new file, and closes it when you surface.</li>
<li><b>Mark interesting spots</b> with a quick press (a fish, a pipe, a colour change). Each marker is saved with its timestamp and all the readings at that instant.</li>
<li><b>Watch the ascent number.</b> Coming up too fast turns the rate tile amber, then red. Slow is safer for you <i>and</i> the sensors.</li>
<li>Wi-Fi switches off once a dive begins (useless underwater, saves battery), so this page isn't reachable mid-dive. It returns on the surface.</li>
</ul>
<div class=danger><b>Safety first:</b> this is a <b>science data logger, not a dive computer</b>. The depth and ascent numbers are for your data, not for planning your dive or no-decompression limits. Always dive with a proper dive computer and within your training.</div>
</div></details>

<details><summary>&#129514; Calibration &mdash; how &amp; when</summary><div class=db>
<p>Calibration teaches the logger what known samples read like, so your measurements are accurate. Do it on the bench with the stack out of the housing.</p>
<p><b>To start:</b> hold the push button while powering on (or tap the calibration button under Settings). You'll get a step-by-step wizard with a <b>stability bar</b> &mdash; wait for it to settle before capturing. In the wizard, a <b>quick press captures</b> (or cycles a choice) and a <b>long hold cancels</b> (or selects). When done it saves and restarts on its own.</p>
<p><b>What you can calibrate:</b></p>
<ul>
<li><b>pH</b> &mdash; 3-point using buffers <b>4.00, 7.00 and 10.00</b>. Rinse the probe between buffers; capture each when stable.</li>
<li><b>Conductivity (EC)</b> &mdash; K1.0 standards (<b>12,880</b> and <b>80,000&nbsp;&micro;S/cm</b>). Pick the standard, let it stabilise, capture.</li>
<li><b>Fluorometer</b> (if fitted) &mdash; 2-point: a clean-water <b>blank</b>, then a <b>known standard</b>. Set its value and units first under Settings.</li>
<li><b>ORP</b> &mdash; <b>not enabled yet</b>; coming once the correct standard (quinhydrone) is in the kit. Skip for now.</li>
</ul>
<p><b>How often (good practice for class use):</b></p>
<table>
<tr><th>Sensor</th><th>Suggested cadence</th></tr>
<tr><td>pH</td><td>Before a field day or weekly under heavy use; whenever readings look off or after long storage.</td></tr>
<tr><td>EC</td><td>Less often &mdash; it's stable. Each season/deployment block, or if it drifts.</td></tr>
<tr><td>Fluorometer</td><td>Each session, against your standard.</td></tr>
</table>
<p>Always re-check after cleaning a sensor, after long storage, or whenever a value seems wrong.</p>
</div></details>

<details><summary>&#129532; Care &amp; maintenance</summary><div class=db>
<ul>
<li><b>Rinse after every use</b> with clean (ideally deionised) water &mdash; especially after salt or brackish water. Salt corrodes and clogs sensors.</li>
<li><b>Never store the electrochemical sensors dry.</b> Keep them moist per the sensor maker's instructions; a dried-out pH sensor can be slow or ruined.</li>
<li><b>Don't touch the sensor faces</b> with fingers, and don't wipe them hard &mdash; rinse and let drips fall.</li>
<li><b>Check the O-ring seals before every dive.</b> Clean off grit, look for nicks, add a thin film of silicone grease. A good seal is your main defence against flooding.</li>
<li><b>Leak alarm:</b> if the logger <b>beeps rapidly</b>, water may have gotten inside &mdash; end the dive, surface safely, dry everything out, and inspect the seals before reuse.</li>
<li><b>Charging:</b> slide the stack out of the housing to charge. Charge after use; don't store the battery flat.</li>
<li><b>microSD card:</b> make sure it's seated. Don't pull it while the status line shows <span class=k>LOG</span>.</li>
<li><b>Depth:</b> stay within the housing's rated depth.</li>
</ul>
</div></details>

<details><summary>&#128260; Updating the firmware</summary><div class=db>
<p>Firmware is the logger's built-in software. Updates add features and fixes. Because the logger is <b>sealed</b>, updates go over its own Wi-Fi &mdash; no cable, no opening the housing.</p>
<p><b>It's a two-step trip</b> (the logger's Wi-Fi has no internet, so one phone can't do both at once):</p>
<ol>
<li><b>Get the file first, on the internet.</b> On your phone or laptop &mdash; while on normal Wi-Fi or cellular &mdash; download the firmware file (it ends in <code>.bin</code>) that the team sent you. Note where it saved (usually <b>Downloads</b>).</li>
<li><b>Then upload it to the logger.</b> Join the logger's Wi-Fi <code>WaterQuality-Logger</code>, open <code>192.168.4.1</code>, go to <b>SETTINGS &rarr; Firmware update</b>, choose the <code>.bin</code> you downloaded, and confirm.</li>
</ol>
<p>The logger checks the file, installs it, and restarts on its own. The screen shows <span class=k>UPDATING</span> with a progress bar; when it finishes it reboots. Rejoin <code>WaterQuality-Logger</code> after about 10&nbsp;seconds and re-open this page &mdash; the version shown at the top should be the new one.</p>
<div class=danger><b>Don't power the logger off while it says UPDATING.</b> Charge the battery before you start. If an upload fails or is interrupted, the logger simply keeps its <b>old</b> firmware &mdash; just try again.</div>
<p><b>Safety net (recovery mode):</b> if an update ever leaves the logger misbehaving, hold the push button while powering on and <b>keep holding past the calibration prompt</b> until the screen reads <span class=k>RECOVERY</span>. That brings up a stripped-down Wi-Fi that does one thing &mdash; re-flash firmware. Join <code>WaterQuality-Logger</code>, open <code>192.168.4.1</code>, upload a known-good <code>.bin</code>. Power-cycle to leave.</p>
<p class=hint>Only upload firmware meant for <b>this</b> logger. It rejects files that aren't for its chip, but it can't tell two builds made for it apart &mdash; so use the file the team gave you.</p>
</div></details>

<details><summary>&#128190; Your data &amp; log files</summary><div class=db>
<p>Everything saves to the microSD card as plain <b>CSV</b> you can open in Excel, Google Sheets, or any spreadsheet.</p>
<ul>
<li><b>One file per dive</b>, named like <code>dive*.csv</code>, with a header recording the mission, operator, site, water type and location you entered.</li>
<li>Each row is one reading with its time; <b>marker (POI) rows are flagged</b> so you can find them.</li>
<li><b>To download:</b> use <b>DOWNLOAD LOGS</b> &mdash; tick the dives you want and "Download selected", or grab them all combined.</li>
<li><b>No phone handy?</b> Power off, pop out the microSD, and read it on any computer.</li>
</ul>
<p><b>Other files on the card</b> (leave alone unless you know why):</p>
<table>
<tr><th>File</th><th>What it is</th></tr>
<tr><td><code>cal.json</code></td><td>Active calibration. <b>Delete this and you lose your calibration.</b></td></tr>
<tr><td><code>callog.csv</code></td><td>History of every calibration (audit trail).</td></tr>
<tr><td><code>state.json</code></td><td>Settings, last mission info, time and alarm thresholds.</td></tr>
</table>
<p><b>Back up dive files before clearing the card</b>, and don't remove the card mid-recording. (The DOWNLOAD LOGS screen offers a combined-copy download before "Clear logs".)</p>
</div></details>

<details><summary>&#128505; What the numbers mean</summary><div class=db>
<table>
<tr><th>Reading</th><th>Meaning</th></tr>
<tr><td><b>Depth</b> (m)</td><td>How deep, in metres, from water pressure.</td></tr>
<tr><td><b>Temp</b> (&deg;C)</td><td>Water temperature.</td></tr>
<tr><td><b>pH</b></td><td>Acidity vs. alkalinity, 0&ndash;14 (7 is neutral). No units.</td></tr>
<tr><td><b>ORP</b> (mV)</td><td>Oxidation-reduction potential &mdash; how oxidising or reducing the water is.</td></tr>
<tr><td><b>EC</b> (mS/cm)</td><td>Conductivity &mdash; how well the water carries current; rises with dissolved salts.</td></tr>
<tr><td><b>Sal</b> (PSU)</td><td>Salinity, from conductivity, temperature and pressure.</td></tr>
<tr><td><b>Fluor</b></td><td>Fluorometer reading (e.g. chlorophyll or turbidity), if a Cyclops is fitted.</td></tr>
</table>
</div></details>

<details><summary>&#128679; Troubleshooting</summary><div class=db>
<table>
<tr><th>You see&hellip;</th><th>Likely fix</th></tr>
<tr><td>Readings show <code>--</code></td><td>Not in water (status <span class="pill pr">AIR</span>), or that sensor isn't connected or is switched off. Submerge it, check the plug, or re-enable it under <b>Settings &rarr; Sensors</b> (<b>Re-scan</b> to detect it).</td></tr>
<tr><td><span class="pill pr">SD!</span></td><td>Card problem. Re-seat the microSD or try another card. Data isn't saving until this clears.</td></tr>
<tr><td><span class="pill pr">t!</span> or <span class="pill py">t~</span></td><td>Clock not set / approximate. Connect your phone and re-sync time on the home screen.</td></tr>
<tr><td>Tiles turn red</td><td>A reading crossed an alarm limit &mdash; or your limits need adjusting under Settings.</td></tr>
<tr><td>Charge light doesn't come on</td><td>Check the cable and that the stack is seated on the charger.</td></tr>
<tr><td>Rapid beeping</td><td>Possible leak &mdash; surface safely, dry out, inspect seals before reuse.</td></tr>
<tr><td>Can't set GPS / get weather</td><td>Phone location and weather need internet, which the captive Wi-Fi doesn't provide. Paste coordinates or a Maps link, or type the weather.</td></tr>
</table>
</div></details>

<button class=bk onclick="go('home')">Back</button></div>

<div class=view id=v_about>
<div class=c>
<p><b>Dive WaterQuality Logger</b></p>
<p class=hint>Engineered by iSENSYS for Fieldwerx.</p>
<p class=hint>Project lead &amp; Mechanical Engineering &mdash; Jessica Foley<br>Electrical Engineering &amp; Software Stack &mdash; Scott McLeslie<br>Design &mdash; Katrin Barshe</p>
<p class=hint id=aver></p>
<p class=hint>&copy; 2026</p></div>

<details open><summary>&#128300; What it does</summary><div class=db>
<p>The logger reads several water-quality measurements many times a minute and saves them to the SD card, so you can study what the water was doing during a dive or deployment.</p>
<ul>
<li>Logs <b>temperature, pH (ISFET), ORP, conductivity/salinity and depth</b>, plus an optional <b>Cyclops fluorometer</b>.</li>
<li><b>Records every dive automatically</b> to a spreadsheet-ready CSV, with operator metadata and per-mission alarm bands.</li>
<li>Lets you <b>mark points of interest</b> with a press and set <b>alarm limits</b> that colour the screen.</li>
</ul>
</div></details>

<details><summary>&#129518; What's inside</summary><div class=db>
<div class=sp><b>Brain</b><span>Seeed Studio XIAO ESP32-C6 (built-in Wi-Fi).</span></div>
<div class=sp><b>Water</b><span>POET multiparameter electrochemical sensor &mdash; pH, ORP, conductivity.</span></div>
<div class=sp><b>Depth</b><span>Blue Robotics BAR30 (MS5837-30BA) pressure/depth.</span></div>
<div class=sp><b>Temp</b><span>Optional Blue Robotics Celsius (TSYS01) high-accuracy temperature.</span></div>
<div class=sp><b>Fluorometer</b><span>Optional Cyclops-7F via ADS1015 ADC.</span></div>
<div class=sp><b>Display</b><span>2.0" ST7789 colour screen with microSD storage.</span></div>
<div class=sp><b>Control</b><span>A single sealed push button &mdash; the only button.</span></div>
<div class=sp><b>Housing</b><span>Pressure-safe; the stack slides out for charging and bench calibration.</span></div>
<div class=sp><b>Safety</b><span>Built-in leak detector and buzzer.</span></div>
</div></details>

<details><summary>&#127758; Designed for the field</summary><div class=db>
<p>Built to work where there's <b>no computer and often no phone</b>:</p>
<ul>
<li>Everything saves <b>locally to the SD card</b> &mdash; nothing needs the internet.</li>
<li>When a phone is handy, it connects to the logger's Wi-Fi only to <b>set the clock</b> and <b>record mission notes</b>, then gets out of the way.</li>
<li>Underwater the logger runs <b>on its own</b> and starts/stops recording by sensing the water.</li>
</ul>
</div></details>

<details><summary>&#127891; For learners</summary><div class=db>
<p>Made for <b>STEM classrooms and dive programs</b>. Readings use standard scientific units, so the data drops straight into spreadsheets, graphs and lab reports. The plain-language help, on-screen colour alarms and one-button operation keep the focus on the science, not the gadget.</p>
<div class=danger><b>Remember:</b> this is a <b>data logger, not a dive computer</b>. Never use its depth or ascent readings to plan a dive &mdash; always carry a proper dive computer and follow your training.</div>
</div></details>

<button class=bk onclick="go('home')">Back</button></div>

<script>
var TM=['temp','ph','orp','ec','sal','depth','cyc'],TB=['wlo','whi','alo','ahi'];
var ACC=['#19c3c3','#ffa400','#27c93f','#ff3b30'];
function id(x){return document.getElementById(x);}
function V(i){return id(i).value;}
function g(m){id('msg').innerHTML='<p class=warn>'+m+'</p>';}
function ok(m){id('msg').innerHTML='<p class=ok>'+m+'</p>';}
function applyAccent(i){document.documentElement.style.setProperty('--accent',ACC[+i]||ACC[0]);}
function markSwatch(i){var b=document.querySelectorAll('#swatches .sw');for(var k=0;k<b.length;k++)b[k].classList.toggle('sel',+b[k].dataset.i===+i);}
function pickAccent(i){id('accent').value=i;applyAccent(i);markSwatch(i);}
function clampDim(v){v=parseInt(v,10);if(isNaN(v))v=10;if(v<1)v=1;if(v>240)v=240;return v;}
function setDim(v){v=clampDim(v);id('dim').value=v;id('dimn').value=v;}
function syncDim(src){setDim(src==='n'?id('dimn').value:id('dim').value);}
function go(v){var n=document.getElementsByClassName('view');for(var i=0;i<n.length;i++)n[i].classList.remove('on');
id('v_'+v).classList.add('on');window.scrollTo(0,0);if(v==='logs')loadLogs();}
function sync(){fetch('/api/sync',{method:'POST',body:JSON.stringify({epoch_ms:Date.now()})})
.then(r=>r.text()).then(t=>{var e=id('ts');e.className='ok';e.textContent='time OK (synced '+new Date().toISOString().slice(11,19)+' UTC)';id('synccard').style.display='none';});}
function parseGps(s){if(!s)return '';s=s.trim();
var m=s.match(/@(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)/)||s.match(/[?&](?:q|ll|destination)=(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)/);
if(m)return m[1]+','+m[2];
var t=s.replace(/[^0-9.,nsewNSEW+-]/g,' ');
var re=/(-?\d+(?:\.\d+)?)\s*([nsewNSEW])?/g,c=[],x;
while((x=re.exec(t))!==null&&c.length<2){var v=parseFloat(x[1]);if(isNaN(v))continue;
var h=x[2]?x[2].toUpperCase():'';if(h==='S'||h==='W')v=-Math.abs(v);else if(h==='N'||h==='E')v=Math.abs(v);c.push(v);}
return c.length>=2?(c[0]+','+c[1]):'';}
function gpsChanged(){var p=parseGps(id('gps').value);if(p){id('gps').value=p;wx();}}
function geo(){if(!navigator.geolocation||!window.isSecureContext){g('GPS needs https - paste a Maps link or lat,lon instead');return;}
navigator.geolocation.getCurrentPosition(p=>{id('gps').value=p.coords.latitude.toFixed(5)+','+p.coords.longitude.toFixed(5);},e=>g('GPS blocked - paste a Maps link or lat,lon instead'));}
var WMO={0:'clear',1:'mainly clear',2:'partly cloudy',3:'overcast',45:'fog',48:'rime fog',51:'lt drizzle',53:'drizzle',55:'hvy drizzle',56:'frz drizzle',57:'frz drizzle',61:'lt rain',63:'rain',65:'hvy rain',66:'frz rain',67:'frz rain',71:'lt snow',73:'snow',75:'hvy snow',77:'snow grains',80:'rain showers',81:'rain showers',82:'hvy showers',85:'snow showers',86:'snow showers',95:'thunderstorm',96:'tstorm hail',99:'tstorm hail'};
function wmo(c){return WMO[c]||('code'+c);}
function wx(){var v=parseGps(id('gps').value).split(',');if(v.length<2){g('set location first');return;}
fetch('https://api.open-meteo.com/v1/forecast?latitude='+v[0]+'&longitude='+v[1]+'&current=temperature_2m,weather_code')
.then(r=>r.json()).then(j=>{id('wx').value=j.current.temperature_2m+'C, '+wmo(j.current.weather_code);}).catch(e=>g('no internet on this device - use an ethernet computer/cellular, or type it'));}
function cal(){fetch('/api/cal',{method:'POST'}).then(r=>r.text()).then(t=>{ok('Switching device to calibration mode...');}).catch(e=>{});}
function buildThresh(){var th={};TM.forEach(function(m){var o={};TB.forEach(function(b){var v=id(m+'_'+b).value;if(v!=='')o[b]=parseFloat(v);});if(Object.keys(o).length)th[m]=o;});return th;}
function fillState(s){if(!s)return;
if(s.ver){id('ver').textContent='firmware '+s.ver;id('aver').textContent='Firmware '+s.ver;var fv=id('fwver');if(fv)fv.textContent=s.ver;}
if(s.mission!=null)id('mission').value=s.mission;
if(s.op!=null)id('op').value=s.op;
if(s.site!=null)id('site').value=s.site;
if(s.wt)id('wt').value=s.wt;
if(s.notes!=null)id('notes').value=s.notes;
if(s.gps)id('gps').value=s.gps;
if(s.wx!=null)id('wx').value=s.wx;
if(s.accent!=null){id('accent').value=s.accent;applyAccent(s.accent);markSwatch(s.accent);}
if(s.poet_en!=null)id('poet_en').checked=s.poet_en;
if(s.bar30_en!=null)id('bar30_en').checked=s.bar30_en;
if(s.cels_en!=null)id('cels_en').checked=s.cels_en;
if(s.cyc_en!=null)id('cyc_en').checked=s.cyc_en;
if(s.cyc_u!=null)id('cyc_u').value=s.cyc_u;
if(s.cyc_s!=null)id('cyc_s').value=s.cyc_s;
if(s.det)paintDetect(s.det);
if(s.dim!=null)setDim(s.dim);
if(s.thresh)TM.forEach(function(m){var o=s.thresh[m]||{};TB.forEach(function(b){if(o[b]!=null)id(m+'_'+b).value=o[b];});});
var t=id('ts'),good=(s.synced&&!s.approx);
if(good){t.className='ok';t.textContent='time OK';id('synccard').style.display='none';}
else{t.className='warn';t.textContent=(s.approx?'time approximate':'no time set')+' \u2013 syncing...';
id('synmsg').textContent=(s.approx?"The logger's time is approximate":"The logger has no time set")+' \u2013 sync it from this phone for accurate timestamps.';
id('synccard').style.display='block';sync();}}
function loadState(){fetch('/api/state').then(r=>r.json()).then(fillState).catch(e=>{});}
function setSens(s,ok){var c=id('sens_'+s),d=id('det_'+s);if(!c)return;
c.classList.remove('found','absent');c.classList.add(ok?'found':'absent');
d.textContent=ok?'detected':'not found';d.className='det '+(ok?'okd':'badd');}
function paintDetect(d){if(!d)return;setSens('poet',d.poet);setSens('bar30',d.bar30);setSens('cels',d.cels);setSens('cyc',d.cyc);}
function scanSensors(){fetch('/api/scan').then(r=>r.json()).then(paintDetect).catch(e=>{});}
function payload(){return {mission:V('mission'),op:V('op'),site:V('site'),wt:V('wt'),accent:parseInt(V('accent')),gps:parseGps(V('gps')),wx:V('wx'),notes:V('notes'),
poet_en:id('poet_en').checked,bar30_en:id('bar30_en').checked,cels_en:id('cels_en').checked,
cyc_en:id('cyc_en').checked,cyc_u:V('cyc_u'),cyc_s:parseFloat(V('cyc_s')),dim:clampDim(V('dim')),thresh:buildThresh()};}
function commit(m){fetch('/api/deploy',{method:'POST',body:JSON.stringify(payload())}).then(r=>r.text()).then(t=>{ok(m);go('home');}).catch(e=>{ok(m);go('home');});}
function start(){commit('Mission saved. You can disconnect and dive.');}
function saveSettings(){commit('Settings saved.');}
function kb(n){return n<1024?n+' B':(n/1024).toFixed(1)+' KB';}
function loadLogs(){id('loglist').textContent='loading...';
fetch('/api/logs').then(r=>r.json()).then(function(a){
if(!a.length){id('loglist').innerHTML='<p>No dive logs yet.</p>';return;}
var h='';a.forEach(function(f){h+='<div class=lg><input type=checkbox class=lsel value="'+f.n+'"><span>'+f.n+'</span><span>'+kb(f.s)+'</span><a href="/api/log?f='+f.n+'" download>get</a></div>';});
id('loglist').innerHTML=h;}).catch(e=>{id('loglist').textContent='SD error';});}
function dlSel(){var s=document.querySelectorAll('.lsel:checked');if(!s.length)return;var i=0;
(function nxt(){if(i>=s.length)return;var a=document.createElement('a');a.href='/api/log?f='+s[i].value;a.download='';document.body.appendChild(a);a.click();a.remove();i++;setTimeout(nxt,700);})();}
function clearLogs(){
if(confirm('Download a combined copy of all logs first? (recommended - deletion cannot be undone)')){location.href='/api/logall';
setTimeout(function(){if(confirm('Delete ALL dive logs from the card now?'))doClear();},2000);}
else if(confirm('Delete ALL dive logs from the card now? This cannot be undone.'))doClear();}
function doClear(){fetch('/api/logclear',{method:'POST'}).then(r=>r.text()).then(t=>{loadLogs();}).catch(e=>{id('loglist').textContent='clear failed';});}
function otaUpload(){var f=id('fwfile').files[0];
var w=id('otawrap'),fill=id('otafill'),m=id('otamsg');
function say(c,t){w.style.display='block';m.className=c;m.textContent=t;}
if(!f){say('warn','Choose a .bin firmware file first.');return;}
if(!/\.bin$/i.test(f.name)){say('warn','That does not look like a .bin file.');return;}
if(!confirm('Flash "'+f.name+'" ('+kb(f.size)+')?\n\nThe logger will verify it, then reboot. Do NOT power it off during the update.'))return;
say('hint','Uploading 0%');fill.style.width='0%';
var x=new XMLHttpRequest();x.open('POST','/api/ota?size='+f.size);
x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);fill.style.width=p+'%';m.textContent='Uploading '+p+'%';}};
x.onload=function(){var r={};try{r=JSON.parse(x.responseText);}catch(e){}
if(x.status===200&&r.ok){fill.style.width='100%';say('ok','Update OK \u2014 the logger is rebooting. Rejoin the Wi-Fi in ~10 s, then re-open this page to confirm the version.');}
else{say('warn','Update failed: '+(r.err||('HTTP '+x.status))+'. The logger kept its old firmware.');}};
x.onerror=function(){say('warn','Connection dropped. If the logger rebooted, the update probably succeeded \u2014 rejoin the Wi-Fi and check the version.');};
var fd=new FormData();fd.append('f',f);x.send(fd);}
window.onload=function(){loadState();};
</script></body></html>
)HTML";

// Minimal upload-only page served by the recovery AP (boot-hold-past-CAL gesture). Standalone:
// it shares no JS with PAGE above, because recovery skips the normal portal entirely.
static const char RECOVERY_PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<style>
body{font-family:system-ui;margin:0;padding:18px;background:#0c1020;color:#e8eaf0}
h2{color:#ff5b50;margin:.2em 0}.c{background:#161b30;border-radius:12px;padding:16px;margin:12px 0}
.hint{font-size:13px;color:#9aa3c0;line-height:1.5}
code{background:#0c1020;border:1px solid #2a3252;border-radius:5px;padding:1px 6px;font-family:ui-monospace,monospace;color:#19c3c3}
input,button{width:100%;box-sizing:border-box;padding:12px;border-radius:8px;font-size:16px;margin-top:8px}
input{border:1px solid #2a3252;background:#0c1020;color:#fff}button{border:0;background:#185fa5;color:#fff}
#bar{display:none;margin-top:10px}#wrap{background:#0c1020;border:1px solid #2a3252;border-radius:6px;height:14px;overflow:hidden}
#fill{height:100%;width:0%;background:#19c3c3;transition:width .2s}#msg{font-size:13px;margin-top:6px}
.ok{color:#5dcaa5}.warn{color:#efb02a}
</style></head><body>
<h2>Recovery &mdash; firmware upload</h2>
<div class=c>
<p class=hint>This is the logger's <b>recovery mode</b>. It does one thing: re-flash firmware. Pick a firmware <code>.bin</code> and upload it &mdash; the logger verifies it, flashes itself and reboots into normal mode. <b>Don't power off during the update.</b></p>
<input type=file id=f accept=".bin,application/octet-stream">
<button onclick=up()>Upload &amp; flash firmware</button>
<div id=bar><div id=wrap><div id=fill></div></div><div id=msg></div></div>
<p class=hint>Power-cycle the logger to leave recovery without flashing.</p>
</div>
<script>
function kb(n){return n<1024?n+' B':(n/1024).toFixed(1)+' KB';}
function up(){var f=document.getElementById('f').files[0];
if(!f){alert('Choose a .bin file first.');return;}
if(!/\.bin$/i.test(f.name)){alert('That does not look like a .bin file.');return;}
if(!confirm('Flash "'+f.name+'" ('+kb(f.size)+')? Do not power off during the update.'))return;
var bar=document.getElementById('bar'),fill=document.getElementById('fill'),m=document.getElementById('msg');
bar.style.display='block';m.className='';m.textContent='Uploading 0%';fill.style.width='0%';
var x=new XMLHttpRequest();x.open('POST','/api/ota?size='+f.size);
x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);fill.style.width=p+'%';m.textContent='Uploading '+p+'%';}};
x.onload=function(){var r={};try{r=JSON.parse(x.responseText);}catch(e){}
if(x.status===200&&r.ok){fill.style.width='100%';m.className='ok';m.textContent='Update OK \u2014 rebooting into the new firmware.';}
else{m.className='warn';m.textContent='Failed: '+(r.err||('HTTP '+x.status));}};
x.onerror=function(){m.className='warn';m.textContent='Connection dropped \u2014 if it rebooted, the flash likely succeeded.';};
var fd=new FormData();fd.append('f',f);x.send(fd);}
</script></body></html>
)HTML";
