/**
 * @file web_pages.c
 * @brief Pages HTML embarquees pour le serveur web Medbox
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "web_pages";

// ============================================
// Page HTML principale (Single Page App)
// Design moderne inspire Apple/Microsoft
// ============================================
static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Medbox</title>
    <style>
        :root {
            --bg: #f5f5f7;
            --card: rgba(255,255,255,0.8);
            --text: #1d1d1f;
            --text2: #86868b;
            --accent: #0071e3;
            --accent2: #147ce5;
            --green: #34c759;
            --red: #ff3b30;
            --orange: #ff9500;
            --border: rgba(0,0,0,0.04);
            --shadow: 0 2px 12px rgba(0,0,0,0.08);
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Display', 'Segoe UI', Roboto, sans-serif;
            background: var(--bg);
            color: var(--text);
            line-height: 1.5;
            -webkit-font-smoothing: antialiased;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 32px 20px 24px;
            text-align: center;
        }
        .header h1 {
            font-size: 28px;
            font-weight: 600;
            letter-spacing: -0.5px;
        }
        .header p {
            font-size: 13px;
            opacity: 0.85;
            margin-top: 4px;
        }
        .tabs {
            display: flex;
            background: white;
            padding: 8px;
            gap: 6px;
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 100;
            backdrop-filter: blur(20px);
        }
        .tab {
            flex: 1;
            padding: 10px 16px;
            text-align: center;
            color: var(--text2);
            cursor: pointer;
            border: none;
            background: transparent;
            font-size: 14px;
            font-weight: 500;
            border-radius: 8px;
            transition: all 0.2s ease;
        }
        .tab.active {
            background: var(--accent);
            color: white;
        }
        .tab:hover:not(.active) {
            background: rgba(0,0,0,0.04);
        }
        .content {
            padding: 16px;
            max-width: 480px;
            margin: 0 auto;
        }
        .page { display: none; animation: fadeIn 0.3s ease; }
        .page.active { display: block; }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: translateY(0); } }
        .card {
            background: var(--card);
            border-radius: 16px;
            padding: 20px;
            margin-bottom: 12px;
            box-shadow: var(--shadow);
            backdrop-filter: blur(20px);
            border: 1px solid var(--border);
        }
        .card-title {
            font-size: 11px;
            font-weight: 600;
            color: var(--text2);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-bottom: 16px;
        }
        .row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px 0;
            border-bottom: 1px solid var(--border);
        }
        .row:last-child { border-bottom: none; }
        .label {
            color: var(--text);
            font-size: 15px;
        }
        .value {
            font-weight: 500;
            font-size: 15px;
            color: var(--text2);
        }
        .badge {
            display: inline-flex;
            align-items: center;
            padding: 4px 10px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 600;
        }
        .badge-ok { background: rgba(52,199,89,0.12); color: var(--green); }
        .badge-err { background: rgba(255,59,48,0.12); color: var(--red); }
        .badge-warn { background: rgba(255,149,0,0.12); color: var(--orange); }
        .badge::before {
            content: '';
            width: 6px;
            height: 6px;
            border-radius: 50%;
            margin-right: 6px;
            background: currentColor;
        }
        input, select {
            width: 100%;
            padding: 14px 16px;
            margin: 6px 0 12px;
            border: 1px solid rgba(0,0,0,0.1);
            border-radius: 12px;
            font-size: 16px;
            background: white;
            transition: border-color 0.2s, box-shadow 0.2s;
        }
        input:focus {
            outline: none;
            border-color: var(--accent);
            box-shadow: 0 0 0 3px rgba(0,113,227,0.1);
        }
        label {
            font-size: 13px;
            font-weight: 500;
            color: var(--text2);
        }
        button {
            width: 100%;
            padding: 14px;
            background: var(--accent);
            color: white;
            border: none;
            border-radius: 12px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        button:hover { background: var(--accent2); transform: translateY(-1px); }
        button:active { transform: translateY(0); }
        button:disabled { background: #ccc; transform: none; }
        button.secondary {
            background: rgba(0,0,0,0.05);
            color: var(--text);
        }
        button.secondary:hover { background: rgba(0,0,0,0.08); }
        .network-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 14px 0;
            border-bottom: 1px solid var(--border);
            cursor: pointer;
            transition: background 0.15s;
        }
        .network-item:last-child { border-bottom: none; }
        .network-item:hover { background: rgba(0,0,0,0.02); margin: 0 -20px; padding: 14px 20px; }
        .network-name { font-weight: 500; font-size: 15px; }
        .network-info { font-size: 12px; color: var(--text2); margin-top: 2px; }
        .rssi-bar {
            display: flex;
            gap: 2px;
            align-items: flex-end;
        }
        .rssi-bar span {
            width: 4px;
            background: var(--text2);
            border-radius: 1px;
        }
        .error-item {
            padding: 14px;
            background: rgba(255,59,48,0.04);
            border-radius: 12px;
            margin-bottom: 8px;
            border-left: 3px solid var(--red);
        }
        .error-code {
            font-family: 'SF Mono', Monaco, monospace;
            font-size: 12px;
            font-weight: 600;
            color: var(--red);
            background: rgba(255,59,48,0.1);
            padding: 2px 8px;
            border-radius: 4px;
        }
        .error-msg {
            font-size: 14px;
            margin-top: 6px;
            color: var(--text);
        }
        .error-ctx {
            font-size: 12px;
            color: var(--text2);
            margin-top: 4px;
        }
        .error-time {
            font-size: 11px;
            color: var(--text2);
            margin-top: 6px;
        }
        .loader {
            text-align: center;
            padding: 32px;
            color: var(--text2);
            font-size: 14px;
        }
        .msg {
            padding: 14px 16px;
            border-radius: 12px;
            margin-bottom: 16px;
            font-size: 14px;
            font-weight: 500;
        }
        .msg-ok { background: rgba(52,199,89,0.1); color: var(--green); }
        .msg-err { background: rgba(255,59,48,0.1); color: var(--red); }
        .empty-state {
            text-align: center;
            padding: 40px 20px;
            color: var(--text2);
        }
        .empty-state svg { width: 48px; height: 48px; opacity: 0.3; margin-bottom: 12px; }
        .uuid-display {
            font-family: 'SF Mono', Monaco, monospace;
            font-size: 11px;
            word-break: break-all;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Medbox</h1>
        <p id="header-uuid">Configuration</p>
    </div>
    <div class="tabs">
        <button class="tab active" onclick="showPage('status',this)">Status</button>
        <button class="tab" onclick="showPage('wifi',this)">WiFi</button>
        <button class="tab" onclick="showPage('errors',this)">Erreurs</button>
    </div>
    <div class="content">
        <!-- Page Status -->
        <div id="status" class="page active">
            <div class="card">
                <div class="card-title">Systeme</div>
                <div class="row"><span class="label">Firmware</span><span class="value" id="firmware">-</span></div>
                <div class="row"><span class="label">Uptime</span><span class="value" id="uptime">-</span></div>
                <div class="row"><span class="label">Memoire libre</span><span class="value" id="heap">-</span></div>
            </div>
            <div class="card">
                <div class="card-title">WiFi Station</div>
                <div class="row"><span class="label">Etat</span><span id="sta-status">-</span></div>
                <div class="row"><span class="label">Adresse IP</span><span class="value" id="sta-ip">-</span></div>
                <div class="row"><span class="label">Signal</span><span class="value" id="sta-rssi">-</span></div>
            </div>
            <div class="card">
                <div class="card-title">Point d'Acces</div>
                <div class="row"><span class="label">Etat</span><span id="ap-status">-</span></div>
                <div class="row"><span class="label">Nom</span><span class="value" id="ap-ssid">-</span></div>
                <div class="row"><span class="label">Adresse IP</span><span class="value" id="ap-ip">-</span></div>
            </div>
            <button class="secondary" onclick="loadStatus()">Actualiser</button>
        </div>

        <!-- Page WiFi -->
        <div id="wifi" class="page">
            <div class="card">
                <div class="card-title">Connexion WiFi</div>
                <div id="wifi-msg"></div>
                <label>Nom du reseau (SSID)</label>
                <input type="text" id="wifi-ssid" placeholder="Selectionnez ou entrez le nom">
                <label>Mot de passe</label>
                <input type="password" id="wifi-pass" placeholder="Entrez le mot de passe">
                <button onclick="saveWifi()">Se connecter</button>
            </div>
            <div class="card">
                <div class="card-title">Reseaux disponibles</div>
                <div id="networks"><div class="loader">Appuyez sur Scanner</div></div>
                <button class="secondary" onclick="scanWifi()">Scanner les reseaux</button>
            </div>
        </div>

        <!-- Page Erreurs -->
        <div id="errors" class="page">
            <div class="card">
                <div class="card-title">Historique des erreurs</div>
                <div id="error-list"><div class="loader">Chargement...</div></div>
            </div>
            <button class="secondary" onclick="loadErrors()">Actualiser</button>
        </div>
    </div>

    <script>
        function showPage(name, btn) {
            document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.getElementById(name).classList.add('active');
            btn.classList.add('active');
            if (name === 'status') loadStatus();
            if (name === 'errors') loadErrors();
        }

        function formatUptime(sec) {
            const d = Math.floor(sec / 86400);
            const h = Math.floor((sec % 86400) / 3600);
            const m = Math.floor((sec % 3600) / 60);
            if (d > 0) return d + 'j ' + h + 'h ' + m + 'm';
            if (h > 0) return h + 'h ' + m + 'm';
            return m + 'm ' + (sec % 60) + 's';
        }

        function toHex(n) {
            return '0x' + n.toString(16).toUpperCase().padStart(4, '0');
        }

        function formatTime(sec) {
            const h = Math.floor(sec / 3600);
            const m = Math.floor((sec % 3600) / 60);
            const s = sec % 60;
            return 'il y a ' + (h > 0 ? h + 'h ' : '') + (m > 0 ? m + 'm ' : '') + s + 's';
        }

        function rssiToBar(rssi) {
            const bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : 1;
            let html = '<div class="rssi-bar">';
            for (let i = 1; i <= 4; i++) {
                const h = 4 + i * 3;
                const op = i <= bars ? '1' : '0.2';
                html += '<span style="height:' + h + 'px;opacity:' + op + '"></span>';
            }
            return html + '</div>';
        }

        function loadStatus() {
            fetch('/api/status')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('header-uuid').textContent = d.uuid ? d.uuid.substring(0, 8) + '...' : 'Configuration';
                    document.getElementById('firmware').textContent = 'v' + d.firmware_version;
                    document.getElementById('uptime').textContent = formatUptime(d.uptime_sec);
                    document.getElementById('heap').textContent = Math.round(d.free_heap / 1024) + ' KB';

                    const sta = d.wifi_sta;
                    document.getElementById('sta-status').innerHTML = sta.connected ?
                        '<span class="badge badge-ok">Connecte</span>' : '<span class="badge badge-err">Deconnecte</span>';
                    document.getElementById('sta-ip').textContent = sta.ip || '-';
                    document.getElementById('sta-rssi').textContent = sta.rssi ? sta.rssi + ' dBm' : '-';

                    const ap = d.wifi_ap;
                    document.getElementById('ap-status').innerHTML = ap.active ?
                        '<span class="badge badge-ok">Actif</span>' : '<span class="badge badge-warn">Inactif</span>';
                    document.getElementById('ap-ssid').textContent = ap.ssid || '-';
                    document.getElementById('ap-ip').textContent = ap.ip || '-';
                })
                .catch(e => console.error('Erreur:', e));
        }

        function scanWifi() {
            document.getElementById('networks').innerHTML = '<div class="loader">Recherche en cours...</div>';
            fetch('/api/wifi/scan')
                .then(r => r.json())
                .then(d => {
                    if (d.networks.length === 0) {
                        document.getElementById('networks').innerHTML = '<div class="empty-state">Aucun reseau trouve</div>';
                        return;
                    }
                    let html = '';
                    d.networks.forEach(n => {
                        html += '<div class="network-item" onclick="selectNetwork(\'' + n.ssid.replace(/'/g, "\\'") + '\')">';
                        html += '<div><div class="network-name">' + n.ssid + '</div>';
                        html += '<div class="network-info">' + n.security + '</div></div>';
                        html += rssiToBar(n.rssi) + '</div>';
                    });
                    document.getElementById('networks').innerHTML = html;
                })
                .catch(e => {
                    document.getElementById('networks').innerHTML = '<div class="empty-state">Erreur de scan</div>';
                });
        }

        function selectNetwork(ssid) {
            document.getElementById('wifi-ssid').value = ssid;
            document.getElementById('wifi-pass').focus();
        }

        function saveWifi() {
            const ssid = document.getElementById('wifi-ssid').value;
            const pass = document.getElementById('wifi-pass').value;
            if (!ssid) {
                showMsg('wifi-msg', 'Veuillez entrer un nom de reseau', false);
                return;
            }
            fetch('/api/wifi/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ssid: ssid, password: pass})
            })
            .then(r => r.json())
            .then(d => {
                showMsg('wifi-msg', d.message, d.success);
                if (d.success) setTimeout(loadStatus, 3000);
            })
            .catch(e => showMsg('wifi-msg', 'Erreur de connexion', false));
        }

        function showMsg(id, msg, ok) {
            const el = document.getElementById(id);
            el.innerHTML = '<div class="msg ' + (ok ? 'msg-ok' : 'msg-err') + '">' + msg + '</div>';
            setTimeout(() => el.innerHTML = '', 5000);
        }

        function loadErrors() {
            fetch('/api/errors')
                .then(r => r.json())
                .then(d => {
                    if (d.errors.length === 0) {
                        document.getElementById('error-list').innerHTML = '<div class="empty-state">Aucune erreur enregistree</div>';
                        return;
                    }
                    let html = '';
                    d.errors.forEach(e => {
                        html += '<div class="error-item">';
                        html += '<span class="error-code">' + toHex(e.code) + '</span>';
                        html += '<div class="error-msg">' + e.message + '</div>';
                        if (e.context) html += '<div class="error-ctx">' + e.context + '</div>';
                        if (e.timestamp) html += '<div class="error-time">' + formatTime(e.timestamp) + '</div>';
                        html += '</div>';
                    });
                    document.getElementById('error-list').innerHTML = html;
                })
                .catch(e => {
                    document.getElementById('error-list').innerHTML = '<div class="empty-state">Erreur de chargement</div>';
                });
        }

        loadStatus();
    </script>
</body>
</html>
)rawliteral";

// ============================================
// Handler page principale
// ============================================
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

// ============================================
// Enregistrement des handlers de pages
// ============================================
esp_err_t web_pages_register_handlers(httpd_handle_t server)
{
    httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
    };

    httpd_uri_t uri_index_wifi = {
        .uri = "/wifi",
        .method = HTTP_GET,
        .handler = index_handler,
    };

    httpd_uri_t uri_index_errors = {
        .uri = "/errors",
        .method = HTTP_GET,
        .handler = index_handler,
    };

    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_index_wifi);
    httpd_register_uri_handler(server, &uri_index_errors);

    ESP_LOGI(TAG, "Pages web enregistrees");
    return ESP_OK;
}
