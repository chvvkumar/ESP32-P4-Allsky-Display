# Image Sources Sun/Moon/GOES Rows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the image-source preset selectors into three dedicated rows (Sun, Moon Phase, GOES) and make the moon source present its phase settings both in the add toolbar and in the source list.

**Architecture:** Frontend-only change inside the client-rendered images app (`HTML_IMAGES_APP`) and a small block of CSS (`HTML_CSS`) in `web_config_html.h`. The `/api/images/state` endpoint already supplies `presets`, the global `moon` config, and a per-source `isMoon` flag; the `/api/addPreset` and `/api/setMoon` endpoints are reused unchanged. Moon controls are factored into shared `moonInline`/`moonDrawer`/`bindMoonControls` helpers so the add row and the in-list moon row render and behave identically against the single global `state.moon`.

**Tech Stack:** ESP32 Arduino sketch, JavaScript embedded in C++ PROGMEM raw string literals. No JS test runner exists; verification is by `arduino-cli` compile plus manual browser check (Playwright MCP).

---

## File Structure

- Modify: `web_config_html.h`
  - `HTML_CSS` block (around line 118-159): add a few layout classes.
  - `HTML_IMAGES_APP` block (around line 228-431): add moon helper functions, restructure `renderSourcesCard`, branch `renderRow` for the moon source, update `bind` and `bindRows`.

No other files change. No backend, NVS, or preset-catalog changes.

## Reference: relevant existing code

- `renderSourcesCard` currently: `web_config_html.h:261-294`.
- `renderRow` currently: `web_config_html.h:298-330`.
- `bgOpts`: `web_config_html.h:296`.
- `bind` (preset/moon wiring at top): `web_config_html.h:392-408`.
- `bindRows`: `web_config_html.h:414-423`.
- State shape (`presets`, `moon`, `sources[].isMoon`): `web_config_api.cpp:465-548`.

---

### Task 1: Add CSS classes for the new rows

**Files:**
- Modify: `web_config_html.h` (HTML_CSS block, after the `.img-count` rule at line 158)

- [ ] **Step 1: Add the classes**

Insert these rules immediately after the `.img-count{...}` line (line 158), before the closing `)rawliteral";` of HTML_CSS:

```css
.img-row-label{min-width:5.5rem;font-weight:600;color:var(--text);display:inline-flex;align-items:center;min-height:var(--tap)}
.img-moon-inline{display:inline-flex;align-items:center;gap:0.35rem;color:var(--muted);font-size:0.85rem}
.img-moon-inline select{width:auto;min-height:var(--tap)}
.img-moon-cog{min-height:var(--tap);min-width:var(--tap);background:var(--sunken);border:1px solid var(--border);border-radius:var(--radius);color:var(--text);cursor:pointer}
.img-moon-label{flex:1 1 160px;font-weight:600;color:var(--text);display:inline-flex;align-items:center;min-height:var(--tap)}
```

- [ ] **Step 2: Commit**

```bash
git add web_config_html.h
git commit -m "feat(web): add CSS classes for split image-source rows"
```

---

### Task 2: Add shared moon-control helper functions

**Files:**
- Modify: `web_config_html.h` (HTML_IMAGES_APP block, immediately after `bgOpts` at line 296)

These helpers render and bind the moon controls used by both the add toolbar and the in-list moon row. Ids are namespaced by a caller-supplied `prefix` so the same markup can appear twice without collisions.

- [ ] **Step 1: Insert the helper functions after `bgOpts`**

Add directly below the existing `bgOpts` function (line 296):

