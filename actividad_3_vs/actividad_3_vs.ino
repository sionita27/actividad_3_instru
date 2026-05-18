// ============================================================================
//  actividad_3_vs.ino — Orquestador del sistema ACME integrado (Actividad 3).
//
//  Fusiona en un único ESP32 los dos sistemas previos:
//    · Act1 — clasificador de cajas por altura (módulo classifier).
//    · Act2 — ascensor inteligente de 5 plantas + control ambiental.
//
//  Integración funcional: cuando el clasificador confirma una caja ALTA,
//  el módulo classifier mapea su altura a una planta y llama internamente
//  a elevator_request() — la caja "sube" por el ascensor.
//
//  Bucle no bloqueante (millis(), nunca delay()). Cada iteración:
//    1) Sondea IR + botones de planta → peticiones al ascensor.
//    2) Tick del clasificador (HC-SR04 + FSM + puente al ascensor).
//    3) Tick de la FSM del ascensor (cabina + puerta + cola FIFO).
//    4) Lee LDR (rápido) y, cada 2 s, el DHT22.
//    5) Tick del control ambiental (modelo + lazos ON-OFF con histéresis).
//    6) Refresca HMI (LCD 20x4) y logger CSV.
// ============================================================================

#include <math.h>            // NAN, isnan().
#include <Wire.h>
#include "config.h"
#include "sensors.h"
#include "ircontrol.h"
#include "actuators.h"
#include "classifier.h"
#include "elevator.h"
#include "control.h"
#include "display.h"
#include "logger.h"

// ----------------------------------------------------------------------------
//  Cache de las últimas lecturas ambientales válidas.
//  lastT/lastH arrancan en NaN para que control_tick() NO se ejecute hasta
//  que el DHT22 devuelva al menos UNA muestra buena.
// ----------------------------------------------------------------------------
static float    lastT    = NAN;
static float    lastH    = NAN;
static float    lastLux  = 0.0f;        // El LDR no falla, arranca a 0 lx.
static uint32_t tLastEnv = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(50);

    Wire.begin();               // Bus I²C: LCD (0x27) + RTC DS1307 (0x68).
    initDisplay();
    initSensors();
    ircontrol_setup();
    initActuators();
    classifier_setup();
    elevator_setup();
    control_setup();
    initLogger();

    Serial.println();
    Serial.println(F("[Act3] boot OK — sistema integrado Act1+Act2"));
}

void loop() {
    uint32_t now = millis();

    // ------------------------------------------------------------------
    //  Peticiones al ascensor: mando IR + 5 botones físicos de planta.
    // ------------------------------------------------------------------
    int8_t plantaIR = ircontrol_poll();
    if (plantaIR >= 0) {
        elevator_request(plantaIR);
    }
    int8_t plantaBtn = buttonsPoll();
    if (plantaBtn >= 0) {
        elevator_request(plantaBtn);
    }

    // ------------------------------------------------------------------
    //  Clasificador de cajas (HC-SR04). Si confirma una caja ALTA,
    //  internamente llama a elevator_request() con la planta destino.
    // ------------------------------------------------------------------
    classifier_tick();

    // FSM del ascensor (no bloqueante, gestiona puertas y cola FIFO).
    elevator_tick();

    // ------------------------------------------------------------------
    //  Sensores ambientales: el LDR se lee en cada iteración; el DHT22
    //  está limitado a una lectura cada 2 s (hardware).
    // ------------------------------------------------------------------
    lastLux = readLux();

    if (now - tLastEnv >= DHT22_PERIODO_MS) {
        tLastEnv = now;
        float t, h;
        if (readEnvironment(t, h)) {
            lastT = t;
            lastH = h;
        }
    }

    // Tick del control en cada iteración (se auto-limita internamente).
    // Si aún no hay lectura buena del DHT22, saltamos el control.
    if (!isnan(lastT) && !isnan(lastH)) {
        control_tick(lastT, lastH, lastLux);
    }

    // ------------------------------------------------------------------
    //  HMI (LCD 20x4) + logger CSV — ambos auto-rate-limited.
    // ------------------------------------------------------------------
    displayUpdate();
    logger_tick();

    // PIR — log de flancos (la FSM del ascensor ya lee pirRead() directamente).
    {
        static bool pirPrev = false;
        bool pirNow = pirRead();
        if (pirNow != pirPrev) {
            pirPrev = pirNow;
            Serial.printf("[%lu] PIR=%d\r\n", now, pirNow ? 1 : 0);
        }
    }
}
