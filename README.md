# ACME — Actividad 3 (MUINTEL07): sistema integrado

Firmware para **ESP32** que **fusiona en un único sistema** los dos proyectos
previos de la asignatura de Instrumentación Electrónica:

- **Actividad 1** — clasificador de cajas ACME: mide la altura de las cajas con
  un sensor ultrasónico y las desvía con una compuerta servoaccionada hacia el
  *ascensor* (cajas altas) o la *escalera* (cajas bajas).
- **Actividad 2** — ascensor inteligente de 5 plantas: llamadas por mando IR y
  botones, cola FIFO, detección de presencia (PIR) y control ambiental ON-OFF
  con zona muerta (temperatura, humedad e iluminación).

La simulación corre en **Wokwi** (extensión de VS Code). No hace falta hardware
físico.

> **Mejora de la Actividad 3:** sobre la base fusionada Act1+Act2 se ha
> implementado el **autodiagnóstico** (instrumentación avanzada): redundancia
> de sensores con conmutación automática, detección de fallos por rango,
> promediado de medidas y alarma escalonada — ver [§7](#7-autodiagnóstico-actividad-3).

---

## 1. Integración funcional Act1 ↔ Act2

Los dos sistemas no solo conviven en el mismo `loop()`: están **conectados
funcionalmente**. Cuando el clasificador confirma una **caja alta (ASCENSOR)**,
mapea su altura a una planta destino (caja más alta → planta más alta) y llama
internamente a `elevator_request(planta)`. Es decir: la caja que se desvía al
carril del ascensor en Act1 **sube físicamente** por el ascensor de Act2.

Las cajas bajas (ESCALERA) se desvían a la escalera y no llaman al ascensor.

---

## 2. Estructura del proyecto

```
actividad_3/
├── README.md                  ← este archivo
├── muintel07_act3.docx         ← enunciado oficial
└── actividad_3_vs/             ← sketch Arduino integrado
    ├── actividad_3_vs.ino      ← orquestador (setup + loop no bloqueante)
    ├── config.h                ← pines, setpoints, umbrales y constantes
    ├── sensors.{h,cpp}          ← HC-SR04 + DHT22 + LDR + PIR + botones planta
    ├── actuators.{h,cpp}        ← 2 servos + LEDs (heater/cooler/lamp/verde/rojo) + buzzer
    ├── classifier.{h,cpp}       ← FSM del clasificador de cajas + puente al ascensor
    ├── elevator.{h,cpp}         ← FSM del ascensor + cola FIFO
    ├── control.{h,cpp}          ← lazos ON-OFF con histéresis + modelo térmico simulado
    ├── diagnostics.{h,cpp}       ← autodiagnóstico: redundancia, fallos, alarma
    ├── ircontrol.{h,cpp}        ← decodificación del mando IR
    ├── display.{h,cpp}          ← HMI sobre LCD 20×4
    ├── logger.{h,cpp}           ← telemetría CSV unificada (+ RTC DS1307)
    ├── diagram.json             ← circuito Wokwi
    ├── wokwi.toml               ← configuración de la simulación
    └── libraries.txt            ← librerías requeridas
```

---

## 3. Hardware — asignación de pines

Todos los pines coexisten **sin conflicto**: los GPIO 4, 34, 21 y 22 son
sensores compartidos a propósito (DHT22, LDR y bus I²C); el resto son
disjuntos entre ambos subsistemas.

| Componente | GPIO | Subsistema | Notas |
|---|---|---|---|
| DHT22 principal (T, H) | 4 | compartido | Sensor ambiental |
| DHT22 reserva (T, H) | 17 | autodiagnóstico | Sensor redundante (GPIO antes del botón P4) |
| LDR (iluminación) | 34 | compartido | Input-only, ADC1 |
| LCD 20×4 I²C | 21 / 22 | compartido | Bus I²C, dirección `0x27` |
| RTC DS1307 | 21 / 22 | logger | Mismo bus I²C, dirección `0x68` |
| HC-SR04 TRIG / ECHO | 5 / 18 | clasificador | Medida de altura de cajas |
| Servo cabina (ascensor) | 19 | ascensor | PWM, 0–180° (5 plantas) |
| Servo compuerta (clasificador) | 13 | clasificador | PWM, posiciones 30/90/150° |
| Receptor IR | 15 | ascensor | Mando a distancia (NEC) |
| Botones planta P0–P3 | 32, 33, 35, 16 | ascensor | P2 (35) con pull-up externo 10 kΩ; la planta 4 no tiene botón físico |
| Botón RESET contadores | 14 | clasificador | INPUT_PULLUP |
| PIR HC-SR501 | 23 | ascensor | Presencia en cabina |
| LED calentador (rojo) | 2 | control | ON-OFF |
| LED enfriador (azul) | 12 | control | ON-OFF |
| LED lámpara (amarillo) | 0 | control | PWM (LEDC 8 bits) |
| LED verde (→ ASCENSOR) | 25 | clasificador | Feedback de clasificación |
| LED rojo (→ ESCALERA) | 26 | clasificador | Feedback de clasificación |
| Buzzer | 27 | clasificador / alarma | Pitido en clasificación y en alarma de diagnóstico |
| Potenciómetro (sensor de corriente) | 39 | autodiagnóstico | Input-only ADC1; inyector del fallo de sobreconsumo |

---

## 4. Compilación

Requiere **arduino-cli** con el núcleo **ESP32** instalado.

### 4.1 Núcleo y librerías

```bash
# Núcleo ESP32 (probado con esp32:esp32 3.3.8)
arduino-cli core install esp32:esp32

# Librerías requeridas (ver libraries.txt)
arduino-cli lib install "LiquidCrystal I2C" "DHT sensor library" \
  "Adafruit Unified Sensor" "ESP32Servo" "IRremote" "RTClib"
```

### 4.2 Compilar el sketch

```bash
cd actividad_3_vs
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build .
```

Genera en `build/` los artefactos `actividad_3_vs.ino.bin` y
`actividad_3_vs.ino.elf` que necesita la simulación.

> El *warning* `library LiquidCrystal I2C claims to run on avr architecture`
> es inofensivo: la librería funciona igual en ESP32.

### 4.3 Simular en Wokwi (VS Code)

1. Abrir `actividad_3_vs/diagram.json` en VS Code.
2. `F1 → Wokwi: Start Simulator`.

La extensión de Wokwi **no compila**: simula el binario generado en el paso
4.2 (rutas indicadas en `wokwi.toml`). Tras cualquier cambio de código hay
que recompilar antes de volver a simular. Editar `diagram.json` (mover
componentes, cables) **no** requiere recompilar.

---

## 5. Arquitectura del firmware

El `loop()` es completamente **no bloqueante**: cada subsistema usa su propio
temporizador con `millis()`, nunca `delay()`. Cadena de ejecución por iteración:

```
ircontrol_poll() + buttonsPoll()  →  elevator_request()   (llamadas IR/botón)
classifier_tick()                 →  FSM del clasificador + puente al ascensor
elevator_tick()                   →  FSM del ascensor (cabina, puerta, cola)
diag_tick()                       →  autodiagnóstico: lee y valida sensores,
                                      conmuta a la reserva, promedia, alarma
control_tick()                    →  lazos ON-OFF con las medidas validadas
displayUpdate()                   →  refresco del LCD 20×4 (cada 1 s)
logger_tick()                     →  fila CSV por Serial (cada 1 s)
```

### 5.1 FSM del clasificador (`classifier`)

`IDLE → MEASURING → CLASSIFY → WAIT_CLEAR`. Exige `MUESTRAS_VALIDACION` (3)
lecturas consecutivas bajo umbral para confirmar una caja (anti-rebote).
Al confirmarla, clasifica por altura, mueve la compuerta, enciende el LED
verde/rojo, pita y —si es caja alta— **dispara el puente al ascensor**.

### 5.2 FSM del ascensor (`elevator`)

`IDLE → MOVING → DOOR_OPEN → OCCUPIED → ALARM`, con cola FIFO sin duplicados
(hasta 8 peticiones). El PIR mantiene la puerta abierta (`OCCUPIED`) mientras
detecta pasajero.

### 5.3 Control ON-OFF con zona muerta e histéresis (`control`)

Tres lazos independientes:

- Temperatura: SP 25 °C, banda ±3 °C → calentador / reposo / enfriador.
- Iluminación: SP 400 lx, banda ±50 lx → lámpara ON / reposo / OFF.
- Humedad: SP 80 %, banda ±5 % → humidificador / reposo / deshumidificador.

La histéresis apaga el actuador al cruzar el setpoint (no el borde de la
banda), evitando el *chattering*.

### 5.4 HMI — LCD 20×4

Las cuatro líneas muestran todo a la vez, sin conmutar pantallas:

```
Línea 0:  CAJA#07 ASCENSOR P3      ← última caja clasificada
Línea 1:  ELEV MOVING P0>P3        ← estado del ascensor
Línea 2:  T23.0C H55% L401lx       ← clima (temperatura, humedad, lux)
Línea 3:  Asc07 Esc04 OK LP P0     ← contadores + actuadores + PIR
```

### 5.5 Logger — CSV unificado

Una sola tabla CSV por Serial cada 1 s, con timestamp del RTC DS1307 (o
`millis()` si el RTC no responde). Incluye las columnas de autodiagnóstico:

```
timestamp,piece_n,box_h_cm,box_class,floor_cur,floor_tgt,elev_state,
T_C,H_pct,Lux_lx,heater,cooler,lamp_pwm,humidifier,dehumidifier,pir,
temp_src,dht_pri_ok,dht_bck_ok,ldr_ok,act_ok,current_ma,diag_sev
```

### 5.6 Autodiagnóstico (`diagnostics`)

Capa de instrumentación que se interpone entre la lectura cruda de sensores y
el lazo de control. En cada tick lee, valida, promedia y vigila — ver
[§7](#7-autodiagnóstico-actividad-3).

---

## 6. Mando IR y controles

| Botón mando IR | Command NEC | Planta |
|---|---|---|
| `0` | `0x68` | P0 |
| `1` | `0x30` | P1 |
| `2` | `0x18` | P2 |
| `3` | `0x7A` | P3 |
| `4` | `0x10` | P4 |

- **Botones físicos P0–P3**: redundantes con el mando IR (llaman a planta).
  La **planta 4 no tiene pulsador físico** — su GPIO17 se reutiliza para el
  2º DHT22 del autodiagnóstico —, pero **la planta 4 sigue siendo plenamente
  accesible con el botón 4 del mando IR**. Los botones físicos y el mando IR
  siempre fueron redundantes entre sí, así que no se pierde ninguna planta.
- **Botón RESET (GPIO14)**: pone a cero los contadores de clasificación
  (`Asc`, `Esc` y nº de caja) y borra la última caja mostrada. No afecta al
  ascensor.

---

## 7. Autodiagnóstico (Actividad 3)

La mejora de instrumentación avanzada elegida para la Actividad 3 es el
**autodiagnóstico**. El módulo `diagnostics` se interpone entre la lectura
cruda de los sensores y el lazo de control, y aporta cuatro funciones:

### 7.1 Promediado de medidas

Media móvil de las últimas muestras de temperatura, humedad e iluminación,
para reducir el ruido de medida (tema 6, fig. 11 del enunciado).

### 7.2 Detección de fallos por límites de rango

Cada lectura se compara con un rango físico plausible. Una lectura fuera de
rango (o `NaN` en el DHT22), sostenida varias muestras consecutivas, se
interpreta como **avería del sensor** (cortocircuito o circuito abierto de la
conexión). Se vigilan el DHT22 principal, el DHT22 de reserva y el LDR.

### 7.3 Redundancia del sensor de temperatura

Se ha **duplicado el sensor de temperatura** con un 2º DHT22 (en GPIO17). Si
el sensor principal falla, el sistema **conmuta automáticamente al de
reserva** y sigue operando; cuando el principal se recupera, vuelve a él. La
HMI indica la fuente activa con un marcador `P` (principal) / `R` (reserva)
en la línea de clima.

> **Nota importante:** el GPIO17 que ahora usa el 2º DHT22 era el del pulsador
> físico de la planta 4. Ese pulsador se ha retirado, **pero la planta 4
> sigue siendo plenamente accesible mediante el botón 4 del mando IR** — los
> botones físicos y el mando IR siempre fueron redundantes. No se pierde
> ninguna planta del ascensor.

### 7.4 Diagnóstico de actuadores por consumo

Un sensor de corriente (potenciómetro en GPIO39, que modela la señal ya
acondicionada de un shunt + amplificador) vigila el consumo de los actuadores.
Si la corriente supera el umbral de seguridad, se señala un **sobreconsumo**.

### 7.5 Reacción escalonada ante el fallo

| Severidad | Cuándo | Reacción |
|---|---|---|
| **OK** | Todo correcto | Operación normal |
| **AVISO** | Falla un sensor con reserva (DHT principal) | Conmuta al de reserva, lo indica en LCD y CSV; **sin alarma**, el sistema sigue operando |
| **ALARMA** | Fallo crítico sin reserva: ambos DHT22, o el LDR, o sobreconsumo de actuador | El ascensor entra en estado `ALARM` (cabina congelada) + buzzer periódico + banner en el LCD |

Al desaparecer el fallo, el sistema se recupera automáticamente: la alarma se
libera y el ascensor vuelve a `IDLE`.

> HC-SR04 y PIR quedan fuera de la detección de fallos por rango: no es
> fiable distinguir "no hay caja" / "no hay presencia" de una avería real.

---

## 8. Licencia y autoría

Proyecto académico — asignatura **Instrumentación Electrónica** (MUINTEL07),
Máster Universitario en Ingeniería de Telecomunicación, UNIR.

Autor: Stefan Lica Cristian Ionita.