```javascript
function northUpOpts(sel){return '<option value="1"'+(+sel===1?' selected':'')+'>On (lock upright)</option><option value="0"'+(+sel===0?' selected':'')+'>Off (true sky tilt)</option>'}
function mfNum(prefix,key,label,val,step){return '<div class="transform-field"><label>'+label+'</label><input type="number" step="'+step+'" id="'+prefix+'Moon'+key+'" class="form-control" value="'+(+val)+'"></div>'}
function mfSel(prefix,key,label,opts,sel){var o='';for(var i=0;i<opts.length;i++){o+='<option value="'+i+'"'+(+sel===i?' selected':'')+'>'+opts[i]+'</option>'}return '<div class="transform-field"><label>'+label+'</label><select id="'+prefix+'Moon'+key+'" class="form-control">'+o+'</select></div>'}
function moonInline(prefix){var m=state.moon||{};var h='';h+='<label class="img-moon-inline"><span>Background</span><select id="'+prefix+'MoonBg" class="form-control">'+bgOpts(m.bg)+'</select></label>';h+='<label class="img-moon-inline"><span>North up</span><select id="'+prefix+'MoonNorthUp" class="form-control">'+northUpOpts(m.northup)+'</select></label>';h+='<button type="button" class="img-moon-cog" id="'+prefix+'MoonCog" aria-expanded="false" title="More moon settings"><i class="fas fa-cog"></i></button>';return h}
function moonDrawer(prefix){var m=state.moon||{};var h='<div class="img-drawer" id="'+prefix+'MoonDrawer"><div class="img-moon-grid">';h+=mfNum(prefix,'Lat','Latitude',m.lat,'0.0001');h+=mfNum(prefix,'Lon','Longitude',m.lon,'0.0001');h+=mfSel(prefix,'FlipU','Flip horizontal (U)',['Off','On'],m.flipu);h+=mfSel(prefix,'FlipV','Flip vertical (V)',['Off','On'],m.flipv);h+=mfNum(prefix,'Roll','Roll offset (deg)',m.roll,'1');h+=mfNum(prefix,'Yaw','Yaw offset (deg)',m.yaw,'1');h+=mfNum(prefix,'Pitch','Pitch offset (deg)',m.pitch,'1');h+='<div class="transform-field"><label>Drag light mode</label><select id="'+prefix+'MoonLight" class="form-control"><option value="0"'+(+m.light===0?' selected':'')+'>True phase</option><option value="1"'+(+m.light===1?' selected':'')+'>Explore</option></select></div>';h+='<div class="transform-field"><label>Spin mode</label><select id="'+prefix+'MoonSpin" class="form-control"><option value="0"'+(+m.spin===0?' selected':'')+'>Snap back</option><option value="1"'+(+m.spin===1?' selected':'')+'>Free spin</option></select></div>';h+=mfNum(prefix,'SpinRet','Free-spin return (s)',m.spinret,'1');h+='</div><button type="button" class="btn btn-success" id="'+prefix+'MoonSave">Save moon</button></div>';return h}
function bindMoonControls(prefix){var cog=document.getElementById(prefix+'MoonCog');if(cog)cog.onclick=function(){var d=document.getElementById(prefix+'MoonDrawer');if(d){var open=d.classList.toggle('is-open');cog.setAttribute('aria-expanded',open?'true':'false')}};function g(k,def){var e=document.getElementById(prefix+'Moon'+k);return e?(e.value||def):def}function collect(){return {lat:g('Lat','0'),lon:g('Lon','0'),bg:g('Bg','1'),flipu:g('FlipU','0'),flipv:g('FlipV','0'),roll:g('Roll','0'),yaw:g('Yaw','0'),pitch:g('Pitch','0'),northup:g('NorthUp','1'),light:g('Light','0'),spin:g('Spin','0'),spinret:g('SpinRet','10')}}function saveMoon(){post('/api/setMoon',collect()).then(function(j){if(j.status==='success'){toast(j.message||'Moon saved','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})}var save=document.getElementById(prefix+'MoonSave');if(save)save.onclick=saveMoon;['Bg','NorthUp'].forEach(function(k){var e=document.getElementById(prefix+'Moon'+k);if(e)e.onchange=saveMoon})}
```

- [ ] **Step 2: Commit**

```bash
git add web_config_html.h
git commit -m "feat(web): add shared moon-control render/bind helpers"
```

---

### Task 3: Restructure `renderSourcesCard` into Sun / Moon / GOES rows

**Files:**
- Modify: `web_config_html.h` (replace the body of `renderSourcesCard`, lines 261-294)

- [ ] **Step 1: Replace the function**

Replace the entire existing `renderSourcesCard` function (from `function renderSourcesCard(){` through its closing `}` at line 294) with:

