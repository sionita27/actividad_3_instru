// ============================================================================
//  logger.cpp — Implementación del logger CSV unificado (Actividad 3).
//
//  El RTC DS1307 viaja por el bus I²C en 0x68 (el LCD está en 0x27 → no
//  chocan). Si el RTC no responde, getTimestamp() usa millis(): el sistema
//  no se rompe (fallo silencioso, criterio C3).
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <math.h>
#include "config.h"
#include "logger.h"
#include "classifier.h"
#include "elevator.h"
#include "control.h"
#include "actuators.h"
#include "sensors.h"

// ----------------------------------------------------------------------------
//  Estado interno del módulo
// ----------------------------------------------------------------------------
static RTC_DS1307 rtc;                 // Driver del chip RTC.
static bool       rtcOk = false;       // true si el chip respondió al begin().
static uint32_t   tLast = 0;           // Cadencia de la fila CSV.

// ============================================================================
//  initLogger()
// ============================================================================
void initLogger() {
    // Intentar inicializar el RTC. begin() devuelve false si no responde.
    rtcOk = rtc.begin();
    if (rtcOk) {
        if (!rtc.isrunning()) {
            // RTC sin programar → fijamos la fecha/hora de compilación.
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            Serial.println(F("[RTC] sin hora previa, fijada con la de compilacion"));
        }
        Serial.println(F("[RTC] OK"));
    } else {
        Serial.println(F("[RTC] no detectado, se usaran timestamps relativos"));
    }

    // Cabecera CSV unificada (clasificador + ascensor + clima + actuadores).
    Serial.println();
    Serial.println(F("# CSV ACME integrado (Act1+Act2)"));
    Serial.println(F("timestamp,piece_n,box_h_cm,box_class,floor_cur,floor_tgt,"
                     "elev_state,T_C,H_pct,Lux_lx,heater,cooler,lamp_pwm,"
                     "humidifier,dehumidifier,pir"));
    tLast = millis();
}

// ============================================================================
//  getTimestamp() — "YYYY-MM-DD HH:MM:SS" (RTC) o "T+000123s" (millis).
// ============================================================================
String getTimestamp() {
    char buf[24];
    if (rtcOk) {
        DateTime now = rtc.now();
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(),  now.day(),
                 now.hour(), now.minute(), now.second());
    } else {
        unsigned long s = millis() / 1000UL;
        snprintf(buf, sizeof(buf), "T+%06lus", s);
    }
    return String(buf);
}

// ============================================================================
//  logger_tick() — una fila CSV cada LOG_PERIOD_MS.
// ============================================================================
void logger_tick() {
    uint32_t now = millis();
    if (now - tLast < LOG_PERIOD_MS) return;
    tLast = now;

    String ts = getTimestamp();

    Serial.printf("%s,%d,%d,%s,%d,%d,%s,%.2f,%.2f,%.2f,%d,%d,%u,%d,%d,%d\r\n",
                  ts.c_str(),
                  classifier_lastPiece(),
                  classifier_lastHeightCm(),
                  classifier_lastClass(),                 // "" si aún ninguna.
                  (int) elevator_currentFloor(),
                  (int) elevator_targetFloor(),
                  elevator_stateName(),
                  ctrlTemp(), ctrlHum(), ctrlLux(),
                  heaterIsOn()       ? 1 : 0,
                  coolerIsOn()       ? 1 : 0,
                  (unsigned) lampPwm(),
                  humidifierIsOn()   ? 1 : 0,
                  dehumidifierIsOn() ? 1 : 0,
                  pirRead()          ? 1 : 0);
}
