// ============================================================================
//  control.cpp — Lazos de control y modelo térmico simulado.
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "control.h"
#include "actuators.h"

// ----------------------------------------------------------------------------
//  Estado del modelo simulado (offsets acumulados desde el arranque)
// ----------------------------------------------------------------------------
static float    tempOffset = 0.0f;
static float    humOffset  = 0.0f;
static float    luxOffset  = 0.0f;
static uint32_t simLastTick = 0;

// Últimas lecturas con offset aplicado (las exponemos al .ino para el LCD).
static float    lastT   = 0.0f;
static float    lastH   = 0.0f;
static float    lastLux = 0.0f;

// Estado memorizado de cada lazo con histéresis (definidos arriba para que
// control_setup() pueda resetearlos; el algoritmo de actualización está
// más abajo en hystUpdate()).
static int8_t stateTemp = 0;
static int8_t stateHum  = 0;
static int8_t stateLux  = 0;

void control_setup() {
    tempOffset  = 0.0f;
    humOffset   = 0.0f;
    luxOffset   = 0.0f;
    simLastTick = millis();
    stateTemp   = 0;
    stateHum    = 0;
    stateLux    = 0;
}

// ============================================================================
//  Función pura: ON-OFF de tres posiciones con zona muerta.
//      err > +band → -1
//      err < -band → +1
//      |err| ≤ band → 0
// ============================================================================
int8_t controlOnOffDeadband(float meas, float sp, float band) {
    float err = meas - sp;
    if (err >  band) return -1;
    if (err < -band) return +1;
    return 0;
}

// ============================================================================
//  Modelo simulado: cada SIM_PERIODO_MS,
//      · Si el actuador "positivo" está ON → offset += SIM_DELTA.
//      · Si el "negativo"  está ON           → offset -= SIM_DELTA.
//      · Si NINGUNO está ON → el offset DECAE hacia 0 (regresión a ambiente),
//        SIN cruzar 0 (clamp). Esto evita que la lámpara/calentador parpadee
//        cuando la lectura cruda ya está dentro de la banda muerta.
// ============================================================================
static inline void leakToward0(float &offset, float fuga) {
    if (offset > 0.0f) {
        offset -= fuga;
        if (offset < 0.0f) offset = 0.0f;
    } else if (offset < 0.0f) {
        offset += fuga;
        if (offset > 0.0f) offset = 0.0f;
    }
}

static void simModelTick() {
    uint32_t now = millis();
    if (now - simLastTick < SIM_PERIODO_MS) return;
    simLastTick = now;

    if      (heaterIsOn()) tempOffset += SIM_DELTA_TEMP;
    else if (coolerIsOn()) tempOffset -= SIM_DELTA_TEMP;
    else                   leakToward0(tempOffset, SIM_FUGA_TEMP);

    if      (humidifierIsOn())   humOffset += SIM_DELTA_HUM;
    else if (dehumidifierIsOn()) humOffset -= SIM_DELTA_HUM;
    else                         leakToward0(humOffset, SIM_FUGA_HUM);

    if (lampIsOn()) luxOffset += SIM_DELTA_LUX;
    else            leakToward0(luxOffset, SIM_FUGA_LUX);
}

// ============================================================================
//  Lazos con histéresis real (ON-OFF con zona muerta + memoria de estado).
//
//  Idea: el actuador se enciende al cruzar el borde EXTERIOR de la banda
//  muerta y solo se apaga cuando la medida alcanza el setpoint (mitad de
//  la banda). Esto evita el "chattering" que se daba con la versión sin
//  memoria, en la que el LED se apagaba justo en el borde y volvía a
//  encender al instante por la siguiente lectura.
//
//  Estados internos (memorizados entre llamadas):
//      +1  → actuador "positivo" activo (calentador / humidif. / lámpara)
//      -1  → actuador "negativo" activo (enfriador / deshumidif.)
//       0  → ambos apagados (dentro de banda muerta tras llegar al SP)
// ============================================================================
static int8_t hystUpdate(int8_t prev, float meas, float sp, float band) {
    // Disparo: cruzo el borde exterior → encender el actuador correspondiente.
    if (meas < sp - band) return +1;
    if (meas > sp + band) return -1;
    // Zona muerta: si venía calentando y ya alcancé el setpoint → apagar.
    if (prev > 0 && meas >= sp) return 0;
    // Zona muerta: si venía enfriando y ya alcancé el setpoint → apagar.
    if (prev < 0 && meas <= sp) return 0;
    // Resto de la banda muerta: mantengo el estado anterior (histéresis).
    return prev;
}

static void loopTemp(float t) {
    stateTemp = hystUpdate(stateTemp, t, SETPOINT_TEMP_C, BANDA_TEMP_C);
    heaterSet(stateTemp > 0);
    coolerSet(stateTemp < 0);
}

static void loopHum(float h) {
    stateHum = hystUpdate(stateHum, h, SETPOINT_HUM_PCT, BANDA_HUM_PCT);
    humidifierSet(stateHum > 0);
    dehumidifierSet(stateHum < 0);
}

static void loopLux(float lux) {
    stateLux = hystUpdate(stateLux, lux, SETPOINT_LUX_LX, BANDA_LUX_LX);
    if      (stateLux > 0) lampSetPwm(255);
    else if (stateLux < 0) lampSetPwm(0);
    // stateLux == 0 → conservar el duty actual (sin cambios bruscos).
}

// ============================================================================
//  control_tick() — entrada principal. Recibe lecturas crudas, aplica
//  el offset del modelo (si MODO_SIM_TERMICO=1) y ejecuta los 3 lazos.
// ============================================================================
void control_tick(float rawT, float rawH, float rawLux) {
    simModelTick();

#if MODO_SIM_TERMICO
    lastT   = rawT   + tempOffset;
    lastH   = rawH   + humOffset;
    lastLux = rawLux + luxOffset;
#else
    lastT   = rawT;
    lastH   = rawH;
    lastLux = rawLux;
#endif

    loopTemp(lastT);
    loopHum(lastH);
    loopLux(lastLux);
}

float ctrlTemp() { return lastT;   }
float ctrlHum()  { return lastH;   }
float ctrlLux()  { return lastLux; }
