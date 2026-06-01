// ============================================================================
//  diagnostics.h — Autodiagnóstico del sistema (Actividad 3).
//
//  Capa de instrumentación avanzada que se interpone entre la lectura cruda
//  de los sensores ambientales y el lazo de control. En cada tick:
//    1) Lee el DHT22 principal, el DHT22 de reserva, el LDR y el sensor de
//       corriente de los actuadores.
//    2) Valida cada lectura contra límites de rango físico plausible
//       (detección de cortocircuito / circuito abierto).
//    3) Conmuta automáticamente al sensor de temperatura de reserva si el
//       principal falla (redundancia).
//    4) Promedia las medidas (media móvil) para reducir el ruido.
//    5) Decide una severidad escalonada y, si procede, dispara la alarma.
//
//  Reacción escalonada:
//    OK      — todo correcto.
//    AVISO   — un sensor con reserva ha fallado; el sistema ha conmutado y
//              sigue operando (no suena alarma).
//    ALARMA  — fallo crítico sin reserva (ambos DHT, o el LDR, o sobreconsumo
//              de actuador): el ascensor entra en ALARM y suena el buzzer.
// ============================================================================

#pragma once
#include <stdint.h>

enum class DiagSeverity : uint8_t { OK, AVISO, ALARMA };

// Llamar UNA vez en setup().
void diag_setup();

// Tick periódico (llamar en cada loop). Auto-cadenciado; no bloquea.
void diag_tick();

// ---- Medidas validadas y promediadas (las consume el control) ------------
float diag_temp();          // °C  (NaN si no hay fuente de temperatura válida).
float diag_hum();           // %HR (NaN si no hay fuente válida).
float diag_lux();           // lux (promediados).
bool  diag_hasClimate();    // true si hay una fuente de T/H válida con datos.

// ---- Estado del autodiagnóstico ------------------------------------------
//  La temperatura y la humedad se diagnostican y conmutan de forma
//  INDEPENDIENTE: el sistema puede usar el sensor de reserva para una
//  magnitud y el principal para la otra.
bool         diag_tempSourceIsBackup();  // true → temperatura del DHT de reserva.
bool         diag_humSourceIsBackup();   // true → humedad del DHT de reserva.
DiagSeverity diag_severity();
const char*  diag_message();             // Texto corto del fallo activo ("" si OK).

// ---- Flags individuales para la telemetría CSV ---------------------------
bool  diag_tempPriOk();    // Salud de la TEMPERATURA del DHT principal.
bool  diag_tempBckOk();    // Salud de la TEMPERATURA del DHT de reserva.
bool  diag_humPriOk();     // Salud de la HUMEDAD del DHT principal.
bool  diag_humBckOk();     // Salud de la HUMEDAD del DHT de reserva.
bool  diag_ldrOk();
bool  diag_actuatorOk();
float diag_currentMa();

// ---- Lecturas CRUDAS del sensor (sin validar ni promediar) ----------------
//  Reflejan lo que el sensor entregó en el último ciclo, incluso si la
//  lectura está fuera de rango. A diferencia de diag_temp()/diag_lux(), que
//  se "congelan" en el último valor válido durante un fallo, estas getters
//  permiten ver en el CSV el valor que provocó la avería.
float diag_rawTemp();   // T cruda del DHT principal (NaN si dio NaN).
float diag_rawHum();    // H cruda del DHT principal (NaN si dio NaN).
float diag_rawLux();    // Última lectura cruda del LDR.
