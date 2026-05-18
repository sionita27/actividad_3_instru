// ============================================================================
//  elevator.h — Máquina de estados del ascensor.
//
//  La FSM atiende llamadas (botones físicos + mando IR) y mueve la cabina
//  con el servo entre 5 plantas. Mantiene una cola FIFO de plantas
//  pendientes y abre la puerta DOOR_OPEN_MS tras cada parada.
//
//  Estados:
//    IDLE        — cabina parada, cola vacía.
//    MOVING      — servo en movimiento hacia floorTarget.
//    DOOR_OPEN   — cabina parada en floorCurrent con la puerta abierta.
//    OCCUPIED    — DOOR_OPEN extendido por el PIR (paso 9).
//    ALARM       — incidencia (paso 9, opcional).
//
//  En el paso 8 implementamos sólo IDLE/MOVING/DOOR_OPEN sin cola
//  (1 destino a la vez). En el paso 9 se añade la cola y los estados
//  OCCUPIED/ALARM.
// ============================================================================

#pragma once
#include <stdint.h>

enum class ElevatorState : uint8_t {
    IDLE,
    MOVING,
    DOOR_OPEN,
    OCCUPIED,
    ALARM
};

void elevator_setup();

// Encola una petición de planta. Si la cola está llena, se descarta.
// Si la planta solicitada coincide con la actual y no hay nada en
// movimiento, abre la puerta brevemente.
void elevator_request(int8_t planta);

// Tick periódico de la FSM. Llamar en cada loop.
void elevator_tick();

int8_t          elevator_currentFloor();
int8_t          elevator_targetFloor();
ElevatorState   elevator_state();
const char*     elevator_stateName();
