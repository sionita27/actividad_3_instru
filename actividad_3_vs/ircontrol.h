// ============================================================================
//  ircontrol.h — Decodificación del mando a distancia IR del ascensor.
//
//  Usa la librería IRremoteESP8266 (compatible con ESP32). El mando virtual
//  de Wokwi (wokwi-ir-remote) emite tramas NEC de 32 bits; aquí mapeamos los
//  botones 0..4 del mando a las 5 plantas del ascensor.
// ============================================================================

#pragma once
#include <stdint.h>

// Inicializa el receptor IR en PIN_IR_RX. Llamar una vez en setup().
void ircontrol_setup();

// Sondea el receptor: si llega una trama válida la decodifica y devuelve la
// planta solicitada (0..NUM_PLANTAS-1). Si no hay trama o el código no
// está en el mapeo conocido, devuelve -1.
//
// Llamar de forma frecuente (cada loop), no bloquea.
int8_t ircontrol_poll();

// Pausa / reanuda el timer interno del receptor IR. La librería IRremote v4
// instala un timer hardware que muestrea el pin a 50 µs. Eso choca con el
// bit-banging del DHT22 (pulsos de 26-70 µs). Antes de leer el DHT22 hay
// que pausar el timer; tras la lectura se reanuda.
void ircontrol_pause();
void ircontrol_resume();