```javascript
function renderSourcesCard(){
var sources=state.sources||[];
var multi=sources.length>1;
var presets=state.presets||[];
var sunOpts='',goesOpts='';
for(var i=0;i<presets.length;i++){var pid=presets[i].id;var op='<option value="'+esc(pid)+'">'+esc(presets[i].label)+'</option>';if(pid.indexOf('sdo_')===0||pid.indexOf('soho_')===0){sunOpts+=op}else if(pid.indexOf('goes')===0){goesOpts+=op}}
var moonExists=false;for(var k=0;k<sources.length;k++){if(sources[k].isMoon){moonExists=true;break}}
var h='<div class="card"><h2>Image Sources<span class="img-count">'+sources.length+' source'+(sources.length===1?'':'s')+'</span></h2>';
h+='<div class="img-toolbar">';
h+='<div class="img-add-bar"><input type="text" id="addUrl" class="form-control" placeholder="https://example.com/image.jpg"><button type="button" class="btn btn-success" id="addUrlBtn">Add</button></div>';
h+='<div class="img-add-bar"><label class="img-row-label">Sun</label><select id="sunSel" class="form-control" style="flex:1 1 220px">'+sunOpts+'</select><button type="button" class="btn btn-secondary" id="addSunBtn">Add</button></div>';
h+='<div class="img-add-bar"><label class="img-row-label">Moon Phase</label>'+moonInline('add')+'<button type="button" class="btn btn-secondary" id="addMoonBtn"'+(moonExists?' disabled title="Moon already in list"':'')+'>Add</button>'+moonDrawer('add')+'</div>';
h+='<div class="img-add-bar"><label class="img-row-label">GOES</label><select id="goesSel" class="form-control" style="flex:1 1 220px">'+goesOpts+'</select><button type="button" class="btn btn-secondary" id="addGoesBtn">Add</button></div>';
h+='</div>';
if(multi){
h+='<div class="img-bulk-bar"><label><input type="checkbox" id="selAll"> Select all</label>';
h+='<button type="button" class="btn btn-danger" id="bulkDelBtn" disabled>Delete selected (<span id="selCount">0</span>)</button></div>';
}
h+='<div class="img-list">';
for(var x=0;x<sources.length;x++){h+=renderRow(sources[x],multi)}
h+='</div></div>';
return h;
}
```

- [ ] **Step 2: Commit**

```bash
git add web_config_html.h
git commit -m "feat(web): split image-source presets into Sun/Moon/GOES rows"
```

---

### Task 4: Branch `renderRow` for the moon source

**Files:**
- Modify: `web_config_html.h` (replace the body of `renderRow`, lines 298-330)

The moon row shows a `Moon Phase` label instead of the url input, and the shared moon controls instead of the generic transform drawer. Non-moon rows keep their existing markup exactly.

- [ ] **Step 1: Replace the function**

Replace the entire existing `renderRow` function (from `function renderRow(s,multi){` through its closing `}` at line 330) with:

```javascript
function renderRow(s,multi){
var idx=s.index;
var active=(state.currentIndex===idx);
var cls='img-row'+(active?' is-active':'')+(s.enabled?'':' is-disabled');
var h='<div class="'+cls+'" data-index="'+idx+'">';
h+='<div class="img-row-main">';
h+='<button type="button" class="img-toggle-btn" data-act="toggle" data-index="'+idx+'" aria-pressed="'+(s.enabled?'true':'false')+'"><i class="fas fa-'+(s.enabled?'eye':'eye-slash')+'"></i> '+(s.enabled?'On':'Off')+'</button>';
h+='<span class="img-idx">'+(idx+1)+'.</span>';
if(s.isMoon){
var mp='moonrow'+idx;
h+='<span class="img-moon-label">Moon Phase</span>';
h+=moonInline(mp);
if(multi){h+='<label style="display:inline-flex;align-items:center;min-height:var(--tap)"><input type="checkbox" class="img-sel" data-index="'+idx+'"></label>'}
h+='</div>';
h+='<div class="img-row-meta">';
h+='<span>Duration <input type="number" class="form-control" data-act="duration" data-index="'+idx+'" min="5" max="3600" value="'+(s.duration)+'"> s</span>';
h+='</div>';
h+=moonDrawer(mp);
h+='</div>';
return h;
}
var open=openDrawers[idx];
h+='<input type="text" class="form-control" data-act="url" data-index="'+idx+'" value="'+esc(s.url)+'">';
h+='<button type="button" class="img-caret" data-act="caret" data-index="'+idx+'" aria-expanded="'+(open?'true':'false')+'"><i class="fas fa-chevron-'+(open?'up':'down')+'"></i></button>';
if(multi){h+='<label style="display:inline-flex;align-items:center;min-height:var(--tap)"><input type="checkbox" class="img-sel" data-index="'+idx+'"></label>'}
h+='</div>';
h+='<div class="img-row-meta">';
h+='<span>Duration <input type="number" class="form-control" data-act="duration" data-index="'+idx+'" min="5" max="3600" value="'+(s.duration)+'"> s</span>';
h+='<span class="img-summary">'+esc(summary(s))+'</span>';
h+='</div>';
h+='<div class="img-drawer'+(open?' is-open':'')+'" data-drawer="'+idx+'">';
h+='<div class="transform-grid">';
h+=tf(idx,'scaleX','Scale X',s.scaleX,0.01,0.1,state.maxScale);
h+=tf(idx,'scaleY','Scale Y',s.scaleY,0.01,0.1,state.maxScale);
h+=tfInt(idx,'offsetX','Offset X',s.offsetX);
h+=tfInt(idx,'offsetY','Offset Y',s.offsetY);
h+='<div class="transform-field"><label>Rotation</label><select class="form-control" data-act="tf" data-prop="rotation" data-index="'+idx+'">'+rotOpts(s.rotation)+'</select></div>';
h+='</div>';
h+='<div class="img-drawer-actions">';
h+='<button type="button" class="btn btn-secondary" data-act="reset" data-index="'+idx+'">Reset to defaults</button>';
var tuned=state.tuning&&state.tuning.active&&state.tuning.index===idx;
if(tuned){h+='<span class="status-pill status-pill--active">Holding #'+(idx+1)+' on display</span><button type="button" class="btn btn-success" data-act="tunestop">Done, resume cycling</button>'}
else{h+='<button type="button" class="btn btn-secondary" data-act="tune" data-index="'+idx+'">Tune on device</button>'}
h+='</div></div></div>';
return h;
}
```

