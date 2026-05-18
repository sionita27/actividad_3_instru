// ============================================================================
//  display.cpp — Implementación del HMI sobre LCD 20x4 I²C.
//
//  Las 4 líneas muestran simultáneamente el clasificador, el ascensor y el
//  control ambiental. Se refresca cada LCD_REFRESH_MS. Cada línea se imprime
//  rellena a 20 caracteres para borrar el contenido anterior sin parpadeo.
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "display.h"
#include "classifier.h"
#include "elevator.h"
#include "control.h"
#include "actuators.h"
#include "sensors.h"

static LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLS, LCD_ROWS);

static uint32_t tLastRefresh = 0;

// ----------------------------------------------------------------------------
//  Helper: imprime una cadena en una fila rellenando/truncando a LCD_COLS.
// ----------------------------------------------------------------------------
static void printPadded(const char* s, uint8_t row) {
    char buf[LCD_COLS + 1];
    snprintf(buf, sizeof(buf), "%-*s", LCD_COLS, s);
    buf[LCD_COLS] = '\0';
    lcd.setCursor(0, row);
    lcd.print(buf);
}

// ----------------------------------------------------------------------------
//  Etiquetas cortas de actuador (2 chars → ancho fijo, sin parpadeo).
// ----------------------------------------------------------------------------
static const char* labelTemp() {
    if (heaterIsOn()) return "HT";   // Calentador ON.
    if (coolerIsOn()) return "CL";   // Enfriador ON.
    return "OK";
}
static const char* labelLamp() {
    return lampIsOn() ? "LP" : "--";
}

void initDisplay() {
    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.clear();
    printPadded("ACME Integrado Act3", 0);
    printPadded("Clasificador+Ascensor", 1);
    printPadded("Inicializando...", 2);
    printPadded("", 3);
    tLastRefresh = millis();
}

void displayUpdate() {
    uint32_t now = millis();
    if (now - tLastRefresh < LCD_REFRESH_MS) return;
    tLastRefresh = now;

    char line[LCD_COLS + 1];

    // ----- Línea 0 — última caja clasificada ------------------------------
    if (classifier_lastPiece() == 0) {
        printPadded("-- SIN CAJA --", 0);
    } else if (classifier_lastFloor() >= 0) {
        snprintf(line, sizeof(line), "CAJA#%02d ASCENSOR P%d",
                 classifier_lastPiece(), (int) classifier_lastFloor());
        printPadded(line, 0);
    } else {
        snprintf(line, sizeof(line), "CAJA#%02d ESCALERA",
                 classifier_lastPiece());
        printPadded(line, 0);
    }

    // ----- Línea 1 — estado del ascensor ----------------------------------
    int8_t fc = elevator_currentFloor();
    int8_t ft = elevator_targetFloor();
    if (ft == fc || ft < 0) {
        snprintf(line, sizeof(line), "ELEV %-9s P%d",
                 elevator_stateName(), (int) fc);
    } else {
        snprintf(line, sizeof(line), "ELEV %-9s P%d>P%d",
                 elevator_stateName(), (int) fc, (int) ft);
    }
    printPadded(line, 1);

    // ----- Línea 2 — clima (T, H, iluminación) ----------------------------
    snprintf(line, sizeof(line), "T%.1fC H%.0f%% L%.0flx",
             ctrlTemp(), ctrlHum(), ctrlLux());
    printPadded(line, 2);

    // ----- Línea 3 — contadores + actuadores + PIR ------------------------
    snprintf(line, sizeof(line), "Asc%02d Esc%02d %s %s P%d",
             classifier_totalAsc(), classifier_totalEsc(),
             labelTemp(), labelLamp(), pirRead() ? 1 : 0);
    printPadded(line, 3);
}
