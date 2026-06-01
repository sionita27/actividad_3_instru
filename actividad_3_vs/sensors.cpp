// ============================================================================
//  sensors.cpp — Implementación del módulo de sensores (Actividad 3).
//
//  Fusiona el HC-SR04 del clasificador (Act1) con DHT22 + LDR + PIR + botones
//  de planta del ascensor (Act2). El DHT22 y el LDR son los MISMOS sensores
//  físicos en ambos sistemas: una sola copia da servicio a todo.
// ============================================================================

#include <Arduino.h>
#include <math.h>
#include <DHT.h>             // Adafruit — driver del DHT22.
#include "config.h"
#include "sensors.h"
#include "ircontrol.h"       // Para pausar el timer de IR durante la lectura DHT.

// Objetos DHT — viven solo dentro de este .cpp.
//   dht  : sensor principal (PIN_DHT22).
//   dhtB : sensor de reserva del autodiagnóstico (PIN_DHT22_B).
static DHT dht(PIN_DHT22, DHT22);
static DHT dhtB(PIN_DHT22_B, DHT22);

// ----------------------------------------------------------------------------
//  Constantes internas del HC-SR04
// ----------------------------------------------------------------------------
// Timeout de pulseIn(): 30 000 µs ≈ 5 m. Sin timeout, pulseIn() podría
// bloquear el loop indefinidamente si el sensor no ve nada.
static const unsigned long PULSEIN_TIMEOUT_US   = 30000UL;
// Conversión µs → cm (ida+vuelta, sonido ≈ 340 m/s): d_cm ≈ t_us / 58.
static const unsigned long US_PER_CM_ROUND_TRIP = 58UL;

// ----------------------------------------------------------------------------
//  Estado interno de los botones de planta (debounce + flanco descendente).
// ----------------------------------------------------------------------------
struct ButtonState {
    uint8_t  pin;
    uint8_t  lastReading;
    uint32_t lastChange;
    uint8_t  stable;
    bool     pressed;
};

// Cuatro botones (P0..P3). La planta 4 no tiene pulsador físico — se llama
// con el mando IR (su GPIO17 lo usa ahora el 2º DHT22).
static ButtonState BUTTONS[] = {
    { PIN_BTN_P0, HIGH, 0, HIGH, false },
    { PIN_BTN_P1, HIGH, 0, HIGH, false },
    { PIN_BTN_P2, HIGH, 0, HIGH, false },
    { PIN_BTN_P3, HIGH, 0, HIGH, false },
};
static const size_t NUM_BUTTONS = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

// ============================================================================
//  initSensors() — preparar pines y librerías.
// ============================================================================
void initSensors() {
    // HC-SR04: TRIG como salida (en reposo a LOW), ECHO como entrada.
    pinMode(PIN_HCSR04_TRIG, OUTPUT);
    pinMode(PIN_HCSR04_ECHO, INPUT);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    // LDR — GPIO34 input-only, sin pull-up.
    pinMode(PIN_LDR, INPUT);

    // Sensor de corriente de actuadores (potenciómetro) — GPIO39 input-only.
    pinMode(PIN_CURRENT_SENSE, INPUT);

    // Botones de planta. GPIO35 es input-only sin pull-up interno → usa
    // resistencia externa de 10 kΩ (diagram.json); el resto, pull-up interno.
    for (size_t i = 0; i < NUM_BUTTONS; ++i) {
        if (BUTTONS[i].pin == PIN_BTN_P2) {
            pinMode(BUTTONS[i].pin, INPUT);
        } else {
            pinMode(BUTTONS[i].pin, INPUT_PULLUP);
        }
    }

    // PIR HC-SR501 — salida digital de 3.3 V tolerada.
    pinMode(PIN_PIR, INPUT);

    // DHT22 — begin() configura el pin y resetea el cache de la librería.
    // Se inicializan los dos: principal y reserva.
    dht.begin();
    dhtB.begin();
}

// ============================================================================
//  readHeightCm() — una medida del ultrasónico HC-SR04.
//  Devuelve cm o READ_DISTANCE_ERROR (-1) si la lectura falla.
// ============================================================================
int readHeightCm() {
    // 1) Disparo de 10 µs en TRIG.
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_HCSR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    // 2) Medir la duración del eco en ECHO.
    unsigned long duration_us = pulseIn(PIN_HCSR04_ECHO, HIGH, PULSEIN_TIMEOUT_US);

    // 3) Timeout → lectura no válida.
    if (duration_us == 0) {
        return READ_DISTANCE_ERROR;
    }

    // 4) Convertir tiempo a distancia.
    int cm = (int)(duration_us / US_PER_CM_ROUND_TRIP);

    // 5) Validación de rango útil: descartar zona muerta (<2 cm) y ruido
    //    (> RANGO_MAX_CM). Ambos casos se tratan como "no hay caja".
    if (cm < 2 || cm > RANGO_MAX_CM) {
        return READ_DISTANCE_ERROR;
    }

    return cm;
}

