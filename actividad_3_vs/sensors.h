// ============================================================================
//  sensors.h — Interfaz pública del módulo de sensores (Actividad 3).
//
//  Aglutina TODOS los sensores del sistema integrado:
//    - HC-SR04 (altura de cajas)              — clasificador (Act1).
//    - DHT22 (temperatura, humedad)           — control ambiental.
//    - LDR (iluminación, en lux reales)       — control ambiental.
//    - PIR HC-SR501 (presencia en cabina)     — ascensor (Act2).
//    - 5 botones físicos de planta            — ascensor (Act2).
//
//  El .ino y los módulos solo hacen #include "sensors.h"; nada de hardware
//  se filtra fuera del .cpp.
// ============================================================================

#pragma once

#include <stdint.h>

// Valor de retorno de readHeightCm() cuando la lectura no es válida
// (timeout del eco o distancia fuera del rango útil).
#define READ_DISTANCE_ERROR  (-1)

// Inicializa todos los sensores. Llamar UNA vez en setup().
void initSensors();

// HC-SR04: dispara un pulso y devuelve la distancia en cm, o
// READ_DISTANCE_ERROR (-1) si no hay eco válido.
int readHeightCm();

// DHT22: lee temperatura (°C) y humedad (%) por referencia.
// Devuelve true si la lectura es válida; false si NaN (no toca t/h).
bool readEnvironment(float &t, float &h);

// LDR: iluminación en lux reales (fórmula de Corona 2014).
float readLux();

// PIR HC-SR501: true si detecta presencia.
bool pirRead();

// Sondea los 5 botones de planta con debounce. Devuelve la planta pulsada
// (0..NUM_PLANTAS-1) o -1 si ninguna. Un evento por pulsación.
int8_t buttonsPoll();
