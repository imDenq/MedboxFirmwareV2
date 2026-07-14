#pragma once

#include <stdbool.h>

/**
 * boot_ui - Affichage de boot "pro" sur la console serie.
 *
 * Banniere ASCII MEDBOX + sequence d'initialisation animee (spinner puis
 * [ OK ] / [FAIL] / [WARN]), facon boot systemd. Ecrit directement via printf
 * (pas via ESP_LOG) pour un rendu propre, sans tag ni timestamp.
 *
 * Astuce : couper les logs INFO pendant le boot
 *   esp_log_level_set("*", ESP_LOG_WARN);   // avant la sequence
 *   ... boot_ui_* ...
 *   esp_log_level_set("*", ESP_LOG_INFO);    // apres, pour reprendre les logs
 */

// Banniere ASCII + ligne firmware/ESP-IDF.
void boot_ui_banner(const char *fw_version, const char *idf_version);

// En-tete de section (ex: "Initialisation du systeme").
void boot_ui_section(const char *title);

// Etape "instantanee" : anime un spinner un court instant puis affiche
// [ OK ] (vert) si ok, sinon [FAIL] (rouge).
void boot_ui_step(const char *label, bool ok);

// Etape non bloquante en echec : affiche [WARN] (jaune).
void boot_ui_step_warn(const char *label);

// Ligne d'info discrete : "      label : value".
void boot_ui_info(const char *label, const char *value);

// Spinner "vivant" pour une vraie attente (ex: connexion WiFi) :
//   boot_ui_spin_start("Connexion WiFi");
//   while (...) { boot_ui_spin_tick(); vTaskDelay(...); }
//   boot_ui_spin_end(connected);   // OK si true, WARN sinon
void boot_ui_spin_start(const char *label);
void boot_ui_spin_tick(void);
void boot_ui_spin_end(bool ok);

// Bandeau de fin (vert si success, rouge sinon).
void boot_ui_footer(bool success);
