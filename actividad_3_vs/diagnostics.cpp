// ============================================================================
//  diagnostics.cpp — Implementación del autodiagnóstico (Actividad 3).
//
//  La temperatura y la humedad se diagnostican y conmutan de forma
//  INDEPENDIENTE: el DHT22 tiene un elemento de medida para cada magnitud, y
//  uno puede averiarse sin el otro. Por eso el sistema puede estar usando el
//  sensor de reserva para la temperatura y el principal para la humedad, o
//  viceversa.
// ============================================================================

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "diagnostics.h"
#include "sensors.h"
#include "actuators.h"
#include "elevator.h"

// ----------------------------------------------------------------------------
//  Media móvil — promedia las últimas DIAG_AVG_WINDOW muestras válidas.
// ----------------------------------------------------------------------------
struct MovingAvg {
    float   buf[DIAG_AVG_WINDOW];
    uint8_t count;
    uint8_t idx;
};

static void avgReset(MovingAvg &m) {
    m.count = 0;
    m.idx   = 0;
}
static void avgPush(MovingAvg &m, float v) {
    m.buf[m.idx] = v;
    m.idx = (m.idx + 1) % DIAG_AVG_WINDOW;
    if (m.count < DIAG_AVG_WINDOW) m.count++;
}
static float avgGet(const MovingAvg &m) {
    if (m.count == 0) return NAN;
    float s = 0.0f;
    for (uint8_t i = 0; i < m.count; ++i) s += m.buf[i];
    return s / m.count;
}

// ----------------------------------------------------------------------------
//  Monitor de salud — confirma fallo/recuperación con anti-rebote por número
//  de muestras consecutivas (DIAG_FAULT_SAMPLES).
// ----------------------------------------------------------------------------
struct HealthMon {
    uint8_t badCount;
    uint8_t goodCount;
    bool    ok;
};

static void healthReset(HealthMon &h) {
    h.badCount = 0;
    h.goodCount = 0;
    h.ok = true;
}
static void healthUpdate(HealthMon &h, bool readingOk) {
    if (readingOk) {
        h.badCount = 0;
        if (h.goodCount < 255) h.goodCount++;
        if (h.goodCount >= DIAG_FAULT_SAMPLES) h.ok = true;
    } else {
        h.goodCount = 0;
        if (h.badCount < 255) h.badCount++;
        if (h.badCount >= DIAG_FAULT_SAMPLES) h.ok = false;
    }
}

static inline bool inRange(float v, float lo, float hi) {
    return !isnan(v) && v >= lo && v <= hi;
}

// ----------------------------------------------------------------------------
//  Estado interno del módulo
// ----------------------------------------------------------------------------
static MovingAvg tAvg, hAvg, luxAvg, currentAvg;

// Salud INDEPENDIENTE de la temperatura y de la humedad de cada DHT.
static HealthMon monPriT, monBckT;   // temperatura: principal / reserva.
static HealthMon monPriH, monBckH;   // humedad:     principal / reserva.
static HealthMon monLdr, monAct;

static bool      usingBackupT = false;   // Fuente activa de temperatura.
static bool      usingBackupH = false;   // Fuente activa de humedad.
static float     currentMa    = 0.0f;    // Corriente promediada (mA).
static DiagSeverity severity  = DiagSeverity::OK;
static char      message[24]  = "";

// Últimas lecturas CRUDAS del DHT principal y del LDR (sin validar ni
// promediar) — para que la telemetría pueda mostrar el valor que falló.
static float     rawTpri = NAN;
static float     rawHpri = NAN;
static float     rawLux  = 0.0f;

static uint32_t tLastDht    = 0;
static uint32_t tLastSample = 0;
static uint32_t tLastBuzzer = 0;

