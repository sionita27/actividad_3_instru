// ============================================================================
//  logger.h — Telemetría CSV unificada por el puerto serie (Actividad 3).
//
//  Una sola tabla CSV reúne el estado completo del sistema integrado:
//  clasificador (última caja) + ascensor (planta y estado FSM) + clima
//  (T, H, lux) + actuadores + PIR. Se emite una fila cada LOG_PERIOD_MS.
//
//  El timestamp procede del RTC DS1307 si está presente; si no, se usa
//  millis() como tiempo relativo ("T+000123s"). Fallo silencioso.
// ============================================================================

#pragma once
#include <Arduino.h>

// Inicializa el RTC (vía I²C) e imprime la cabecera CSV. Llamar UNA vez en
// setup(), DESPUÉS de Wire.begin().
void initLogger();

// Devuelve el timestamp: "YYYY-MM-DD HH:MM:SS" (RTC) o "T+000123s" (millis).
String getTimestamp();

// Imprime una fila CSV si ha pasado LOG_PERIOD_MS. Consulta directamente los
// getters de classifier, elevator, control y actuators.
void logger_tick();
