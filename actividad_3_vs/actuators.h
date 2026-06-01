// ============================================================================
//  actuators.h — Interfaz pública del módulo de actuadores (Actividad 3).
//
//  Reúne TODOS los actuadores del sistema integrado:
//    Ascensor (Act2):
//      - Servo de la cabina (movimiento entre las 5 plantas).
//      - LED calentador / LED enfriador (control térmico ON-OFF).
//      - LED lámpara con PWM (control de iluminación).
//      - Humidificador / deshumidificador (estado lógico, sin LED físico).
//    Clasificador (Act1):
//      - Servo de la compuerta de desvío de cajas (NEUTRAL/ASCENSOR/ESCALERA).
//      - LED verde (→ ASCENSOR) / LED rojo (→ ESCALERA).
//      - Buzzer (pitido en cada clasificación).
//
//  Son dos servos en pines distintos (cabina GPIO19, compuerta GPIO13): no
//  hay conflicto de hardware.
// ============================================================================

#pragma once
#include <stdint.h>

void initActuators();

// ---- Ascensor: cabina (servo GPIO19) --------------------------------------
// Mueve el servo de la cabina a la planta destino (0..NUM_PLANTAS-1).
void cabinMoveTo(int8_t planta);
int  cabinAngle();

// ---- Ascensor: control térmico --------------------------------------------
void heaterSet(bool on);
void coolerSet(bool on);
bool heaterIsOn();
bool coolerIsOn();

// ---- Ascensor: lámpara con PWM (duty 0..255) ------------------------------
void    lampSetPwm(uint8_t duty);
void    lampSetOn(bool on);
bool    lampIsOn();
uint8_t lampPwm();

// ---- Ascensor: humedad (estado lógico simulado) ---------------------------
void humidifierSet(bool on);
void dehumidifierSet(bool on);
bool humidifierIsOn();
bool dehumidifierIsOn();

// ---- Clasificador: compuerta (servo GPIO13) -------------------------------
void servoNeutral();
void servoAscensor();
void servoEscalera();

// ---- Clasificador: LEDs y buzzer ------------------------------------------
void setLedAscensor(bool on);   // LED verde.
void setLedEscalera(bool on);   // LED rojo.
void clearLeds();               // Apaga ambos LEDs del clasificador.
void beep();                    // Pitido corto (no bloqueante).
