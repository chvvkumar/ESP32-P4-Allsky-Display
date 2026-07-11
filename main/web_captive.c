#include "web_server.h"
#include "web_internal.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "web_captive";

static httpd_handle_t s_cp = NULL;

/* Minimal restyled provisioning page (setup_ui.html lineage). Scans, lists
 * networks, posts credentials, then redirects to :8080 after a save. */
static const char SETUP_PAGE[] =
"<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>WiFi Setup</title><style>body{font-family:system-ui;background:#0a0c14;color:#e2e8f0;margin:0;padding:20px}"
".c{max-width:440px;margin:0 auto;background:#14171f;border:1px solid #ffffff14;border-radius:10px;padding:22px}"
"h1{font-size:19px;margin:0 0 4px}p.sub{color:#94a3b8;font-size:13px;margin:0 0 16px}"
"input{width:100%;box-sizing:border-box;background:#0c0e14;color:#e2e8f0;border:1px solid #ffffff26;border-radius:6px;padding:10px;font-size:14px;margin:6px 0}"
"button{width:100%;background:#5a86e8;color:#fff;border:0;border-radius:6px;padding:11px;font-size:14px;font-weight:600;cursor:pointer;margin-top:8px}"
".net{padding:9px 10px;border:1px solid #ffffff14;border-radius:6px;margin:5px 0;cursor:pointer;display:flex;justify-content:space-between}"
".net:hover{border-color:#5a86e8}.net small{color:#94a3b8}#list{max-height:260px;overflow:auto;margin:8px 0}#s{font-size:13px;color:#94a3b8;min-height:18px;margin-top:8px}</style></head>"
"<body><div class=c><h1>WiFi Setup</h1><p class=sub>Select a network and enter its password.</p>"
"<button onclick='scan()' id=sb>Scan Networks</button><div id=list></div>"
"<input id=ssid placeholder='SSID'><input id=pw type=password placeholder='Password'>"
"<button onclick='conn()'>Save &amp; Connect</button><div id=s></div></div>"
"<script>"
"function q(t){var d=Math.floor(t);if(t>-60)return['Excellent','#4ade80'];if(t>-70)return['Good','#7aa2f7'];if(t>-80)return['Fair','#fbbf24'];return['Weak','#f87171'];}"
"var poll=null;function scan(){document.getElementById('sb').textContent='Scanning...';fetch('/scan');poll=setInterval(res,1500);res();}"
"function res(){fetch('/scan/results').then(r=>r.json()).then(d=>{if(d.status!=='complete')return;clearInterval(poll);document.getElementById('sb').textContent='Scan Networks';var h='';"
"(d.networks||[]).forEach(function(n){var qq=q(n.rssi);h+=\"<div class=net onclick=\\\"pick('\"+n.ssid.replace(/'/g,\"\\\\'\")+\"')\\\">\"+\"<span>\"+(n.encrypted?'&#128274; ':'')+esc(n.ssid)+\"</span><small style=color:\"+qq[1]+\">\"+qq[0]+\" \"+n.rssi+\"dBm</small></div>\";});"
"document.getElementById('list').innerHTML=h||'<p style=color:#94a3b8>No networks found</p>';});}"
"function esc(s){return s.replace(/[&<>]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;'}[c];});}"
"function pick(s){document.getElementById('ssid').value=s;document.getElementById('pw').focus();}"
"function conn(){var ssid=document.getElementById('ssid').value;if(!ssid){document.getElementById('s').textContent='SSID is required';return;}"
"var fd='ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(document.getElementById('pw').value);"
"fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd}).then(r=>r.json()).then(d=>{document.getElementById('s').textContent=d.message||'Saved';"
"setTimeout(function(){location.href='http://'+location.hostname+':8080';},6000);}).catch(function(){document.getElementById('s').textContent='Save failed';});}"
"setTimeout(scan,400);</script></body></html>";

static esp_err_t cp_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, SETUP_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t cp_scan(httpd_req_t *req)
{
    web_hook_captive_scan_start();
    return web_send_json(req, 200, "{\"status\":\"scanning\"}");
}

static esp_err_t cp_scan_results(httpd_req_t *req)
{
    char *buf = malloc(4096);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    web_hook_captive_scan_results(buf, 4096);
    esp_err_t r = web_send_json(req, 200, buf);
    free(buf);
    return r;
}

static esp_err_t cp_connect(httpd_req_t *req)
{
    char body[400];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    char ssid[64], pw[96] = "";
    if (!web_form_get(body, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0')
        return web_send_400(req, "SSID is required");
    web_form_get(body, "password", pw, sizeof(pw));

    esp_err_t r = web_send_ok(req, "WiFi credentials saved. Device will reboot in 3 seconds to apply changes.");
    web_hook_captive_connect(ssid, pw);   /* network manager: save + test + reboot */
    return r;
}

static esp_err_t cp_404(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    return cp_root(req);   /* serve the setup page for any unknown path */
}

esp_err_t web_captive_start(void)
{
    if (s_cp) return ESP_OK;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.ctrl_port   = 32780;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;
    if (httpd_start(&s_cp, &cfg) != ESP_OK) { s_cp = NULL; return ESP_FAIL; }

    httpd_uri_t r_root   = { "/",             HTTP_GET,  cp_root,         NULL };
    httpd_uri_t r_scan   = { "/scan",         HTTP_GET,  cp_scan,         NULL };
    httpd_uri_t r_res    = { "/scan/results", HTTP_GET,  cp_scan_results, NULL };
    httpd_uri_t r_conn   = { "/connect",      HTTP_POST, cp_connect,      NULL };
    httpd_register_uri_handler(s_cp, &r_root);
    httpd_register_uri_handler(s_cp, &r_scan);
    httpd_register_uri_handler(s_cp, &r_res);
    httpd_register_uri_handler(s_cp, &r_conn);
    httpd_register_err_handler(s_cp, HTTPD_404_NOT_FOUND, cp_404);

    ESP_LOGI(TAG, "Captive portal up on :80");
    return ESP_OK;
}

void web_captive_stop(void)
{
    if (!s_cp) return;
    httpd_stop(s_cp);
    s_cp = NULL;
}