// ============================================================================
//  diag_setup()
// ============================================================================
void diag_setup() {
    avgReset(tAvg);
    avgReset(hAvg);
    avgReset(luxAvg);
    avgReset(currentAvg);
    healthReset(monPriT);
    healthReset(monBckT);
    healthReset(monPriH);
    healthReset(monBckH);
    healthReset(monLdr);
    healthReset(monAct);
    usingBackupT = false;
    usingBackupH = false;
    currentMa    = 0.0f;
    severity     = DiagSeverity::OK;
    message[0]   = '\0';
    rawTpri      = NAN;
    rawHpri      = NAN;
    rawLux       = 0.0f;

    uint32_t now = millis();
    tLastDht = tLastSample = tLastBuzzer = now;
}

// ============================================================================
//  diag_tick() — lectura, validación, conmutación, promediado y alarma.
// ============================================================================
void diag_tick() {
    uint32_t now = millis();

    // ----------------------------------------------------------------------
    //  Ciclo del DHT22 (cada 2 s): leer principal y reserva, validar T y H
    //  por separado, actualizar la salud y decidir cada fuente.
    // ----------------------------------------------------------------------
    if (now - tLastDht >= DHT22_PERIODO_MS) {
        tLastDht = now;

        float tA, hA;
        bool readA = readEnvironment(tA, hA);
        // Lectura cruda del sensor principal (NaN si falló del todo).
        rawTpri = readA ? tA : NAN;
        rawHpri = readA ? hA : NAN;

        float tB, hB;
        bool readB = readEnvironmentB(tB, hB);

        // Validación INDEPENDIENTE de temperatura y humedad de cada sensor.
        // Un NaN del DHT invalida ambas magnitudes de ese sensor.
        bool tA_ok = readA && inRange(tA, DIAG_TEMP_MIN_C, DIAG_TEMP_MAX_C);
        bool hA_ok = readA && inRange(hA, DIAG_HUM_MIN_PCT, DIAG_HUM_MAX_PCT);
        bool tB_ok = readB && inRange(tB, DIAG_TEMP_MIN_C, DIAG_TEMP_MAX_C);
        bool hB_ok = readB && inRange(hB, DIAG_HUM_MIN_PCT, DIAG_HUM_MAX_PCT);

        healthUpdate(monPriT, tA_ok);
        healthUpdate(monBckT, tB_ok);
        healthUpdate(monPriH, hA_ok);
        healthUpdate(monBckH, hB_ok);

        // --- Conmutación de la fuente de TEMPERATURA --------------------
        bool wantBckT = usingBackupT;
        if (monPriT.ok)      wantBckT = false;
        else if (monBckT.ok) wantBckT = true;
        if (wantBckT != usingBackupT) {
            usingBackupT = wantBckT;
            avgReset(tAvg);
            Serial.printf("[DIAG] fuente de TEMPERATURA -> %s\r\n",
                          usingBackupT ? "RESERVA" : "PRINCIPAL");
        }

        // --- Conmutación de la fuente de HUMEDAD ------------------------
        bool wantBckH = usingBackupH;
        if (monPriH.ok)      wantBckH = false;
        else if (monBckH.ok) wantBckH = true;
        if (wantBckH != usingBackupH) {
            usingBackupH = wantBckH;
            avgReset(hAvg);
            Serial.printf("[DIAG] fuente de HUMEDAD -> %s\r\n",
                          usingBackupH ? "RESERVA" : "PRINCIPAL");
        }

        // --- Empujar a la media móvil la lectura válida de cada fuente --
        if (!usingBackupT && tA_ok)      avgPush(tAvg, tA);
        else if (usingBackupT && tB_ok)  avgPush(tAvg, tB);

        if (!usingBackupH && hA_ok)      avgPush(hAvg, hA);
        else if (usingBackupH && hB_ok)  avgPush(hAvg, hB);
    }

    // ----------------------------------------------------------------------
    //  Ciclo de muestreo rápido (cada DIAG_SAMPLE_MS): LDR y corriente.
    // ----------------------------------------------------------------------
    if (now - tLastSample >= DIAG_SAMPLE_MS) {
        tLastSample = now;

        float lux   = readLux();
        rawLux      = lux;                      // Lectura cruda para el CSV.
        bool  luxOk = inRange(lux, DIAG_LUX_MIN_LX, DIAG_LUX_MAX_LX);
        healthUpdate(monLdr, luxOk);
        if (luxOk) avgPush(luxAvg, lux);

        // Sensor de corriente de actuadores. Se promedia (igual que el resto
        // de medidas) para que el ruido del ADC no dispare falsas alarmas;
        // hay sobreconsumo si la corriente promediada supera el umbral.
        avgPush(currentAvg, readActuatorCurrent());
        float cAvg = avgGet(currentAvg);
        currentMa  = isnan(cAvg) ? 0.0f : cAvg;
        bool overcurrent = currentMa > DIAG_CURRENT_MAX_MA;
        healthUpdate(monAct, !overcurrent);
    }

    // ----------------------------------------------------------------------
    //  Severidad escalonada.
    //   - Fallo crítico (sin reserva): los dos sensores de T, o los dos de
    //     H, o el LDR, o sobreconsumo del actuador.
    //   - Aviso: una magnitud (T o H) perdió su sensor principal pero opera
    //     con el de reserva.
    // ----------------------------------------------------------------------
    bool bothTempFail = !monPriT.ok && !monBckT.ok;
    bool bothHumFail  = !monPriH.ok && !monBckH.ok;

    if (bothTempFail || bothHumFail || !monLdr.ok || !monAct.ok) {
        severity = DiagSeverity::ALARMA;
    } else if (usingBackupT || usingBackupH) {
        severity = DiagSeverity::AVISO;
    } else {
        severity = DiagSeverity::OK;
    }

    // Mensaje del fallo activo (prioridad: alarmas antes que avisos).
    if (bothTempFail)                       strcpy(message, "!ALARMA 2x SENSOR T");
    else if (bothHumFail)                   strcpy(message, "!ALARMA 2x SENSOR H");
    else if (!monLdr.ok)                    strcpy(message, "!ALARMA SENSOR LUZ");
    else if (!monAct.ok)                    strcpy(message, "!ALARMA SOBRECONSUMO");
    else if (usingBackupT && usingBackupH)  strcpy(message, "AVISO T+H->RESERVA");
    else if (usingBackupT)                  strcpy(message, "AVISO T->RESERVA");
    else if (usingBackupH)                  strcpy(message, "AVISO H->RESERVA");
    else                                    message[0] = '\0';

    // ----------------------------------------------------------------------
    //  Reacción: el ascensor entra/sale de ALARM; buzzer periódico si alarma.
    // ----------------------------------------------------------------------
    elevator_setAlarm(severity == DiagSeverity::ALARMA);

    if (severity == DiagSeverity::ALARMA) {
        if (now - tLastBuzzer >= DIAG_BUZZER_PERIOD_MS) {
            tLastBuzzer = now;
            beep();
        }
    }
}