// ============================================================================
//  readEnvironment() — DHT22 (temperatura y humedad).
// ----------------------------------------------------------------------------
//  Pausamos el timer de IRremote durante la lectura: el bit-banging del
//  DHT22 (pulsos de 26-70 µs) es muy sensible al ISR de IRremote (50 µs).
//  Sin esta pausa, en ESP32 + IRremote v4 la lectura siempre devuelve NaN.
// ============================================================================
bool readEnvironment(float &t, float &h) {
    ircontrol_pause();
    float tLocal = dht.readTemperature();
    float hLocal = dht.readHumidity();
    ircontrol_resume();

    if (isnan(tLocal) || isnan(hLocal)) {
        static uint32_t failCount = 0;
        failCount++;
        if (failCount <= 3 || (failCount % 5 == 0)) {
            Serial.printf("[DHT] FAIL #%lu (t=%.2f h=%.2f)\r\n",
                          (unsigned long) failCount, tLocal, hLocal);
        }
        return false;
    }

    static uint32_t okCount = 0;
    okCount++;
    if (okCount <= 3 || (okCount % 10 == 0)) {
        Serial.printf("[DHT] OK  #%lu raw T=%.1fC H=%.1f%%\r\n",
                      (unsigned long) okCount, tLocal, hLocal);
    }

    t = tLocal;
    h = hLocal;
    return true;
}

// ============================================================================
//  readEnvironmentB() — DHT22 de RESERVA (autodiagnóstico).
// ----------------------------------------------------------------------------
//  Sensor redundante del autodiagnóstico. Misma mecánica que readEnvironment()
//  (pausa del timer IR incluida), pero sin la traza serie detallada para no
//  duplicar mensajes en el monitor.
// ============================================================================
bool readEnvironmentB(float &t, float &h) {
    ircontrol_pause();
    float tLocal = dhtB.readTemperature();
    float hLocal = dhtB.readHumidity();
    ircontrol_resume();

    if (isnan(tLocal) || isnan(hLocal)) {
        return false;
    }
    t = tLocal;
    h = hLocal;
    return true;
}

// ============================================================================
//  readActuatorCurrent() — sensor de corriente de los actuadores (simulado).
// ----------------------------------------------------------------------------
//  Lee el ADC del potenciómetro (PIN_CURRENT_SENSE) y lo escala linealmente a
//  miliamperios simulados: 0 → 0 mA, fondo de escala → DIAG_CURRENT_FULLSCALE_MA.
//  El potenciómetro representa la señal ya acondicionada de un sensor de
//  corriente real; moverlo permite inyectar el fallo de sobreconsumo.
// ============================================================================
float readActuatorCurrent() {
    int adc = analogRead(PIN_CURRENT_SENSE);
    float mA = (adc / (float) ADC_MAX) * DIAG_CURRENT_FULLSCALE_MA;
    if (mA < 0.0f) mA = 0.0f;
    return mA;
}

// ============================================================================
//  readLux() — LDR convertido a lux reales (fórmula de Corona, 2014).
// ----------------------------------------------------------------------------
//  El componente wokwi-photoresistor-sensor tiene divisor interno:
//      VCC ── R_pullup (10 kΩ) ── AO ── R_LDR ── GND
//  Por eso MÁS luz ⇒ menor R_LDR ⇒ MENOR voltaje en AO.
//  Pasos: ADC → V_AO → R_LDR → lux = 10·(R10/R)^(1/γ).
// ============================================================================
float readLux() {
    int adc = analogRead(PIN_LDR);
    float v = (adc * ADC_VREF) / (float) ADC_MAX;

    // Evitar divisiones por cero al saturar el ADC.
    if (v <= 0.001f) v = 0.001f;
    if (v >= ADC_VREF - 0.001f) v = ADC_VREF - 0.001f;

    float rLdr = LDR_DIV_OHMS * v / (ADC_VREF - v);
    float lux  = 10.0f * powf(LDR_R10_OHMS / rLdr, 1.0f / LDR_GAMMA);

    if (lux < 0.0f)      lux = 0.0f;
    if (lux > 99999.0f)  lux = 99999.0f;
    return lux;
}

// ============================================================================
//  pirRead() — sensor PIR HC-SR501. HIGH ⇒ presencia.
// ============================================================================
bool pirRead() {
    return digitalRead(PIN_PIR) == HIGH;
}

// ============================================================================
//  buttonsPoll() — sondea los 5 botones de planta con debounce.
// ----------------------------------------------------------------------------
//  Debounce por temporización: aceptamos un nivel como "estable" si se
//  mantiene > DEBOUNCE_MS. Emitimos UN evento en el flanco HIGH→LOW estable.
//  Devuelve la primera planta pulsada en este tick, o -1.
// ============================================================================
int8_t buttonsPoll() {
    uint32_t now    = millis();
    int8_t   evento = -1;

    for (size_t i = 0; i < NUM_BUTTONS; ++i) {
        uint8_t r = digitalRead(BUTTONS[i].pin);

        if (r != BUTTONS[i].lastReading) {
            BUTTONS[i].lastReading = r;
            BUTTONS[i].lastChange  = now;
        }

        if (now - BUTTONS[i].lastChange >= DEBOUNCE_MS) {
            if (BUTTONS[i].stable != r) {
                BUTTONS[i].stable = r;

                if (r == LOW && !BUTTONS[i].pressed) {
                    BUTTONS[i].pressed = true;
                    if (evento < 0) {
                        evento = (int8_t) i;
                        Serial.printf("[BTN] planta=%d\r\n", (int) i);
                    }
                }
                if (r == HIGH) {
                    BUTTONS[i].pressed = false;
                }
            }
        }
    }

    return evento;
}
