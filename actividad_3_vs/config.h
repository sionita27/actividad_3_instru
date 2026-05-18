// ============================================================================
//  config.h — Constantes y pines del proyecto ACME integrado (Actividad 3).
//
//  Fusiona los dos sistemas previos en un único ESP32:
//    · Act1 — clasificador de cajas por altura (HC-SR04 + servo-compuerta).
//    · Act2 — ascensor inteligente de 5 plantas + control ambiental.
//
//  Cabecera centralizada: solo declara constantes. Cualquier cambio de pin,
//  setpoint o periodo se hace SÓLO aquí.
//
//  Mapa de GPIO (sin conflictos — 4/34/21/22 son sensores compartidos):
//    Act1:  4(DHT) 5(TRIG) 13(servo compuerta) 14(reset) 18(ECHO)
//           21/22(I²C) 25(LED verde) 26(LED rojo) 27(buzzer) 34(LDR)
//    Act2:  0(lámpara) 2(heater) 4(DHT) 12(cooler) 15(IR) 16/17/32/33/35
//           (botones planta) 19(servo cabina) 21/22(I²C) 23(PIR) 34(LDR)
// ============================================================================

#pragma once

// ----------------------------------------------------------------------------
//  1) Puerto serie (UART) hacia el monitor de Wokwi
// ----------------------------------------------------------------------------
#define SERIAL_BAUD          115200

// ----------------------------------------------------------------------------
//  2) Sensores ambientales compartidos (DHT22 + LDR)
// ----------------------------------------------------------------------------
#define PIN_DHT22            4
#define DHT22_PERIODO_MS     2000UL

#define PIN_LDR              34       // INPUT-only del ESP32 → ideal para ADC.
#define ADC_MAX              4095     // ADC 12 bits.
#define ADC_VREF             3.3f
#define LDR_DIV_OHMS         10000.0f
#define LDR_GAMMA            0.7f
#define LDR_R10_OHMS         50000.0f

// ----------------------------------------------------------------------------
//  3) Sensor ultrasónico HC-SR04 (clasificador de cajas — Act1)
// ----------------------------------------------------------------------------
#define PIN_HCSR04_TRIG      5
#define PIN_HCSR04_ECHO      18

// Umbrales de clasificación por distancia medida (cm):
//   d < UMBRAL_ALTA_CM            → caja ALTA   (ASCENSOR)
//   UMBRAL_ALTA_CM ≤ d ≤ NADA     → caja BAJA   (ESCALERA)
//   d > UMBRAL_NADA_CM            → sin caja
#define UMBRAL_ALTA_CM       15
#define UMBRAL_NADA_CM       30
#define RANGO_MAX_CM         50       // Límite lógico: lecturas mayores = ruido.

// ----------------------------------------------------------------------------
//  4) Bus I²C — LCD 20x4 (0x27) + RTC DS1307 (0x68) en el mismo bus
// ----------------------------------------------------------------------------
#define LCD_I2C_ADDRESS      0x27
#define LCD_COLS             20       // LCD 20x4 (wokwi-lcd2004).
#define LCD_ROWS             4
#define RTC_I2C_ADDRESS      0x68

// ----------------------------------------------------------------------------
//  5) Servomotor de la cabina del ascensor (Act2)
// ----------------------------------------------------------------------------
#define PIN_SERVO_CAB        19

// Ángulos del servo SG90 mapeados a planta (5 plantas, rango 0-180°).
#define SERVO_ANG_PLANTA_0   0
#define SERVO_ANG_PLANTA_1   45
#define SERVO_ANG_PLANTA_2   90
#define SERVO_ANG_PLANTA_3   135
#define SERVO_ANG_PLANTA_4   180
#define NUM_PLANTAS          5

// Tiempo estimado del recorrido del servo (peor caso planta 0 ↔ 4).
#define SERVO_TRAVEL_MS      1500UL

// ----------------------------------------------------------------------------
//  6) Servomotor de la compuerta de desvío de cajas (Act1)
// ----------------------------------------------------------------------------
//  GPIO13: libre, sin función especial al arranque. Distinto del servo
//  de la cabina (GPIO19) → ambos servos coexisten sin conflicto.
#define PIN_SERVO_GATE       13
#define SERVO_ANG_NEUTRAL    90       // Compuerta centrada (reposo).
#define SERVO_ANG_ASCENSOR   30       // Desvía caja alta.
#define SERVO_ANG_ESCALERA   150      // Desvía caja baja.

// ----------------------------------------------------------------------------
//  7) Receptor IR (mando a distancia del ascensor — Act2)
// ----------------------------------------------------------------------------
#define PIN_IR_RX            15

