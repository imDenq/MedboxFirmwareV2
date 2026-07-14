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

/** Configure les GPIO, le microstepping (le plus fin) et le buzzer. Moteur desactive. */
void motor_init(void);

/** Avance la roue de `slots` cases (rampe + buzzer), puis desactive le moteur. Bloquant. */
void motor_advance_slots(int slots);

/**
 * Lance un test de distribution en tache de fond (non bloquant) :
 * `days` * 3 avances d'une case (3 distributions/jour), pause entre chaque.
 * Ignore si un test est deja en cours.
 */
void motor_start_test(int days);

/** Vrai si un test de distribution est en cours. */
bool motor_test_running(void);

#ifdef __cplusplus
}
#endif

#endif  // MOTOR_H
