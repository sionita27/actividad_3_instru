// ============================================================================
//  control.h — Algoritmos de control ON-OFF con histéresis + modelo simulado.
//
//  Tres lazos independientes (temperatura, humedad, iluminación). Cada uno
//  recuerda su estado entre llamadas y aplica HISTÉRESIS:
//      - Enciende el actuador "positivo" (calentador / humidif. / lámpara)
//        al cruzar el borde inferior  SP - banda.
//      - Enciende el "negativo" (enfriador / deshumidif.) al cruzar SP + banda.
//      - Apaga ambos solo cuando la medida vuelve al setpoint (no al borde),
//        evitando el chattering de la versión sin memoria.
//
//  Variables controladas:
//      - Temperatura (DHT22, °C)    → SETPOINT_TEMP_C ± BANDA_TEMP_C
//      - Humedad (DHT22, %)         → SETPOINT_HUM_PCT ± BANDA_HUM_PCT
//      - Iluminación (LDR, lx)      → SETPOINT_LUX_LX ± BANDA_LUX_LX
//
//  Con MODO_SIM_TERMICO=1 los actuadores afectan a un offset numérico que
//  se suma a la lectura cruda antes del control. Así el lazo cerrado se
//  observa en Wokwi aunque los actuadores físicos no influyan en el sensor.
// ============================================================================

#pragma once
#include <stdint.h>

void control_setup();

// rawT en °C, rawH en % HR, rawLux en LUX REALES (no %). Llamar
// periódicamente con el tick del sensor.
void control_tick(float rawT, float rawH, float rawLux);

// Función pura conservada por compatibilidad / pruebas: -1, 0 o +1.
int8_t controlOnOffDeadband(float meas, float sp, float band);

// Lecturas con el offset del modelo aplicado (lo que ve el control y se
// imprime en el LCD).
float ctrlTemp();
float ctrlHum();
float ctrlLux();