// ============================================================================
//  Getters
// ============================================================================
float diag_temp() { return avgGet(tAvg); }
float diag_hum()  { return avgGet(hAvg); }
float diag_lux()  {
    float v = avgGet(luxAvg);
    return isnan(v) ? 0.0f : v;
}
bool  diag_hasClimate() {
    bool tempOk = (monPriT.ok || monBckT.ok) && tAvg.count > 0;
    bool humOk  = (monPriH.ok || monBckH.ok) && hAvg.count > 0;
    return tempOk && humOk;
}

bool         diag_tempSourceIsBackup() { return usingBackupT; }
bool         diag_humSourceIsBackup()  { return usingBackupH; }
DiagSeverity diag_severity()           { return severity; }
const char*  diag_message()            { return message; }

bool  diag_tempPriOk()  { return monPriT.ok; }
bool  diag_tempBckOk()  { return monBckT.ok; }
bool  diag_humPriOk()   { return monPriH.ok; }
bool  diag_humBckOk()   { return monBckH.ok; }
bool  diag_ldrOk()      { return monLdr.ok; }
bool  diag_actuatorOk() { return monAct.ok; }
float diag_currentMa()  { return currentMa; }

float diag_rawTemp() { return rawTpri; }
float diag_rawHum()  { return rawHpri; }
float diag_rawLux()  { return rawLux; }
