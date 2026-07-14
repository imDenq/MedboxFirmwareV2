#include "motor.h"

#include <math.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "motor";

// --- Pinout (confirme schema U4 ESP32-DevKitC) ---
#define MOT_DIR GPIO_NUM_32
#define MOT_STEP GPIO_NUM_33
#define MOT_ENABLED GPIO_NUM_27
#define MOT_MS1 GPIO_NUM_13
#define MOT_MS2 GPIO_NUM_12
#define MOT_MS3 GPIO_NUM_14
#define BUZ_PIN GPIO_NUM_5
#define LED_MEDIC GPIO_NUM_17

#define MOT_ENABLE_LEVEL 0   // ENABLE actif a LOW
#define MOT_DISABLE_LEVEL 1  // HIGH = moteur coupe (repos)
#define MOT_DIR_DISPENSE 1   // sens de distribution (a caler si besoin)

// --- Calibration mecanique — A AJUSTER sur le vrai materiel ---
// 3200 micropas/tour (moteur 200 pas * 1/16) / 22 cases ~= 145 micropas/case.
#define STEPS_PER_SLOT 145
#define DISTRIB_PER_DAY 3

// Rampe de vitesse : delai (us) par demi-pas. Lent au depart/arrivee (couple),
// rapide en croisiere. Profil triangulaire (accel puis decel) -> arret propre.
#define DELAY_SLOW_US 1600
#define DELAY_FAST_US 400

#define PAUSE_BETWEEN_MS 600  // pause entre deux distributions du test

// --- Buzzer (LEDC / PWM) ---
#define BUZ_MODE LEDC_LOW_SPEED_MODE
#define BUZ_TIMER LEDC_TIMER_0
#define BUZ_CHANNEL LEDC_CHANNEL_0

static volatile bool s_test_running = false;

static void buzzer_tone(int freq)
{
    ledc_set_freq(BUZ_MODE, BUZ_TIMER, freq);
    ledc_set_duty(BUZ_MODE, BUZ_CHANNEL, 512);  // ~50 %
    ledc_update_duty(BUZ_MODE, BUZ_CHANNEL);
}

static void buzzer_off(void)
{
    ledc_set_duty(BUZ_MODE, BUZ_CHANNEL, 0);
    ledc_update_duty(BUZ_MODE, BUZ_CHANNEL);
}

// Signal court : bip grave puis bip aigu.
static void buzzer_double_beep(void)
{
    buzzer_tone(600);  // grave
    vTaskDelay(pdMS_TO_TICKS(120));
    buzzer_off();
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(2000);  // aigu
    vTaskDelay(pdMS_TO_TICKS(120));
    buzzer_off();
}

// Delai du pas i sur `total` : profil triangulaire (lent -> rapide -> lent).
static int ramp_delay(int i, int total)
{
    if (total <= 1) {
        return DELAY_FAST_US;
    }
    float t = (float)i / (float)(total - 1);       // 0..1
    float tri = 1.0f - fabsf(2.0f * t - 1.0f);     // 0 aux extremites, 1 au milieu
    return DELAY_SLOW_US - (int)((DELAY_SLOW_US - DELAY_FAST_US) * tri);
}

void motor_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << MOT_DIR) | (1ULL << MOT_STEP) | (1ULL << MOT_ENABLED) |
                        (1ULL << MOT_MS1) | (1ULL << MOT_MS2) | (1ULL << MOT_MS3) |
                        (1ULL << LED_MEDIC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    gpio_set_level(MOT_STEP, 0);
    gpio_set_level(MOT_DIR, 0);
    gpio_set_level(MOT_ENABLED, MOT_DISABLE_LEVEL);  // moteur coupe au repos
    gpio_set_level(LED_MEDIC, 0);

    // Microstepping le plus fin : MS1/MS2/MS3 = HIGH
    gpio_set_level(MOT_MS1, 1);
    gpio_set_level(MOT_MS2, 1);
    gpio_set_level(MOT_MS3, 1);

    // Buzzer via LEDC
    ledc_timer_config_t timer = {
        .speed_mode = BUZ_MODE,
        .timer_num = BUZ_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t chan = {
        .gpio_num = BUZ_PIN,
        .speed_mode = BUZ_MODE,
        .channel = BUZ_CHANNEL,
        .timer_sel = BUZ_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&chan);

    ESP_LOGI(TAG, "Moteur initialise (desactive), microstepping 1/16");
}

// Avance d'une case : active le moteur, chirp, rampe de pas, puis DESACTIVE.
static void advance_one_slot(void)
{
    gpio_set_level(MOT_DIR, MOT_DIR_DISPENSE);
    gpio_set_level(MOT_ENABLED, MOT_ENABLE_LEVEL);
    esp_rom_delay_us(500);  // laisser le driver s'activer

    // Chirp court (feedback sonore)
    buzzer_tone(1500);
    vTaskDelay(pdMS_TO_TICKS(40));
    buzzer_off();

    for (int i = 0; i < STEPS_PER_SLOT; i++) {
        int d = ramp_delay(i, STEPS_PER_SLOT);
        gpio_set_level(MOT_STEP, 1);
        esp_rom_delay_us(d);
        gpio_set_level(MOT_STEP, 0);
        esp_rom_delay_us(d);
    }

    // SECURITE : toujours desactiver le moteur apres le mouvement
    gpio_set_level(MOT_ENABLED, MOT_DISABLE_LEVEL);
}

void motor_advance_slots(int slots)
{
    for (int s = 0; s < slots; s++) {
        advance_one_slot();
    }
}

static void motor_test_task(void *arg)
{
    int count = (int)(intptr_t)arg;
    ESP_LOGI(TAG, "Test distribution demarre : %d avances d'une case", count);

    // Signal de demarrage : bip grave + bip aigu
    buzzer_double_beep();

    for (int i = 0; i < count; i++) {
        gpio_set_level(LED_MEDIC, 1);
        advance_one_slot();
        gpio_set_level(LED_MEDIC, 0);
        ESP_LOGI(TAG, "Distribution %d/%d effectuee", i + 1, count);
        vTaskDelay(pdMS_TO_TICKS(PAUSE_BETWEEN_MS));
    }

    ESP_LOGI(TAG, "Test distribution termine");
    s_test_running = false;
    vTaskDelete(NULL);
}

void motor_start_test(int days)
{
    if (s_test_running) {
        ESP_LOGW(TAG, "Test de distribution deja en cours");
        return;
    }
    if (days <= 0) {
        days = 7;
    }
    int count = days * DISTRIB_PER_DAY;
    s_test_running = true;
    if (xTaskCreate(motor_test_task, "motor_test", 4096, (void *)(intptr_t)count, 4, NULL) !=
        pdPASS) {
        s_test_running = false;
        ESP_LOGE(TAG, "Impossible de lancer la tache de test");
    }
}

bool motor_test_running(void)
{
    return s_test_running;
}
