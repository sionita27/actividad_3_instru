// ============================================================================
//  classifier.h — Clasificador de cajas por altura (heredado de Actividad 1).
//
//  Mide la altura de las cajas con el HC-SR04 y, mediante una FSM con
//  anti-rebote por número de muestras, las clasifica en:
//    - ALTA  → ASCENSOR : además dispara el puente funcional hacia el
//              ascensor (mapea la altura a una planta y llama
//              elevator_request()).
//    - BAJA  → ESCALERA : la caja se desvía a la escalera mecánica.
//
//  Estados de la FSM:
//    IDLE       — sin caja delante.
//    MEASURING  — acumulando muestras consecutivas para descartar ruido.
//    CLASSIFY   — caja confirmada; cartel + actuadores activos.
//    WAIT_CLEAR — esperando a que la caja libere el sensor.
//
//  El módulo es autónomo: gestiona su propia cadencia (CLASIFICADOR_PERIODO_MS)
//  y el botón de reset de contadores. El .ino solo llama a classifier_tick().
// ============================================================================

#pragma once
#include <stdint.h>

enum class ClassifierState : uint8_t { IDLE, MEASURING, CLASSIFY, WAIT_CLEAR };

// Inicializa el botón de reset y el estado de la FSM. Llamar UNA vez en setup().
void classifier_setup();

// Tick periódico (llamar en cada loop). Gestiona internamente la cadencia
// del HC-SR04 y el botón de reset; no bloquea.
void classifier_tick();

// ---- Estado de la FSM -----------------------------------------------------
ClassifierState classifier_state();
const char*     classifier_stateName();

// ---- Última caja clasificada ---------------------------------------------
int         classifier_lastPiece();      // Nº correlativo (0 si aún ninguna).
int         classifier_lastHeightCm();   // Altura medida de la última caja.
const char* classifier_lastClass();      // "ASCENSOR" / "ESCALERA" / "".
int8_t      classifier_lastFloor();      // Planta destino (-1 si ESCALERA/ninguna).

// ---- Contadores acumulados (reseteables con el botón de reset) -----------
int classifier_totalAsc();
int classifier_totalEsc();