- [ ] **Step 2: Commit**

```bash
git add web_config_html.h
git commit -m "feat(web): show Moon Phase label and moon controls on in-list moon row"
```

---

### Task 5: Rewire `bind` for the new add rows and moon controls

**Files:**
- Modify: `web_config_html.h` (the preset/moon wiring portion of `bind`, lines 393-396)

The old `addPresetBtn`, `moonHead`, and `saveMoonBtn` handlers no longer exist in the markup. Replace them with handlers for the three Add buttons and the shared moon-control binding.

- [ ] **Step 1: Replace the old handlers**

In `bind`, replace these four lines (current lines 393-396):

```javascript
var addBtn=document.getElementById('addUrlBtn');if(addBtn)addBtn.onclick=function(){var inp=document.getElementById('addUrl');var u=(inp.value||'').trim();if(!u.match(/^https?:\/\/.+/i)){inputErr(inp);toast('URL must start with http:// or https://','error');return}post('/api/add-source',{url:u}).then(function(j){if(j.status==='success'){toast('Source added','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};
var apBtn=document.getElementById('addPresetBtn');if(apBtn)apBtn.onclick=function(){var sel=document.getElementById('presetSel');if(!sel||!sel.value){toast('No preset selected','warning');return}post('/api/addPreset',{id:sel.value}).then(function(j){if(j.status==='success'){toast(j.message||'Preset added','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};
var mh=document.getElementById('moonHead');if(mh)mh.onclick=function(){var b=document.getElementById('moonBody');if(b)b.classList.toggle('is-open')};
var sm=document.getElementById('saveMoonBtn');if(sm)sm.onclick=function(){var moonData={lat:document.getElementById('moonLat').value||'0',lon:document.getElementById('moonLon').value||'0',bg:document.getElementById('moonBg').value||'1',flipu:document.getElementById('moonFlipU').value||'0',flipv:document.getElementById('moonFlipV').value||'0',roll:document.getElementById('moonRoll').value||'0',yaw:document.getElementById('moonYaw').value||'0',pitch:document.getElementById('moonPitch').value||'0',northup:document.getElementById('moonNorthUp').value||'1',light:document.getElementById('moonLight').value||'0',spin:document.getElementById('moonSpin').value||'0',spinret:document.getElementById('moonSpinRet').value||'10'};post('/api/setMoon',moonData).then(function(j){if(j.status==='success'){toast(j.message||'Moon saved','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};
```

with:

```javascript
var addBtn=document.getElementById('addUrlBtn');if(addBtn)addBtn.onclick=function(){var inp=document.getElementById('addUrl');var u=(inp.value||'').trim();if(!u.match(/^https?:\/\/.+/i)){inputErr(inp);toast('URL must start with http:// or https://','error');return}post('/api/add-source',{url:u}).then(function(j){if(j.status==='success'){toast('Source added','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};
function addPresetById(id){post('/api/addPreset',{id:id}).then(function(j){if(j.status==='success'){toast(j.message||'Preset added','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})}
var addSun=document.getElementById('addSunBtn');if(addSun)addSun.onclick=function(){var sel=document.getElementById('sunSel');if(!sel||!sel.value){toast('No Sun image selected','warning');return}addPresetById(sel.value)};
var addGoes=document.getElementById('addGoesBtn');if(addGoes)addGoes.onclick=function(){var sel=document.getElementById('goesSel');if(!sel||!sel.value){toast('No GOES image selected','warning');return}addPresetById(sel.value)};
var addMoon=document.getElementById('addMoonBtn');if(addMoon)addMoon.onclick=function(){addPresetById('__moon__')};
bindMoonControls('add');
```

