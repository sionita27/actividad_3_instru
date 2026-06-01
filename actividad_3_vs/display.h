// ============================================================================
//  display.h — Interfaz pública del HMI (LCD 20x4 I²C).
//
//  Con un LCD de 4 filas las tres funciones del sistema integrado caben a la
//  vez, sin conmutar pantallas:
//    Línea 0:  última caja clasificada      "CAJA#07 ASCENSOR P3"
//    Línea 1:  estado del ascensor          "ELEV MOVING P0>P3"
//    Línea 2:  clima (T, H, iluminación)    "T23.0C H55% L401lx"
//    Línea 3:  contadores + actuadores + PIR "Asc07 Esc04 OK LP P0"
//
//  displayUpdate() consulta directamente los getters de classifier, elevator,
//  control y actuators; por eso no recibe parámetros.
// ============================================================================

#pragma once

void initDisplay();

// Refresca el LCD. Auto-rate-limited internamente (LCD_REFRESH_MS).
void displayUpdate();
