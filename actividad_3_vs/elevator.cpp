// ============================================================================
//  elevator.cpp — FSM completa del ascensor con cola FIFO de plantas.
//
//  Flujo principal:
//    IDLE
//      └─ recibe petición → encolar → MOVING(target)
//    MOVING
//      └─ servo terminado (timer) → DOOR_OPEN(t0)
//    DOOR_OPEN
//      ├─ PIR=1 dentro de la ventana → OCCUPIED
//      └─ timeout DOOR_OPEN_MS → siguiente_de_cola_o_IDLE
//    OCCUPIED
//      ├─ nueva petición o PIR=0 sostenido → cierra puerta y sigue
//      └─ timeout largo → ALARM (configurable, opcional)
//    ALARM
//      └─ reset manual → IDLE  (gestionado fuera de la FSM)
//
//  Cola FIFO con NO duplicados: si la planta ya está pendiente, se ignora
//  la petición. Tamaño definido en config.h::ELEVATOR_QUEUE_LEN.
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "elevator.h"
#include "actuators.h"
#include "sensors.h"

static ElevatorState st          = ElevatorState::IDLE;
static int8_t        floorCur    = 0;
static int8_t        floorTgt    = 0;
static uint32_t      tStateEnter = 0;

// Cola FIFO circular -------------------------------------------------------
static int8_t   queue[ELEVATOR_QUEUE_LEN];
static uint8_t  qHead   = 0;        // posición de extracción
static uint8_t  qTail   = 0;        // posición de inserción
static uint8_t  qCount  = 0;        // elementos actualmente en cola

static const char* NAMES[] = { "IDLE", "MOVING", "DOOR_OPEN", "OCCUPIED", "ALARM" };

// ---------------------------------------------------------------------------
//  Helpers de cola
// ---------------------------------------------------------------------------
static bool queueContains(int8_t planta) {
    for (uint8_t i = 0; i < qCount; ++i) {
        uint8_t idx = (qHead + i) % ELEVATOR_QUEUE_LEN;
        if (queue[idx] == planta) return true;
    }
    return false;
}

static bool queuePush(int8_t planta) {
    if (qCount >= ELEVATOR_QUEUE_LEN) return false;
    if (queueContains(planta))         return false;
    queue[qTail] = planta;
    qTail = (qTail + 1) % ELEVATOR_QUEUE_LEN;
    qCount++;
    return true;
}

static int8_t queuePop() {
    if (qCount == 0) return -1;
    int8_t v = queue[qHead];
    qHead = (qHead + 1) % ELEVATOR_QUEUE_LEN;
    qCount--;
    return v;
}

// ---------------------------------------------------------------------------
//  Cambio de estado
// ---------------------------------------------------------------------------
static void enter(ElevatorState s) {
    st          = s;
    tStateEnter = millis();
    Serial.printf("[ELV] estado=%s floor=%d target=%d cola=%u\r\n",
                  NAMES[(int) s], floorCur, floorTgt, (unsigned) qCount);
}

// Avanza al siguiente destino de la cola si hay; si no, vuelve a IDLE.
static void serveNextOrIdle() {
    int8_t next = queuePop();
    if (next < 0) {
        enter(ElevatorState::IDLE);
        return;
    }
    floorTgt = next;
    if (next == floorCur) {
        // Ya estamos en esa planta: solo abrimos puerta.
        enter(ElevatorState::DOOR_OPEN);
    } else {
        cabinMoveTo(next);
        enter(ElevatorState::MOVING);
    }
}

// ===========================================================================
//  API pública
// ===========================================================================
void elevator_setup() {
    floorCur = 0;
    floorTgt = 0;
    qHead = qTail = qCount = 0;
    cabinMoveTo(0);
    enter(ElevatorState::IDLE);
}

void elevator_request(int8_t planta) {
    if (planta < 0 || planta >= NUM_PLANTAS) return;

    // Si ya estamos parados en la misma planta, sólo abrimos puerta.
    if ((st == ElevatorState::IDLE || st == ElevatorState::DOOR_OPEN ||
         st == ElevatorState::OCCUPIED) && planta == floorCur) {
        floorTgt = planta;
        // Si ya estamos en DOOR_OPEN/OCCUPIED dejamos que continúe; si IDLE
        // forzamos un DOOR_OPEN breve.
        if (st == ElevatorState::IDLE) enter(ElevatorState::DOOR_OPEN);
        return;
    }

    // Encolar la planta. Duplicados se descartan.
    bool ok = queuePush(planta);
    if (!ok) {
        Serial.printf("[ELV] cola llena/duplicado descartado planta=%d\r\n",
                      (int) planta);
        return;
    }

    // Si la cabina está ociosa, arranca el viaje al primer (y único) destino.
    if (st == ElevatorState::IDLE) {
        serveNextOrIdle();
    }
}

// Entra o sale del estado ALARM por orden del autodiagnóstico (Act3).
void elevator_setAlarm(bool on) {
    if (on) {
        if (st != ElevatorState::ALARM) {
            enter(ElevatorState::ALARM);
        }
    } else {
        if (st == ElevatorState::ALARM) {
            // Al despejarse el fallo volvemos a IDLE; la cola pendiente
            // (si la hay) la reanuda elevator_tick() en el estado IDLE.
            floorTgt = floorCur;
            enter(ElevatorState::IDLE);
        }
    }
}

void elevator_tick() {
    uint32_t now = millis();

    switch (st) {
    case ElevatorState::IDLE:
        // Si hay algo en cola (puede haberlo si se encolaron peticiones
        // mientras estábamos en otro estado), atender.
        if (qCount > 0) serveNextOrIdle();
        break;

    case ElevatorState::MOVING:
        if (now - tStateEnter >= SERVO_TRAVEL_MS) {
            floorCur = floorTgt;
            enter(ElevatorState::DOOR_OPEN);
        }
        break;

    case ElevatorState::DOOR_OPEN:
        // Si el PIR detecta presencia mientras la puerta está abierta,
        // pasamos a OCCUPIED para retener la cabina.
        if (pirRead()) {
            enter(ElevatorState::OCCUPIED);
            break;
        }
        if (now - tStateEnter >= DOOR_OPEN_MS) {
            serveNextOrIdle();
        }
        break;

    case ElevatorState::OCCUPIED:
        // Cuando el PIR deja de detectar presencia y han pasado al menos
        // DOOR_OPEN_MS, salimos a la siguiente planta. Si llega una nueva
        // petición a otra planta, igualmente al cabo de DOOR_OPEN_MS sin
        // PIR cerramos puerta y servimos.
        if (!pirRead() && (now - tStateEnter >= DOOR_OPEN_MS)) {
            serveNextOrIdle();
        }
        break;

    case ElevatorState::ALARM:
        // Sin transición automática; en Act2 no la disparamos por defecto.
        break;
    }
}

int8_t        elevator_currentFloor() { return floorCur; }
int8_t        elevator_targetFloor()  { return floorTgt; }
ElevatorState elevator_state()        { return st; }
const char*   elevator_stateName()    { return NAMES[(int) st]; }
