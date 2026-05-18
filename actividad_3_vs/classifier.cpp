// ============================================================================
//  classifier.cpp — FSM del clasificador de cajas + puente con el ascensor.
//
//  Portado de la lógica que en Actividad 1 vivía en el .ino. Aquí se
//  encapsula como módulo con prefijo classifier_ (estilo Act2), de modo que
//  el orquestador (.ino) quede fino.
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "classifier.h"
#include "sensors.h"
#include "actuators.h"
#include "elevator.h"        // Puente funcional Act1 → ascensor.

// ----------------------------------------------------------------------------
//  Estado de la FSM
// ----------------------------------------------------------------------------
static ClassifierState estado          = ClassifierState::IDLE;
static int             muestrasValidas = 0;     // Lecturas consecutivas bajo umbral.
static int             sumAlturas      = 0;     // Suma para el promedio al confirmar.
static uint32_t        tClasifInicio   = 0;     // millis() al entrar en CLASSIFY.
static uint32_t        tWaitClearInicio= 0;     // millis() al entrar en WAIT_CLEAR.
static uint32_t        tLastDistRead   = 0;     // Cadencia del HC-SR04.

// ----------------------------------------------------------------------------
//  Contadores y datos de la última caja (los leen display y logger).
// ----------------------------------------------------------------------------
static int         pieceCounter = 0;
static int         totalAsc     = 0;
static int         totalEsc     = 0;
static int         lastHeight   = 0;
static const char* lastClass    = "";
static int8_t      lastFloor    = -1;

// ----------------------------------------------------------------------------
//  Estado del botón de reset (debounce por software).
// ----------------------------------------------------------------------------
static bool     botonAnt = HIGH;
static uint32_t tBoton   = 0;

static const char* NAMES[] = { "IDLE", "MEASURING", "CLASSIFY", "WAIT_CLEAR" };

// ============================================================================
//  onNewPiece() — se llama UNA vez por caja confirmada.
// ----------------------------------------------------------------------------
//  Clasifica por altura, actualiza contadores, dispara compuerta + LED +
//  buzzer y, si la caja es ALTA, ejecuta el PUENTE hacia el ascensor:
//  mapea la altura a una planta destino y llama a elevator_request().
// ============================================================================
static void onNewPiece(int alturaCm) {
    // Menor distancia medida ⇒ caja más alta.
    bool esAlta = alturaCm < UMBRAL_ALTA_CM;

    pieceCounter++;
    lastHeight = alturaCm;

    if (esAlta) {
        totalAsc++;
        lastClass = "ASCENSOR";

        // --- Puente Act1 → ascensor --------------------------------------
        // Caja más alta (menor cm) → planta más alta.
        int planta = map(alturaCm, PUENTE_ALTURA_MIN_CM, PUENTE_ALTURA_MAX_CM,
                         NUM_PLANTAS - 1, 0);
        planta = constrain(planta, 0, NUM_PLANTAS - 1);
        lastFloor = (int8_t) planta;

        servoAscensor();
        setLedAscensor(true);
        Serial.printf("[CLF] caja #%d ALTA (%d cm) -> ASCENSOR -> planta %d\r\n",
                      pieceCounter, alturaCm, planta);
        elevator_request((int8_t) planta);
    } else {
        totalEsc++;
        lastClass = "ESCALERA";
        lastFloor = -1;

        servoEscalera();
        setLedEscalera(true);
        Serial.printf("[CLF] caja #%d BAJA (%d cm) -> ESCALERA\r\n",
                      pieceCounter, alturaCm);
    }

    beep();
}

// ============================================================================
//  checkResetButton() — botón de reset de contadores con debounce.
// ============================================================================
static void checkResetButton() {
    bool     ahora = digitalRead(PIN_BOTON_RESET);   // HIGH suelto, LOW pulsado.
    uint32_t now   = millis();

    if (ahora != botonAnt && (now - tBoton) > DEBOUNCE_MS) {
        tBoton = now;
        if (ahora == LOW) {              // Flanco de bajada → pulsación válida.
            pieceCounter = 0;
            totalAsc     = 0;
            totalEsc     = 0;
            lastHeight   = 0;
            lastClass    = "";
            lastFloor    = -1;
            Serial.println(F("[CLF] contadores reseteados"));
        }
        botonAnt = ahora;
    }
}

// ============================================================================
//  classifier_setup()
// ============================================================================
void classifier_setup() {
    pinMode(PIN_BOTON_RESET, INPUT_PULLUP);
    estado        = ClassifierState::IDLE;
    tLastDistRead = millis();
}

// ============================================================================
//  classifier_tick() — botón de reset + FSM del HC-SR04 (cada 500 ms).
// ============================================================================
void classifier_tick() {
    checkResetButton();

    uint32_t now = millis();
    if (now - tLastDistRead < CLASIFICADOR_PERIODO_MS) return;
    tLastDistRead = now;

    int cm = readHeightCm();   // cm o READ_DISTANCE_ERROR (-1).
    // Para la FSM, un timeout se trata como "muy lejos" (no hay caja).
    int cmFsm = (cm == READ_DISTANCE_ERROR) ? 999 : cm;

    switch (estado) {

    case ClassifierState::IDLE:
        // Algo entró en zona de detección → empezar a contar muestras.
        if (cmFsm < UMBRAL_NADA_CM) {
            estado          = ClassifierState::MEASURING;
            muestrasValidas = 1;
            sumAlturas      = cmFsm;
        }
        break;

    case ClassifierState::MEASURING:
        if (cmFsm < UMBRAL_NADA_CM) {
            muestrasValidas++;
            sumAlturas += cmFsm;
            if (muestrasValidas >= MUESTRAS_VALIDACION) {
                // Caja confirmada: clasificamos con la altura promedio.
                int alturaProm = sumAlturas / muestrasValidas;
                onNewPiece(alturaProm);
                tClasifInicio = now;
                estado        = ClassifierState::CLASSIFY;
            }
        } else {
            // Falsa alarma → volver a IDLE.
            estado          = ClassifierState::IDLE;
            muestrasValidas = 0;
            sumAlturas      = 0;
        }
        break;

    case ClassifierState::CLASSIFY:
        // Mantener actuadores/cartel CLASIFICACION_DUR_MS; luego WAIT_CLEAR.
        if (now - tClasifInicio >= CLASIFICACION_DUR_MS) {
            clearLeds();
            servoNeutral();
            estado           = ClassifierState::WAIT_CLEAR;
            tWaitClearInicio = now;
        }
        break;

    case ClassifierState::WAIT_CLEAR:
        // Salida normal: la caja libera el sensor.
        if (cmFsm >= UMBRAL_NADA_CM) {
            estado = ClassifierState::IDLE;
        }
        // Salida por timeout: el sensor sigue viendo "algo" → asumimos que
        // ya es la siguiente caja y volvemos a IDLE para no bloquear la FSM.
        else if (now - tWaitClearInicio >= WAIT_CLEAR_TIMEOUT_MS) {
            estado = ClassifierState::IDLE;
        }
        break;
    }
}

// ============================================================================
//  Getters
// ============================================================================
ClassifierState classifier_state()       { return estado; }
const char*     classifier_stateName()   { return NAMES[(int) estado]; }
int             classifier_lastPiece()   { return pieceCounter; }
int             classifier_lastHeightCm(){ return lastHeight; }
const char*     classifier_lastClass()   { return lastClass; }
int8_t          classifier_lastFloor()   { return lastFloor; }
int             classifier_totalAsc()    { return totalAsc; }
int             classifier_totalEsc()    { return totalEsc; }
