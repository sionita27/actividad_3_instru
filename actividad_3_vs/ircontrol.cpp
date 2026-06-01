// ============================================================================
//  ircontrol.cpp — Decodificador IR usando la librería IRremote v4 (la
//  oficial de Arduino, ya validada con el componente wokwi-ir-receiver).
//
//  Estrategia: en lugar de comparar el dato crudo NEC de 32 bits, comparamos
//  el campo "command" (8 bits) que IRremote ya nos extrae. Es más estable
//  ante variaciones de "address" del mando virtual.
//
//  Imprimimos SIEMPRE protocolo, address, command y rawData en cuanto se
//  decodifica algo, así si Wokwi emite valores distintos vemos el código
//  exacto en el monitor serie.
// ============================================================================

#include <Arduino.h>
#include <IRremote.hpp>          // v4: incluye .hpp en uno de los .cpp del proyecto
#include "config.h"
#include "ircontrol.h"

// ----------------------------------------------------------------------------
//  Mapa: command NEC del mando virtual de Wokwi → planta.
//
//  Los códigos NO son los del mando Elegoo genérico (que muchos tutoriales
//  reproducen). El componente wokwi-ir-remote usa su propia tabla, address
//  fijo a 0x00 y los siguientes commands para los dígitos:
//    0=0x68  1=0x30  2=0x18  3=0x7A  4=0x10
//    5=0x38  6=0x5A  7=0x42  8=0x4A  9=0x52
//  Fuente: docs.wokwi.com/parts/wokwi-ir-remote
// ----------------------------------------------------------------------------
struct IrMap {
    uint8_t  command;
    int8_t   planta;
    const char* label;
};
static const IrMap MAPA[] = {
    { 0x68, 0, "0" },
    { 0x30, 1, "1" },
    { 0x18, 2, "2" },
    { 0x7A, 3, "3" },
    { 0x10, 4, "4" },
    { 0x38, -1, "5" },        // Reserva: NUM_PLANTAS=5, no hay planta 5.
    { 0x5A, -1, "6" },
    { 0x42, -1, "7" },
    { 0x4A, -1, "8" },
    { 0x52, -1, "9" },
};
static const size_t MAPA_LEN = sizeof(MAPA) / sizeof(MAPA[0]);

void ircontrol_setup() {
    // ENABLE_LED_FEEDBACK hace parpadear el LED on-board del DevKit cada vez
    // que llega una trama IR — útil para diagnóstico visual.
    IrReceiver.begin(PIN_IR_RX, ENABLE_LED_FEEDBACK);
    Serial.printf("[IR] receptor preparado en GPIO%d\r\n", PIN_IR_RX);
}

int8_t ircontrol_poll() {
    if (!IrReceiver.decode()) {
        return -1;
    }

    uint16_t        proto    = IrReceiver.decodedIRData.protocol;
    uint16_t        address  = IrReceiver.decodedIRData.address;
    uint16_t        command  = IrReceiver.decodedIRData.command;
    IRRawDataType   rawData  = IrReceiver.decodedIRData.decodedRawData;
    bool            isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
    IrReceiver.resume();

    // Log siempre — cualquier trama, incluso UNKNOWN o repeticiones.
    Serial.printf("[IR] proto=%u addr=0x%02X cmd=0x%02X raw=0x%08lX%s\r\n",
                  (unsigned) proto,
                  (unsigned) address,
                  (unsigned) command,
                  (unsigned long) rawData,
                  isRepeat ? " (repeat)" : "");

    if (isRepeat) return -1;        // No reaccionar a pulsaciones largas.
    if (proto == 0)  return -1;     // UNKNOWN: ruido o trama mal decodificada.

    // Buscar en la tabla por command.
    for (size_t i = 0; i < MAPA_LEN; ++i) {
        if (MAPA[i].command == command) {
            int8_t planta = MAPA[i].planta;
            if (planta >= 0 && planta < NUM_PLANTAS) {
                Serial.printf("[IR]   -> boton '%s' -> planta=%d\r\n",
                              MAPA[i].label, (int) planta);
                return planta;
            } else {
                Serial.printf("[IR]   -> boton '%s' (sin planta asignada)\r\n",
                              MAPA[i].label);
                return -1;
            }
        }
    }
    Serial.println(F("[IR]   -> sin mapeo (command no en tabla)"));
    return -1;
}

// ============================================================================
//  Pausa / reanudación del timer interno de IRremote.
//
//  La librería IRremote v4 muestrea el pin del receptor a 50 µs por timer
//  hardware. Eso compite con el bit-banging del DHT22, cuyos pulsos válidos
//  son de 26-70 µs: si el ISR del IR se dispara en mitad de un pulso, se
//  pierde el conteo y dht.read*() devuelve NaN.
//
//  Solución: justo antes de leer el DHT22 paramos el timer del IR; tras la
//  lectura (~30 ms) lo reanudamos. Durante esa pausa no se decodifican
//  tramas IR — pero como el DHT22 sólo se lee cada 2 s, la "ceguera" del
//  receptor es despreciable.
// ============================================================================
void ircontrol_pause() {
    IrReceiver.stopTimer();
}

void ircontrol_resume() {
    IrReceiver.restartTimer();
}