// ----------------------------------------------------------------------------
//  8) Botones físicos de planta (5 pulsadores — Act2)
// ----------------------------------------------------------------------------
//  GPIO35 es input-only sin pull-up interno → resistencia externa de 10 kΩ
//  a 3V3 (definida en diagram.json).
#define PIN_BTN_P0           32
#define PIN_BTN_P1           33
#define PIN_BTN_P2           35
#define PIN_BTN_P3           16
#define PIN_BTN_P4           17

#define DEBOUNCE_MS          50UL

// ----------------------------------------------------------------------------
//  9) Botón de reset de contadores del clasificador (Act1)
// ----------------------------------------------------------------------------
#define PIN_BOTON_RESET      14

// ----------------------------------------------------------------------------
//  10) Sensor PIR de presencia en cabina (Act2)
// ----------------------------------------------------------------------------
#define PIN_PIR              23

// ----------------------------------------------------------------------------
//  11) LEDs de control ambiental (Act2)
// ----------------------------------------------------------------------------
//  GPIO 0/2/12 son strapping pins; funcionan como salida normal.
#define PIN_LED_HEATER       2        // LED rojo (calentador).
#define PIN_LED_COOLER       12       // LED azul (enfriador).
#define PIN_LED_LAMP         0        // LED amarillo PWM (lámpara).

#define LEDC_LAMP_FREQ_HZ    5000
#define LEDC_LAMP_RES_BITS   8        // 0-255.

// ----------------------------------------------------------------------------
//  12) LEDs y buzzer del clasificador (Act1)
// ----------------------------------------------------------------------------
#define PIN_LED_VERDE        25       // Clasificación → ASCENSOR.
#define PIN_LED_ROJO         26       // Clasificación → ESCALERA.
#define PIN_BUZZER           27
#define BUZZER_FREQ_HZ       2000
#define BUZZER_DUR_MS        120

// ----------------------------------------------------------------------------
//  13) Setpoints y zona muerta del control ON-OFF (Act2)
// ----------------------------------------------------------------------------
#define SETPOINT_TEMP_C      25.0f
#define BANDA_TEMP_C         3.0f

#define SETPOINT_LUX_LX      400.0f
#define BANDA_LUX_LX         50.0f

#define SETPOINT_HUM_PCT     80.0f
#define BANDA_HUM_PCT        5.0f

// ----------------------------------------------------------------------------
//  14) Modelo térmico simulado (Act2)
// ----------------------------------------------------------------------------
//  MODO_SIM_TERMICO=0: el LCD/serial muestran la lectura cruda del sensor.
//  MODO_SIM_TERMICO=1: se simula el efecto del actuador sobre el sensor.
#define MODO_SIM_TERMICO     0
#define SIM_PERIODO_MS       500UL
#define SIM_DELTA_TEMP       0.5f
#define SIM_FUGA_TEMP        0.1f
#define SIM_DELTA_HUM        0.5f
#define SIM_FUGA_HUM         0.1f
#define SIM_DELTA_LUX       30.0f
#define SIM_FUGA_LUX         5.0f

// ----------------------------------------------------------------------------
//  15) FSM del clasificador de cajas (Act1)
// ----------------------------------------------------------------------------
//  Exigimos N lecturas consecutivas bajo umbral para confirmar una caja
//  (anti-rebote). Con periodo 500 ms y N=3 → 1.5 s de presencia mínima.
#define CLASIFICADOR_PERIODO_MS  500UL
#define MUESTRAS_VALIDACION      3
#define CLASIFICACION_DUR_MS     2000UL
#define WAIT_CLEAR_TIMEOUT_MS    3000UL

// ----------------------------------------------------------------------------
//  16) Puente funcional Act1 → ascensor
// ----------------------------------------------------------------------------
//  Cuando el clasificador confirma una caja ALTA (ASCENSOR), su altura se
//  mapea a una planta destino y se llama a elevator_request(planta):
//  caja más alta (menor distancia medida) → planta más alta.
//      altura = PUENTE_ALTURA_MIN_CM  → planta NUM_PLANTAS-1
//      altura = PUENTE_ALTURA_MAX_CM  → planta 0
#define PUENTE_ALTURA_MIN_CM     2
#define PUENTE_ALTURA_MAX_CM     (UMBRAL_ALTA_CM - 1)   // 14 cm

// ----------------------------------------------------------------------------
//  17) Periodos de la FSM del ascensor, refresco de HMI y logger
// ----------------------------------------------------------------------------
#define DOOR_OPEN_MS         5000UL   // 5 s con la puerta abierta.
#define LCD_REFRESH_MS       1000UL   // Refresco del LCD 20x4.
#define LOG_PERIOD_MS        1000UL   // Fila CSV cada 1 s.

#define ELEVATOR_QUEUE_LEN   8        // Máximo de plantas en cola.