- [ ] **Step 2: Commit**

```bash
git add web_config_html.h
git commit -m "feat(web): wire Sun/Moon/GOES add buttons and add-row moon controls"
```

---

### Task 6: Bind moon controls for in-list moon rows

**Files:**
- Modify: `web_config_html.h` (`bindRows`, after line 422 inside the function, before its closing `}`)

- [ ] **Step 1: Add the binding loop**

At the end of `bindRows`, immediately before the function's closing `}` (after the `data-act="tunestop"` line at 422), add:

```javascript
(state.sources||[]).forEach(function(s){if(s.isMoon){bindMoonControls('moonrow'+s.index)}});
```

- [ ] **Step 2: Commit**

```bash
git add web_config_html.h
git commit -m "feat(web): bind moon controls on in-list moon rows"
```

---

### Task 7: Compile the sketch

**Files:** none (build verification)

- [ ] **Step 1: Compile**

Run the project's compile step. Use the existing helper:

Run: `pwsh -File compile-and-upload.ps1 -SkipUpload` (the `-SkipUpload` switch compiles via `arduino-cli` and copies the binary without flashing the device).

Expected: compile succeeds with no errors. The change is JS inside PROGMEM strings, so the only realistic failure is an unbalanced raw-string literal or a stray `)rawliteral"` — fix by checking the edited blocks still open with `R"rawliteral(` and close with `)rawliteral";`.

- [ ] **Step 2: Commit (only if compile produced incidental changes)**

No commit expected unless the build emits artifacts.

---

### Task 8: Browser verification

**Files:** none (manual verification via Playwright MCP against the live device)

Live device: `http://allskyesp3236.lan:8080/config/images` (note port 8080).

- [ ] **Step 1: Load the page and confirm the three rows**

Navigate to the images config page. Confirm the add toolbar shows, top to bottom: generic URL bar, a `Sun` row with a solar-only dropdown and Add, a `Moon Phase` row with Background + North up + cog + Add, and a `GOES` row with a GOES-only dropdown and Add.

Expected: Sun dropdown contains only `Sun ...` labels; GOES dropdown contains only the GOES entry; neither contains "Moon".

- [ ] **Step 2: Confirm the moon cog drawer**

Click the Moon Phase cog. Expected: a drawer expands showing Latitude, Longitude, Flip H/V, Roll, Yaw, Pitch, Drag light mode, Spin mode, Free-spin return, and a "Save moon" button.

- [ ] **Step 3: Confirm Add-moon disabling**

If a moon source already exists, the Moon Phase Add button is disabled with the title "Moon already in list". If not, click Add and confirm a moon source appears and the Add button becomes disabled after the reload.

- [ ] **Step 4: Confirm the in-list moon row**

In the source list, the moon row shows the label `Moon Phase` (not `moon://default`), with inline Background + North up + cog, and no generic transform drawer. Toggling Background or North up here shows a "Moon saved" toast, and after reload the same value is reflected in the top Moon Phase row.

- [ ] **Step 5: Confirm non-moon rows unchanged**

A normal (URL) source row still shows its editable URL input, caret, transform drawer, duration, and summary.

---

## Self-Review Notes

- Spec acceptance criteria 1-7 map to Tasks 3 (rows), 3 (dropdown partition + add buttons), 2+3 (moon inline + cog), 3+5 (add-moon disabling), 4+6 (in-list moon row + label), 2 (shared global state), and 4 (non-moon rows untouched). Criterion 8 (compiles) maps to Task 7.
- Id naming is consistent: `prefix+'Moon'+key` in `mfNum`/`mfSel`/`moonInline`/`moonDrawer` matches `g(k)` reading `prefix+'Moon'+k` in `bindMoonControls`. Prefixes used: `'add'` for the toolbar, `'moonrow'+index` for list rows.
- `bgOpts` is reused unchanged; `northUpOpts`, `mfNum`, `mfSel`, `moonInline`, `moonDrawer`, `bindMoonControls`, `addPresetById` are new.
- Legacy `injectPresetPicker`/`injectMoonSettings` in `HTML_JAVASCRIPT` target `imageSourcesList`, which does not exist on this page; they no-op and are intentionally left untouched.
