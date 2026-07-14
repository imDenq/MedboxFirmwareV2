#include "boot_ui.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- Couleurs ANSI ---
#define C_RESET  "\033[0m"
#define C_DIM    "\033[2m"
#define C_BOLD   "\033[1m"
#define C_CYAN   "\033[36m"
#define C_GREEN  "\033[32m"
#define C_RED    "\033[31m"
#define C_YELLOW "\033[33m"

// --- Timings (ms) ---
#define BOOT_UI_STEP_MS   260   // duree d'animation d'une etape "instantanee"
#define BOOT_UI_FRAME_MS  80    // duree d'une frame de spinner

// Spinner braille (UTF-8)
static const char *const SPIN[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼",
    "⠴", "⠦", "⠧", "⠇", "⠏",
};
#define SPIN_N 10

static int s_spin_i = 0;
static const char *s_spin_label = "";

// Banniere "ANSI Shadow" MEDBOX
static const char *const LOGO[] = {
    "███╗   ███╗███████╗██████╗ ██████╗  ██████╗ ██╗  ██╗",
    "████╗ ████║██╔════╝██╔══██╗██╔══██╗██╔═══██╗╚██╗██╔╝",
    "██╔████╔██║█████╗  ██║  ██║██████╔╝██║   ██║ ╚███╔╝ ",
    "██║╚██╔╝██║██╔══╝  ██║  ██║██╔══██╗██║   ██║ ██╔██╗ ",
    "██║ ╚═╝ ██║███████╗██████╔╝██████╔╝╚██████╔╝██╔╝ ██╗",
    "╚═╝     ╚═╝╚══════╝╚═════╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝",
};
#define LOGO_ROWS 6

static void print_spinner_line(const char *label)
{
    printf("\r  [ " C_CYAN "%s" C_RESET " ] %s", SPIN[s_spin_i % SPIN_N], label);
    fflush(stdout);
    s_spin_i++;
}

void boot_ui_banner(const char *fw_version, const char *idf_version)
{
    printf("\n\n" C_CYAN);
    for (int i = 0; i < LOGO_ROWS; i++) {
        printf("  %s\n", LOGO[i]);
    }
    printf(C_RESET);
    printf("  " C_BOLD "Distributeur intelligent de medicaments" C_RESET "\n");
    printf("  " C_DIM "firmware v%s  ·  ESP-IDF %s" C_RESET "\n\n",
           fw_version ? fw_version : "?", idf_version ? idf_version : "?");
    fflush(stdout);
}

void boot_ui_section(const char *title)
{
    printf("\n  " C_BOLD "%s" C_RESET "\n", title);
    fflush(stdout);
}

void boot_ui_step(const char *label, bool ok)
{
    int frames = BOOT_UI_STEP_MS / BOOT_UI_FRAME_MS;
    for (int i = 0; i < frames; i++) {
        print_spinner_line(label);
        vTaskDelay(pdMS_TO_TICKS(BOOT_UI_FRAME_MS));
    }
    if (ok) {
        printf("\r  [ " C_GREEN "OK" C_RESET " ] %s\n", label);
    } else {
        printf("\r  [" C_RED "FAIL" C_RESET "] %s\n", label);
    }
    fflush(stdout);
}

void boot_ui_step_warn(const char *label)
{
    int frames = BOOT_UI_STEP_MS / BOOT_UI_FRAME_MS;
    for (int i = 0; i < frames; i++) {
        print_spinner_line(label);
        vTaskDelay(pdMS_TO_TICKS(BOOT_UI_FRAME_MS));
    }
    printf("\r  [" C_YELLOW "WARN" C_RESET "] %s\n", label);
    fflush(stdout);
}

void boot_ui_info(const char *label, const char *value)
{
    printf("       " C_DIM "%s:" C_RESET " " C_BOLD "%s" C_RESET "\n", label, value);
    fflush(stdout);
}

void boot_ui_spin_start(const char *label)
{
    s_spin_label = label ? label : "";
    print_spinner_line(s_spin_label);
}

void boot_ui_spin_tick(void)
{
    print_spinner_line(s_spin_label);
}

void boot_ui_spin_end(bool ok)
{
    if (ok) {
        printf("\r  [ " C_GREEN "OK" C_RESET " ] %s\n", s_spin_label);
    } else {
        printf("\r  [" C_YELLOW "WARN" C_RESET "] %s\n", s_spin_label);
    }
    fflush(stdout);
}

void boot_ui_footer(bool success)
{
    if (success) {
        printf("\n  " C_GREEN "✔ Systeme pret" C_RESET "\n\n");
    } else {
        printf("\n  " C_RED "✘ Demarrage incomplet" C_RESET "\n\n");
    }
    fflush(stdout);
}
