/**
 * @file motor.h
 * @brief Pilotage du moteur pas-a-pas de la roue de distribution.
 *
 * Driver type A4988/DRV8825 : DIR, STEP, ENABLE (actif LOW), MS1/2/3 (microstepping).
 * Regle de securite : le moteur est TOUJOURS remis en disable apres un mouvement
 * (economie de courant + ne jamais couper l'alim moteur enable).
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Valeur de calibration par defaut : micropas pour avancer d'une case. */
#define MOTOR_STEPS_PER_SLOT_DEFAULT 145
#define MOTOR_STEPS_PER_SLOT_MIN 1
#define MOTOR_STEPS_PER_SLOT_MAX 4000

/**
 * Configure les GPIO, le microstepping (le plus fin) et le buzzer. Moteur desactive.
 * Recharge aussi la valeur de calibration depuis la NVS si elle existe.
 */
void motor_init(void);

/** Avance la roue de `slots` cases (rampe + buzzer), puis desactive le moteur. Bloquant. */
void motor_advance_slots(int slots);

/**
 * Lance un test de distribution en tache de fond (non bloquant) :
 * `days` * 3 avances d'une case (3 distributions/jour), pause entre chaque.
 * Ignore si un test est deja en cours.
 */
void motor_start_test(int days);

/**
 * Avance d'UNE seule case en tache de fond (non bloquant), avec le double bip.
 * Utilise pour le test de distribution et pour l'essai de calibration.
 * Ignore si un mouvement est deja en cours.
 */
void motor_dispense_once(void);

/** Vrai si un test / mouvement de fond est en cours. */
bool motor_test_running(void);

// ============================================
// Calibration (micropas par case, persistee en NVS)
// ============================================

/** Nombre de micropas actuellement utilise pour avancer d'une case. */
int motor_get_steps_per_slot(void);

/**
 * Definit et persiste (NVS) le nombre de micropas par case.
 * @param steps Valeur bornee a [MOTOR_STEPS_PER_SLOT_MIN, MOTOR_STEPS_PER_SLOT_MAX].
 * @return true si la valeur a ete acceptee et sauvegardee.
 */
bool motor_set_steps_per_slot(int steps);

#ifdef __cplusplus
}
#endif

#endif  // MOTOR_H
