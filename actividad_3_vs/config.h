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
//  8) Botones físicos de planta (4 pulsadores — Act2)
// ----------------------------------------------------------------------------
//  GPIO35 es input-only sin pull-up interno → resistencia externa de 10 kΩ
//  a 3V3 (definida en diagram.json).
//
//  Nota Act3: el pulsador físico de la planta 4 se RETIRÓ para liberar el
//  GPIO17, que ahora usa el 2º DHT22 (sensor de temperatura de reserva del
//  autodiagnóstico). La planta 4 SIGUE siendo accesible mediante el mando IR
//  (botón 4 del mando) — los botones físicos y el IR siempre fueron
//  redundantes entre sí.
#define PIN_BTN_P0           32
#define PIN_BTN_P1           33
#define PIN_BTN_P2           35
#define PIN_BTN_P3           16

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

// ----------------------------------------------------------------------------
//  18) Autodiagnóstico — Actividad 3 (instrumentación avanzada)
// ----------------------------------------------------------------------------
//  Sensor de temperatura REDUNDANTE: 2º DHT22 en GPIO17 (pin liberado al
//  retirar el pulsador de planta 4). Da reserva de temperatura y humedad.
#define PIN_DHT22_B          17

//  Sensor de corriente de los actuadores (simulado con un potenciómetro en
//  GPIO39, input-only/ADC). El potenciómetro modela la señal YA ACONDICIONADA
//  de un sensor de corriente real (shunt + amplificador): en Wokwi no se
//  puede variar el consumo real de un LED, así que el potenciómetro actúa
//  como inyector del fallo de sobreconsumo para la demostración.
#define PIN_CURRENT_SENSE    39

//  Límites de rango físico plausible de cada sensor. Una lectura fuera de
//  estos límites (o NaN en el DHT) indica avería: cortocircuito o circuito
//  abierto de la conexión del sensor.
//  Temperatura: banda de operación plausible del entorno de la planta ACME.
//  Una lectura fuera de [-10, 50] °C se interpreta como avería del sensor.
//  Es más estrecha que el recorrido del slider del DHT22 (-40..80 °C) a
//  propósito: así, llevando el slider a un extremo se puede demostrar el
//  fallo y la conmutación automática al sensor de reserva.
#define DIAG_TEMP_MIN_C      -10.0f
#define DIAG_TEMP_MAX_C       50.0f
//  Humedad: una lectura por debajo del 15 % es implausible en una nave
//  industrial → se interpreta como avería del sensor (el chequeo es
//  h >= 15 % → válido). Por arriba el tope es el límite físico (100 %);
//  el setpoint de control (80 %) queda dentro de la banda válida.
#define DIAG_HUM_MIN_PCT      15.0f
#define DIAG_HUM_MAX_PCT     100.0f
#define DIAG_LUX_MIN_LX        1.0f
#define DIAG_LUX_MAX_LX    20000.0f

//  Promediado (media móvil) para reducir el ruido de medida (tema 6, fig. 11).
#define DIAG_AVG_WINDOW        5      // Nº de muestras de la media móvil.
#define DIAG_SAMPLE_MS         200UL  // Cadencia de muestreo de LDR/corriente.
#define DIAG_FAULT_SAMPLES     3      // Muestras consecutivas para confirmar
                                       //   un fallo (o una recuperación).

//  Sensor de corriente: escalado del ADC a mA simulados y umbral de
//  sobreconsumo. En reposo el potenciómetro queda bajo (corriente pequeña);
//  al subirlo por encima del umbral se inyecta el fallo de sobreconsumo.
#define DIAG_CURRENT_FULLSCALE_MA  800.0f  // Corriente a fondo de escala (ADC máx).
#define DIAG_CURRENT_MAX_MA        500.0f  // Por encima → sobreconsumo (alarma).

//  Periodo del pitido de alarma mientras hay un fallo crítico activo.
#define DIAG_BUZZER_PERIOD_MS  1000UL
