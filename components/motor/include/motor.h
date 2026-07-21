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

/**
 * Calibration : micropas pour avancer d'une case, exprimee au centieme.
 *
 * Un micropas est une impulsion physique : on ne peut pas en envoyer une demie.
 * La partie decimale est donc reportee d'une case a l'autre par un accumulateur
 * (148.50 -> 148 pas, puis 149, puis 148...). La moyenne est exacte et la
 * derive ne s'accumule pas sur un tour complet.
 */
#define MOTOR_STEPS_SCALE 100  // facteur de conversion valeur reelle <-> centiemes
#define MOTOR_STEPS_PER_SLOT_DEFAULT_X100 14500
#define MOTOR_STEPS_PER_SLOT_MIN_X100 100
#define MOTOR_STEPS_PER_SLOT_MAX_X100 400000

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

/** Nombre de cases sur la roue (21 utiles + 1 de calibration). */
#define MOTOR_SLOTS_PER_TURN 22

/**
 * Fait un tour complet (MOTOR_SLOTS_PER_TURN cases) en tache de fond.
 * Sert a valider la calibration : la roue doit revenir exactement a son point
 * de depart. C'est le test le plus revelateur, l'erreur par case y est cumulee.
 * Ignore si un mouvement est deja en cours.
 */
void motor_start_full_turn(void);

/** Vrai si un test / mouvement de fond est en cours. */
bool motor_test_running(void);

// ============================================
// Calibration (micropas par case, persistee en NVS)
// ============================================

/** Micropas par case, en centiemes (14850 = 148.50 micropas). */
int motor_get_steps_per_slot_x100(void);

/**
 * Definit et persiste (NVS) le nombre de micropas par case, en centiemes.
 * Remet l'accumulateur de fraction a zero (nouvelle calibration = nouveau depart).
 * @param steps_x100 Valeur bornee a [MOTOR_STEPS_PER_SLOT_MIN_X100, ..._MAX_X100].
 * @return true si la valeur a ete acceptee et sauvegardee.
 */
bool motor_set_steps_per_slot_x100(int steps_x100);

#ifdef __cplusplus
}
#endif

#endif  // MOTOR_H
