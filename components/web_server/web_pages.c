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
            --bg: #F2F4F6;
            --card: #FFFFFF;
            --text: #0F1419;
            --text2: #5B6570;
            --text3: #8E959D;
            --accent: #0A9B8E;
            --accent-hover: #088A7E;
            --accent-light: rgba(10,155,142,0.08);
            --accent-ring: rgba(10,155,142,0.18);
            --green: #00B67A;
            --red: #E5484D;
            --orange: #F5A623;
            --border: rgba(15,20,25,0.06);
            --shadow-sm: 0 1px 2px rgba(15,20,25,0.04);
            --shadow: 0 2px 8px rgba(15,20,25,0.06), 0 0 1px rgba(15,20,25,0.08);
            --shadow-lg: 0 8px 24px rgba(15,20,25,0.08), 0 0 1px rgba(15,20,25,0.1);
            --radius: 16px;
            --header-h: 140px;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Segoe UI', system-ui, sans-serif;
            background: var(--bg);
            color: var(--text);
            line-height: 1.5;
            -webkit-font-smoothing: antialiased;
            -moz-osx-font-smoothing: grayscale;
            min-height: 100vh;
        }

        /* ===== HEADER ===== */
        .header {
            background: var(--text);
            color: #fff;
            padding: 28px 24px 24px;
            position: relative;
            overflow: hidden;
        }
        .header::after {
            content: '';
            position: absolute;
            bottom: 0; left: 0; right: 0;
            height: 1px;
            background: linear-gradient(90deg, transparent, rgba(10,155,142,0.4), transparent);
        }
        .header-inner {
            max-width: 520px;
            margin: 0 auto;
            display: flex;
            align-items: center;
            gap: 16px;
        }
        .header-logo {
            width: 44px; height: 44px;
            background: var(--accent);
            border-radius: 12px;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-shrink: 0;
        }
        .header-logo svg { width: 24px; height: 24px; }
        .header-text h1 {
            font-size: 20px;
            font-weight: 700;
            letter-spacing: -0.3px;
            line-height: 1.2;
        }
        .header-uuid {
            font-family: 'SF Mono', 'Cascadia Code', 'Consolas', monospace;
            font-size: 11px;
            color: rgba(255,255,255,0.45);
            margin-top: 3px;
            letter-spacing: 0.3px;
        }
        .header-status {
            margin-left: auto;
            display: flex;
            align-items: center;
            gap: 6px;
            font-size: 12px;
            font-weight: 500;
            color: rgba(255,255,255,0.6);
        }
        .status-dot {
            width: 8px; height: 8px;
            border-radius: 50%;
            background: var(--green);
            position: relative;
        }
        .status-dot.connected::after {
            content: '';
            position: absolute;
            inset: -3px;
            border-radius: 50%;
            border: 1.5px solid var(--green);
            animation: pulse 2s ease-in-out infinite;
        }
        .status-dot.disconnected { background: var(--red); }
        .status-dot.disconnected::after { display: none; }
        @keyframes pulse {
            0%, 100% { opacity: 0; transform: scale(0.8); }
            50% { opacity: 0.6; transform: scale(1); }
        }

        /* ===== NAVIGATION ===== */
        .nav {
            position: sticky;
            top: 0;
            z-index: 100;
            background: rgba(242,244,246,0.85);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border-bottom: 1px solid var(--border);
            padding: 0 24px;
        }
        .nav-inner {
            max-width: 520px;
            margin: 0 auto;
            display: flex;
            gap: 2px;
            padding: 8px 0;
        }
        .tab {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 7px;
            padding: 10px 12px;
            color: var(--text3);
            cursor: pointer;
            border: none;
            background: transparent;
            font-family: inherit;
            font-size: 13px;
            font-weight: 600;
            border-radius: 10px;
            transition: all 0.2s cubic-bezier(0.4,0,0.2,1);
            position: relative;
        }
        .tab svg { width: 16px; height: 16px; opacity: 0.5; transition: opacity 0.2s; }
        .tab.active {
            background: var(--card);
            color: var(--accent);
            box-shadow: var(--shadow-sm);
        }
        .tab.active svg { opacity: 1; }
        .tab:hover:not(.active) { background: rgba(15,20,25,0.04); }
        .tab:active { transform: scale(0.97); }

        /* ===== CONTENT ===== */
        .content {
            padding: 20px 24px 40px;
            max-width: 520px;
            margin: 0 auto;
        }
        .page { display: none; }
        .page.active { display: block; animation: pageIn 0.35s cubic-bezier(0.4,0,0.2,1); }
        @keyframes pageIn {
            from { opacity: 0; transform: translateY(12px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* ===== CARDS ===== */
        .card {
            background: var(--card);
            border-radius: var(--radius);
            padding: 0;
            margin-bottom: 16px;
            box-shadow: var(--shadow);
            overflow: hidden;
            transition: box-shadow 0.25s, transform 0.25s;
        }
        .card:hover { box-shadow: var(--shadow-lg); }
        .card-head {
            display: flex;
            align-items: center;
            gap: 10px;
            padding: 18px 20px 0;
        }
        .card-icon {
            width: 32px; height: 32px;
            border-radius: 8px;
            background: var(--accent-light);
            display: flex;
            align-items: center;
            justify-content: center;
            flex-shrink: 0;
        }
        .card-icon svg { width: 16px; height: 16px; color: var(--accent); }
        .card-title {
            font-size: 13px;
            font-weight: 700;
            color: var(--text);
            letter-spacing: -0.1px;
        }
        .card-body { padding: 12px 20px 20px; }
        .row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 11px 0;
            border-bottom: 1px solid var(--border);
        }
        .row:last-child { border-bottom: none; }
        .label {
            font-size: 14px;
            color: var(--text2);
            font-weight: 400;
        }
        .value {
            font-size: 14px;
            font-weight: 600;
            color: var(--text);
            font-variant-numeric: tabular-nums;
        }

        /* ===== BADGES ===== */
        .badge {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            padding: 4px 10px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 600;
        }
        .badge-ok { background: rgba(0,182,122,0.1); color: var(--green); }
        .badge-err { background: rgba(229,72,77,0.1); color: var(--red); }
        .badge-warn { background: rgba(245,166,35,0.1); color: var(--orange); }
        .badge-dot {
            width: 6px; height: 6px;
            border-radius: 50%;
            background: currentColor;
        }

        /* ===== FORMS ===== */
        .form-group { margin-bottom: 16px; }
        .form-label {
            display: block;
            font-size: 12px;
            font-weight: 600;
            color: var(--text2);
            text-transform: uppercase;
            letter-spacing: 0.4px;
            margin-bottom: 6px;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px 14px;
            border: 1.5px solid var(--border);
            border-radius: 10px;
            font-size: 15px;
            font-family: inherit;
            background: var(--bg);
            color: var(--text);
            transition: border-color 0.2s, box-shadow 0.2s, background 0.2s;
        }
        input:focus {
            outline: none;
            border-color: var(--accent);
            background: #fff;
            box-shadow: 0 0 0 3px var(--accent-ring);
        }
        input::placeholder { color: var(--text3); }

        /* ===== BUTTONS ===== */
        .btn {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            width: 100%;
            padding: 13px 20px;
            border: none;
            border-radius: 10px;
            font-family: inherit;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s cubic-bezier(0.4,0,0.2,1);
        }
        .btn:active { transform: scale(0.98); }
        .btn:disabled { opacity: 0.4; cursor: not-allowed; transform: none; }
        .btn-primary {
            background: var(--accent);
            color: #fff;
        }
        .btn-primary:hover:not(:disabled) { background: var(--accent-hover); box-shadow: 0 4px 12px rgba(10,155,142,0.25); }
        .btn-ghost {
            background: transparent;
            color: var(--text2);
            border: 1.5px solid var(--border);
        }
        .btn-ghost:hover { background: rgba(15,20,25,0.03); border-color: rgba(15,20,25,0.12); }
        .btn svg { width: 16px; height: 16px; }

        /* ===== NETWORK LIST ===== */
        .network-item {
            display: flex;
            align-items: center;
            gap: 14px;
            padding: 14px 20px;
            cursor: pointer;
            transition: background 0.15s;
            border-bottom: 1px solid var(--border);
        }
        .network-item:last-child { border-bottom: none; }
        .network-item:hover { background: var(--accent-light); }
        .network-icon {
            width: 36px; height: 36px;
            border-radius: 10px;
            background: var(--bg);
            display: flex;
            align-items: center;
            justify-content: center;
            flex-shrink: 0;
        }
        .network-icon svg { width: 18px; height: 18px; color: var(--text3); }
        .network-detail { flex: 1; min-width: 0; }
        .network-name {
            font-weight: 600;
            font-size: 14px;
            color: var(--text);
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .network-info { font-size: 12px; color: var(--text3); margin-top: 1px; }
        .rssi-bars {
            display: flex;
            gap: 2px;
            align-items: flex-end;
            height: 16px;
        }
        .rssi-bars span {
            width: 3px;
            border-radius: 1px;
            transition: background 0.2s;
        }
        .rssi-bars span.on { background: var(--accent); }
        .rssi-bars span.off { background: rgba(15,20,25,0.1); }

        /* ===== ERRORS ===== */
        .error-item {
            padding: 16px 20px;
            border-bottom: 1px solid var(--border);
            position: relative;
        }
        .error-item:last-child { border-bottom: none; }
        .error-item::before {
            content: '';
            position: absolute;
            left: 0; top: 8px; bottom: 8px;
            width: 3px;
            border-radius: 0 2px 2px 0;
            background: var(--red);
        }
        .error-head {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .error-code {
            font-family: 'SF Mono', 'Cascadia Code', 'Consolas', monospace;
            font-size: 11px;
            font-weight: 700;
            color: var(--red);
            background: rgba(229,72,77,0.08);
            padding: 2px 8px;
            border-radius: 4px;
        }
        .error-time {
            font-size: 11px;
            color: var(--text3);
            margin-left: auto;
        }
        .error-msg {
            font-size: 13px;
            color: var(--text);
            margin-top: 6px;
            line-height: 1.4;
        }
        .error-ctx {
            font-size: 12px;
            color: var(--text3);
            margin-top: 3px;
            font-family: 'SF Mono', 'Cascadia Code', 'Consolas', monospace;
        }

        /* ===== STATES ===== */
        .loader {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 40px 20px;
            color: var(--text3);
            font-size: 13px;
            gap: 12px;
        }
        .spinner {
            width: 24px; height: 24px;
            border: 2.5px solid var(--border);
            border-top-color: var(--accent);
            border-radius: 50%;
            animation: spin 0.7s linear infinite;
        }
        @keyframes spin { to { transform: rotate(360deg); } }
        .empty-state {
            text-align: center;
            padding: 48px 24px;
            color: var(--text3);
        }
        .empty-icon {
            width: 48px; height: 48px;
            margin: 0 auto 12px;
            opacity: 0.25;
        }
        .empty-state p { font-size: 14px; }
        .msg {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 12px 14px;
            border-radius: 10px;
            margin-bottom: 16px;
            font-size: 13px;
            font-weight: 500;
            animation: slideIn 0.25s cubic-bezier(0.4,0,0.2,1);
        }
        @keyframes slideIn {
            from { opacity: 0; transform: translateY(-8px); }
            to { opacity: 1; transform: translateY(0); }
        }
        .msg-ok { background: rgba(0,182,122,0.08); color: var(--green); }
        .msg-err { background: rgba(229,72,77,0.08); color: var(--red); }

        /* ===== FOOTER ===== */
        .footer {
            text-align: center;
            padding: 12px 24px 28px;
            max-width: 520px;
            margin: 0 auto;
        }
        .footer p {
            font-size: 11px;
            color: var(--text3);
            letter-spacing: 0.2px;
        }
    </style>
</head>
<body>

    <!-- HEADER -->
    <div class="header">
        <div class="header-inner">
            <div class="header-logo">
                <svg viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M12 2L12 6M12 18L12 22M2 12L6 12M18 12L22 12"/>
                    <circle cx="12" cy="12" r="4"/>
                </svg>
            </div>
            <div class="header-text">
                <h1>Medbox</h1>
                <div class="header-uuid" id="header-uuid">---</div>
            </div>
            <div class="header-status" id="header-conn">
                <div class="status-dot disconnected" id="status-dot"></div>
                <span id="conn-label">---</span>
            </div>
        </div>
    </div>

    <!-- NAVIGATION -->
    <div class="nav">
        <div class="nav-inner">
            <button class="tab active" onclick="showPage('status',this)">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg>
                Apercu
            </button>
            <button class="tab" onclick="showPage('wifi',this)">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><circle cx="12" cy="20" r="1"/></svg>
                WiFi
            </button>
            <button class="tab" onclick="showPage('errors',this)">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
                Journal
            </button>
            <button class="tab" onclick="showPage('console',this)">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/></svg>
                Console
            </button>
        </div>
    </div>

    <!-- CONTENT -->
    <div class="content">

        <!-- ========== STATUS PAGE ========== -->
        <div id="status" class="page active">
            <div class="card">
                <div class="card-head">
                    <div class="card-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="4" width="16" height="16" rx="2"/><path d="M9 9h6v6H9z"/><path d="M9 1v3M15 1v3M9 20v3M15 20v3M20 9h3M20 14h3M1 9h3M1 14h3"/></svg>
                    </div>
                    <div class="card-title">Systeme</div>
                </div>
                <div class="card-body">
                    <div class="row"><span class="label">Firmware</span><span class="value" id="firmware">---</span></div>
                    <div class="row"><span class="label">Uptime</span><span class="value" id="uptime">---</span></div>
                    <div class="row"><span class="label">Memoire libre</span><span class="value" id="heap">---</span></div>
                </div>
            </div>

            <div class="card">
                <div class="card-head">
                    <div class="card-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><circle cx="12" cy="20" r="1"/></svg>
                    </div>
                    <div class="card-title">WiFi Station</div>
                </div>
                <div class="card-body">
                    <div class="row"><span class="label">Etat</span><span id="sta-status">---</span></div>
                    <div class="row"><span class="label">Adresse IP</span><span class="value" id="sta-ip">---</span></div>
                    <div class="row"><span class="label">Signal</span><span class="value" id="sta-rssi">---</span></div>
                </div>
            </div>

            <div class="card">
                <div class="card-head">
                    <div class="card-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2L2 7l10 5 10-5-10-5z"/><path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/></svg>
                    </div>
                    <div class="card-title">Point d'Acces</div>
                </div>
                <div class="card-body">
                    <div class="row"><span class="label">Etat</span><span id="ap-status">---</span></div>
                    <div class="row"><span class="label">Nom</span><span class="value" id="ap-ssid">---</span></div>
                    <div class="row"><span class="label">Adresse IP</span><span class="value" id="ap-ip">---</span></div>
                </div>
            </div>

            <button class="btn btn-ghost" onclick="loadStatus()">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/></svg>
                Actualiser
            </button>
        </div>

        <!-- ========== WIFI PAGE ========== -->
        <div id="wifi" class="page">
            <div class="card">
                <div class="card-head">
                    <div class="card-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="11" width="20" height="11" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>
                    </div>
                    <div class="card-title">Connexion WiFi</div>
                </div>
                <div class="card-body">
                    <div id="wifi-msg"></div>
                    <div class="form-group">
                        <label class="form-label">Nom du reseau (SSID)</label>
                        <input type="text" id="wifi-ssid" placeholder="Selectionnez ou saisissez le nom">
                    </div>
                    <div class="form-group">
                        <label class="form-label">Mot de passe</label>
                        <input type="password" id="wifi-pass" placeholder="Saisissez le mot de passe">
                    </div>
                    <button class="btn btn-primary" onclick="saveWifi()">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"/><circle cx="12" cy="20" r="1"/></svg>
                        Se connecter
                    </button>
                </div>
            </div>

            <div class="card">
                <div class="card-head">
                    <div class="card-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                    </div>
                    <div class="card-title">Reseaux disponibles</div>
                </div>
                <div id="networks"><div class="loader"><div class="spinner"></div>Appuyez sur Scanner</div></div>
                <div style="padding:0 20px 20px;">
                    <button class="btn btn-ghost" onclick="scanWifi()">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                        Scanner les reseaux
                    </button>
                </div>
            </div>
        </div>

        <!-- ========== ERRORS PAGE ========== -->
        <div id="errors" class="page">
            <div class="card">
                <div class="card-head">
                    <div class="card-icon" style="background:rgba(229,72,77,0.08);">
                        <svg viewBox="0 0 24 24" fill="none" stroke="var(--red)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
                    </div>
                    <div class="card-title">Journal des erreurs</div>
                </div>
                <div id="error-list"><div class="loader"><div class="spinner"></div>Chargement...</div></div>
            </div>
            <button class="btn btn-ghost" onclick="loadErrors()">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/></svg>
                Actualiser
            </button>
        </div>

        <!-- ========== CONSOLE PAGE ========== -->
        <div id="console" class="page">
            <div class="card">
                <div class="card-head">
                    <div class="card-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/></svg>
                    </div>
                    <div class="card-title">Console live <span id="console-status" style="font-weight:400;color:#8a94a6;font-size:12px;margin-left:6px;">(deconnecte)</span></div>
                </div>
                <div id="console-log" style="font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12px;line-height:1.5;background:#0b0f14;color:#c8d6e5;padding:12px 14px;height:70vh;overflow-y:scroll;white-space:pre-wrap;word-break:break-word;"></div>
            </div>
        </div>
    </div>

    <!-- FOOTER -->
    <div class="footer">
        <p id="footer-info">Medbox Firmware v1.0.0</p>
    </div>

    <script>
        var consoleWS = null;
        function connectConsole() {
            if (consoleWS && (consoleWS.readyState === 0 || consoleWS.readyState === 1)) return;
            var log = document.getElementById('console-log');
            var st = document.getElementById('console-status');
            consoleWS = new WebSocket('ws://' + location.host + '/ws/console');
            consoleWS.onopen = function() { st.textContent = '(connecte)'; };
            consoleWS.onmessage = function(e) {
                var d = document.createElement('div');
                d.textContent = e.data;
                log.appendChild(d);
                while (log.childNodes.length > 500) log.removeChild(log.firstChild);
                log.scrollTop = log.scrollHeight;
            };
            consoleWS.onclose = function() { st.textContent = '(deconnecte)'; };
        }

        function showPage(name, btn) {
            document.querySelectorAll('.page').forEach(function(p) { p.classList.remove('active'); });
            document.querySelectorAll('.tab').forEach(function(t) { t.classList.remove('active'); });
            document.getElementById(name).classList.add('active');
            btn.classList.add('active');
            if (name === 'status') loadStatus();
            if (name === 'errors') loadErrors();
            if (name === 'console') { connectConsole(); }
            else if (consoleWS) { consoleWS.close(); consoleWS = null; }
        }

        function formatUptime(sec) {
            var d = Math.floor(sec / 86400);
            var h = Math.floor((sec % 86400) / 3600);
            var m = Math.floor((sec % 3600) / 60);
            if (d > 0) return d + 'j ' + h + 'h ' + m + 'm';
            if (h > 0) return h + 'h ' + m + 'm';
            return m + 'm ' + (sec % 60) + 's';
        }

        function toHex(n) {
            return '0x' + n.toString(16).toUpperCase().padStart(4, '0');
        }

        function formatTime(sec) {
            var h = Math.floor(sec / 3600);
            var m = Math.floor((sec % 3600) / 60);
            var s = sec % 60;
            return 'il y a ' + (h > 0 ? h + 'h ' : '') + (m > 0 ? m + 'm ' : '') + s + 's';
        }

        function rssiToBar(rssi) {
            var bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : 1;
            var html = '<div class="rssi-bars">';
            for (var i = 1; i <= 4; i++) {
                var h = 4 + i * 3;
                html += '<span class="' + (i <= bars ? 'on' : 'off') + '" style="height:' + h + 'px"></span>';
            }
            return html + '</div>';
        }

        function loadStatus() {
            fetch('/api/status')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    document.getElementById('header-uuid').textContent = d.uuid || '---';
                    document.getElementById('firmware').textContent = 'v' + d.firmware_version;
                    document.getElementById('uptime').textContent = formatUptime(d.uptime_sec);
                    document.getElementById('heap').textContent = Math.round(d.free_heap / 1024) + ' KB';
                    document.getElementById('footer-info').textContent = 'Medbox Firmware v' + d.firmware_version + ' / ESP-IDF ' + (d.idf_version || '---');

                    var sta = d.wifi_sta;
                    var dot = document.getElementById('status-dot');
                    var connLabel = document.getElementById('conn-label');
                    if (sta.connected) {
                        dot.className = 'status-dot connected';
                        connLabel.textContent = 'En ligne';
                        document.getElementById('sta-status').innerHTML = '<span class="badge badge-ok"><span class="badge-dot"></span>Connecte</span>';
                    } else {
                        dot.className = 'status-dot disconnected';
                        connLabel.textContent = 'Hors ligne';
                        document.getElementById('sta-status').innerHTML = '<span class="badge badge-err"><span class="badge-dot"></span>Deconnecte</span>';
                    }
                    document.getElementById('sta-ip').textContent = sta.ip || '---';
                    document.getElementById('sta-rssi').textContent = sta.rssi ? sta.rssi + ' dBm' : '---';

                    var ap = d.wifi_ap;
                    document.getElementById('ap-status').innerHTML = ap.active ?
                        '<span class="badge badge-ok"><span class="badge-dot"></span>Actif</span>' :
                        '<span class="badge badge-warn"><span class="badge-dot"></span>Inactif</span>';
                    document.getElementById('ap-ssid').textContent = ap.ssid || '---';
                    document.getElementById('ap-ip').textContent = ap.ip || '---';
                })
                .catch(function(e) { console.error('Erreur:', e); });
        }

        function scanWifi() {
            document.getElementById('networks').innerHTML = '<div class="loader"><div class="spinner"></div>Recherche en cours...</div>';
            fetch('/api/wifi/scan')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.networks.length === 0) {
                        document.getElementById('networks').innerHTML = '<div class="empty-state"><svg class="empty-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><circle cx="12" cy="20" r="1"/></svg><p>Aucun reseau trouve</p></div>';
                        return;
                    }
                    var html = '';
                    d.networks.forEach(function(n) {
                        html += '<div class="network-item" onclick="selectNetwork(\'' + n.ssid.replace(/'/g, "\\'") + '\')">';
                        html += '<div class="network-icon"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><circle cx="12" cy="20" r="1"/></svg></div>';
                        html += '<div class="network-detail"><div class="network-name">' + n.ssid + '</div>';
                        html += '<div class="network-info">' + n.security + '</div></div>';
                        html += rssiToBar(n.rssi) + '</div>';
                    });
                    document.getElementById('networks').innerHTML = html;
                })
                .catch(function(e) {
                    document.getElementById('networks').innerHTML = '<div class="empty-state"><p>Erreur de scan</p></div>';
                });
        }

        function selectNetwork(ssid) {
            document.getElementById('wifi-ssid').value = ssid;
            document.getElementById('wifi-pass').focus();
        }

        function saveWifi() {
            var ssid = document.getElementById('wifi-ssid').value;
            var pass = document.getElementById('wifi-pass').value;
            if (!ssid) {
                showMsg('wifi-msg', 'Veuillez entrer un nom de reseau', false);
                return;
            }
            fetch('/api/wifi/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ssid: ssid, password: pass})
            })
            .then(function(r) { return r.json(); })
            .then(function(d) {
                showMsg('wifi-msg', d.message, d.success);
                if (d.success) setTimeout(loadStatus, 3000);
            })
            .catch(function(e) { showMsg('wifi-msg', 'Erreur de connexion', false); });
        }

        function showMsg(id, msg, ok) {
            var el = document.getElementById(id);
            var icon = ok ?
                '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>' :
                '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>';
            el.innerHTML = '<div class="msg ' + (ok ? 'msg-ok' : 'msg-err') + '">' + icon + msg + '</div>';
            setTimeout(function() { el.innerHTML = ''; }, 5000);
        }

        function loadErrors() {
            document.getElementById('error-list').innerHTML = '<div class="loader"><div class="spinner"></div>Chargement...</div>';
            fetch('/api/errors')
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    if (d.errors.length === 0) {
                        document.getElementById('error-list').innerHTML = '<div class="empty-state"><svg class="empty-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg><p>Aucune erreur enregistree</p></div>';
                        return;
                    }
                    var html = '';
                    d.errors.forEach(function(e) {
                        html += '<div class="error-item">';
                        html += '<div class="error-head"><span class="error-code">' + toHex(e.code) + '</span>';
                        if (e.timestamp) html += '<span class="error-time">' + formatTime(e.timestamp) + '</span>';
                        html += '</div>';
                        html += '<div class="error-msg">' + e.message + '</div>';
                        if (e.context) html += '<div class="error-ctx">' + e.context + '</div>';
                        html += '</div>';
                    });
                    document.getElementById('error-list').innerHTML = html;
                })
                .catch(function(e) {
                    document.getElementById('error-list').innerHTML = '<div class="empty-state"><p>Erreur de chargement</p></div>';
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
