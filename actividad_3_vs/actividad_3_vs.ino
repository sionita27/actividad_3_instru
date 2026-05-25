// ============================================================================
//  actividad_3_vs.ino — Orquestador del sistema ACME integrado (Actividad 3).
//
//  Fusiona en un único ESP32 los dos sistemas previos:
//    · Act1 — clasificador de cajas por altura (módulo classifier).
//    · Act2 — ascensor inteligente de 5 plantas + control ambiental.
//  y añade la mejora de instrumentación avanzada de la Actividad 3:
//    · Act3 — autodiagnóstico (módulo diagnostics): redundancia de sensores,
//             detección de fallos por rango, promediado y alarma escalonada.
//
//  Integración funcional: cuando el clasificador confirma una caja alta,
//  el módulo classifier mapea su altura a una planta y dispara
//  elevator_request() — la caja "sube" por el ascensor.
//
//  Bucle no bloqueante (millis(), nunca delay()). Cada iteración:
//    1) Sondea IR + botones de planta → peticiones al ascensor.
//    2) Tick del clasificador (HC-SR04 + FSM + puente al ascensor).
//    3) Tick de la FSM del ascensor (cabina + puerta + cola FIFO).
//    4) Tick del autodiagnóstico: lee y valida sensores, conmuta a la
//       reserva si hace falta, promedia y decide la alarma.
//    5) Tick del control ambiental con las medidas validadas.
//    6) Refresca HMI (LCD 20x4) y logger CSV.
// ============================================================================

#include <Wire.h>
#include "config.h"
#include "sensors.h"
#include "ircontrol.h"
#include "actuators.h"
#include "classifier.h"
#include "elevator.h"
#include "diagnostics.h"
#include "control.h"
#include "display.h"
#include "logger.h"

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
    diag_setup();
    initLogger();

    Serial.println();
    Serial.println(F("[Act3] boot OK — sistema integrado Act1+Act2+autodiagnostico"));
}

void loop() {
    uint32_t now = millis();

    // ------------------------------------------------------------------
    //  Peticiones al ascensor: mando IR + 4 botones físicos de planta.
    //  (La planta 4 no tiene botón físico; se llama con el mando IR.)
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
    //  Autodiagnóstico: lee DHT principal/reserva, LDR y sensor de
    //  corriente; valida, conmuta de fuente, promedia y gestiona la alarma.
    // ------------------------------------------------------------------
    diag_tick();

    // ------------------------------------------------------------------
    //  Control ambiental con las medidas YA validadas y promediadas por el
    //  autodiagnóstico. Si no hay una fuente de temperatura válida (ambos
    //  DHT en fallo), se omite el control.
    // ------------------------------------------------------------------
    if (diag_hasClimate()) {
        control_tick(diag_temp(), diag_hum(), diag_lux());
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
