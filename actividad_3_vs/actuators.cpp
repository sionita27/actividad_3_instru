// ============================================================================
//  actuators.cpp — Implementación de los actuadores (Actividad 3).
//
//  El ESP32 genera el PWM por hardware con el periférico LEDC. ESP32Servo lo
//  usa internamente para cada servo; para el LED de la lámpara configuramos
//  un canal LEDC propio con ledcAttach(). El buzzer suena con tone().
//
//  Hay DOS servos: la cabina del ascensor (GPIO19) y la compuerta del
//  clasificador (GPIO13). ESP32Servo admite varios servos simultáneos.
// ============================================================================

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "actuators.h"

// ----------------------------------------------------------------------------
//  Estado interno (encapsulado dentro del .cpp con 'static').
// ----------------------------------------------------------------------------
static Servo  cabinServo;          // Servo de la cabina del ascensor.
static Servo  gateServo;           // Servo de la compuerta del clasificador.
static int    cabinLastAngle = 0;

static bool    heaterOn       = false;
static bool    coolerOn       = false;
static uint8_t lampDuty       = 0;
static bool    humidifierOn   = false;
static bool    dehumidifierOn = false;

// Tabla planta → ángulo de la cabina.
static const int ANG_POR_PLANTA[NUM_PLANTAS] = {
    SERVO_ANG_PLANTA_0,
    SERVO_ANG_PLANTA_1,
    SERVO_ANG_PLANTA_2,
    SERVO_ANG_PLANTA_3,
    SERVO_ANG_PLANTA_4,
};

// ============================================================================
//  initActuators() — configurar pines, PWM y servos.
// ============================================================================
void initActuators() {
    // --- Servo de la cabina (ascensor) ------------------------------------
    cabinServo.setPeriodHertz(50);
    cabinServo.attach(PIN_SERVO_CAB, 500, 2400);
    cabinServo.write(SERVO_ANG_PLANTA_0);
    cabinLastAngle = SERVO_ANG_PLANTA_0;

    // --- Servo de la compuerta (clasificador) -----------------------------
    gateServo.setPeriodHertz(50);
    gateServo.attach(PIN_SERVO_GATE, 500, 2400);
    gateServo.write(SERVO_ANG_NEUTRAL);

    // --- LEDs heater / cooler (ascensor) ----------------------------------
    pinMode(PIN_LED_HEATER, OUTPUT);
    pinMode(PIN_LED_COOLER, OUTPUT);
    digitalWrite(PIN_LED_HEATER, LOW);
    digitalWrite(PIN_LED_COOLER, LOW);

    // --- LED lámpara con PWM (LEDC) ---------------------------------------
    ledcAttach(PIN_LED_LAMP, LEDC_LAMP_FREQ_HZ, LEDC_LAMP_RES_BITS);
    ledcWrite(PIN_LED_LAMP, 0);

    // --- LEDs verde/rojo y buzzer (clasificador) --------------------------
    pinMode(PIN_LED_VERDE, OUTPUT);
    pinMode(PIN_LED_ROJO,  OUTPUT);
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_ROJO,  LOW);
    pinMode(PIN_BUZZER, OUTPUT);
}

// ============================================================================
//  Cabina del ascensor (servo GPIO19)
// ============================================================================
void cabinMoveTo(int8_t planta) {
    if (planta < 0 || planta >= NUM_PLANTAS) return;
    int ang = ANG_POR_PLANTA[planta];
    cabinServo.write(ang);
    cabinLastAngle = ang;
    Serial.printf("[CAB] planta=%d angulo=%d\r\n", planta, ang);
}

int cabinAngle() { return cabinLastAngle; }

// ============================================================================
//  Actuadores térmicos del ascensor (LEDs ON/OFF)
// ============================================================================
void heaterSet(bool on) {
    heaterOn = on;
    digitalWrite(PIN_LED_HEATER, on ? HIGH : LOW);
}
void coolerSet(bool on) {
    coolerOn = on;
    digitalWrite(PIN_LED_COOLER, on ? HIGH : LOW);
}
bool heaterIsOn() { return heaterOn; }
bool coolerIsOn() { return coolerOn; }

// ============================================================================
//  Lámpara del ascensor (LED con PWM)
// ============================================================================
void lampSetPwm(uint8_t duty) {
    lampDuty = duty;
    ledcWrite(PIN_LED_LAMP, duty);
}
void    lampSetOn(bool on) { lampSetPwm(on ? 255 : 0); }
bool    lampIsOn()         { return lampDuty > 0; }
uint8_t lampPwm()          { return lampDuty; }

// ============================================================================
//  Humedad del ascensor (solo estado lógico — sin LED físico)
// ============================================================================
void humidifierSet(bool on)   { humidifierOn   = on; }
void dehumidifierSet(bool on) { dehumidifierOn = on; }
bool humidifierIsOn()         { return humidifierOn;   }
bool dehumidifierIsOn()       { return dehumidifierOn; }

// ============================================================================
//  Compuerta del clasificador (servo GPIO13)
// ============================================================================
void servoNeutral()  { gateServo.write(SERVO_ANG_NEUTRAL);  }
void servoAscensor() { gateServo.write(SERVO_ANG_ASCENSOR); }
void servoEscalera() { gateServo.write(SERVO_ANG_ESCALERA); }

// ============================================================================
//  LEDs y buzzer del clasificador
// ============================================================================
void setLedAscensor(bool on) { digitalWrite(PIN_LED_VERDE, on ? HIGH : LOW); }
void setLedEscalera(bool on) { digitalWrite(PIN_LED_ROJO,  on ? HIGH : LOW); }

void clearLeds() {
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_ROJO,  LOW);
}

void beep() {
    tone(PIN_BUZZER, BUZZER_FREQ_HZ, BUZZER_DUR_MS);
}
