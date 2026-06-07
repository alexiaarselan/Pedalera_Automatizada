// ======================================================
// PEDALERA AUTOMATIZADA - VERSION 11
// CON CONTROL PI DE RPM Y SENSOR HALL
// ======================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include "PI_Controller.h"

// ======================================================
// LCD
// ======================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================================
// PINES
// ======================================================

const int PIN_DIR           = 8;  // ← movido para liberar pin 2
const int PIN_BRAKE         = 3;

const int PIN_BOTON_START   = 4;
const int PIN_BOTON_MAS     = 5;
const int PIN_BOTON_MENOS   = 6;
const int PIN_EMERGENCIA    = 7;

const int PIN_PWM           = 9;

const int PIN_LED_VERDE     = 10;
const int PIN_LED_ROJO      = 11;

const int PIN_BOTON_RESET   = 12;

const int PIN_POT           = A0;

const int PIN_HALL          = 2;   // ← INT0, sensor Hall

// ======================================================
// CONFIGURACION
// ======================================================

const bool DIRECCION_FIJA = 1;

// ------------------------------------------------------
// SENSOR HALL Y RPM
// ------------------------------------------------------

const float PULSOS_POR_REV = 100.0f;   // ← CALIBRAR contando pulsos por vuelta manual-->LISTO 90 - 100
const float RPM_MIN        = 25.0f;  // ← ajustar según necesidad clínica
const float RPM_MAX        = 75.0f;  // ← ajustar según necesidad clínica

// ------------------------------------------------------
// CONTROLADOR PI
// sampleTime coincide con DELAY_RAMPA (40ms = 0.04s)
// ------------------------------------------------------

const float KP          = 1.0f;   // ← CALIBRAR
const float KI          = 0.5f;   // ← CALIBRAR
const float SAMPLE_TIME = 0.04f;
const float DEAD_ZONE   = 1.0f;  // ← PWM mínimo para vencer fricción, LISTO (dio 8, pruebo con 10)
const float PWM_MAX_PI  = 70.0f; // Probar

// ------------------------------------------------------
// ARRANQUE ASISTIDO
// ------------------------------------------------------

const int PWM_ARRANQUE    = 35;
const int TIEMPO_ARRANQUE = 80;

// ------------------------------------------------------
// RAMPA (solo para bajada en pausa/stop)
// ------------------------------------------------------

const int PASO_RAMPA  = 1;
const int DELAY_RAMPA = 40;

// ------------------------------------------------------
// ZONA MUERTA POTENCIOMETRO
// ------------------------------------------------------

const int UMBRAL_POTE_MINIMO = 110;

// Duración mensaje temporal
const unsigned long DURACION_MENSAJE = 1500;

// ======================================================
// ESTADOS
// ======================================================

enum Estado { ESPERA_SEGURA, FUNCIONANDO, PAUSA, EMERGENCIA };

Estado estadoActual = ESPERA_SEGURA;

// ======================================================
// VARIABLES
// ======================================================

int minutosSeteados = 0;

unsigned long tiempoRestante = 0;
unsigned long ultimoSegundo  = 0;

// RPM
volatile unsigned long contadorPulsos = 0;  // ISR
float rpmMedidas    = 0.0f;
float rpmObjetivo   = 0.0f;

// PWM
int velocidadAplicada = 0;
bool motorDetenido    = true;

unsigned long ultimoCambioPWM = 0;

// LCD
bool mostrarMensajeTiempo    = false;
unsigned long mensajeTemporalHasta = 0;

char cacheLinea0[17] = "";
char cacheLinea1[17] = "";

// Anti-rebote
bool ultimoStart      = HIGH;
bool ultimoMas        = HIGH;
bool ultimoMenos      = HIGH;
bool ultimoEmergencia = HIGH;
bool ultimoReset      = HIGH;

bool esperandoConfirmacionReset = false;

// ======================================================
// CONTROLADOR PI
// ======================================================

PI_Controller pi(KP, KI, SAMPLE_TIME, DEAD_ZONE, PWM_MAX_PI);

// ======================================================
// ISR — SENSOR HALL
// ======================================================

void contarPulso()
{
    contadorPulsos++;
}

// ======================================================
// PROTOTIPOS
// ======================================================

void verificarEmergencia();
bool potenciometroEnMinimo();
void actualizarLEDs(bool poteMinimo);
void manejarBotonesTiempo(bool poteMinimo);
void manejarStartStop(bool poteMinimo);
void manejarReset();
void ejecutarReset();
void manejarMotor();
void manejarTimer();
void actualizarLCD();
void escribirLCD(int fila, const char* texto);
void debugSerial();

// ======================================================
// SETUP
// ======================================================

void setup()
{
    pinMode(PIN_DIR,         OUTPUT);
    pinMode(PIN_BRAKE,       OUTPUT);
    pinMode(PIN_PWM,         OUTPUT);
    pinMode(PIN_BOTON_START, INPUT_PULLUP);
    pinMode(PIN_BOTON_MAS,   INPUT_PULLUP);
    pinMode(PIN_BOTON_MENOS, INPUT_PULLUP);
    pinMode(PIN_EMERGENCIA,  INPUT_PULLUP);
    pinMode(PIN_BOTON_RESET, INPUT_PULLUP);
    pinMode(PIN_LED_VERDE,   OUTPUT);
    pinMode(PIN_LED_ROJO,    OUTPUT);
    pinMode(PIN_HALL,        INPUT);

    digitalWrite(PIN_DIR,   DIRECCION_FIJA);
    digitalWrite(PIN_BRAKE, LOW);
    analogWrite(PIN_PWM, 0);

    attachInterrupt(digitalPinToInterrupt(PIN_HALL), contarPulso, RISING);

    Serial.begin(9600);
    lcd.init();
    lcd.backlight();
    actualizarLCD();
}

// ======================================================
// LOOP
// ======================================================

void loop()
{
    verificarEmergencia();

    bool poteMinimo = potenciometroEnMinimo();

    actualizarLEDs(poteMinimo);
    manejarBotonesTiempo(poteMinimo);
    manejarStartStop(poteMinimo);
    manejarReset();
    manejarMotor();
    manejarTimer();
    actualizarLCD();
    debugSerial();
}

// ======================================================
// EMERGENCIA
// ======================================================

void verificarEmergencia()
{
    bool lectura = digitalRead(PIN_EMERGENCIA);

    if (ultimoEmergencia == HIGH && lectura == LOW)
    {
        estadoActual = EMERGENCIA;

        analogWrite(PIN_PWM, 0);
        digitalWrite(PIN_BRAKE, HIGH);
        pi.reset();

        digitalWrite(PIN_LED_VERDE, LOW);
        digitalWrite(PIN_LED_ROJO,  HIGH);

        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("BLOQUEO TOTAL");
        lcd.setCursor(0, 1); lcd.print("APAGUE EL EQUIPO");

        Serial.println("!!! EMERGENCIA !!!");

        while (true) {}
    }

    ultimoEmergencia = lectura;
}

// ======================================================
// POTENCIOMETRO
// ======================================================

bool potenciometroEnMinimo()
{
    return (analogRead(PIN_POT) < UMBRAL_POTE_MINIMO);
}

// ======================================================
// LEDS — sin cambios
// ======================================================

void actualizarLEDs(bool poteMinimo)
{
    if (esperandoConfirmacionReset)
    {
        digitalWrite(PIN_LED_VERDE, poteMinimo ? HIGH : LOW);
        digitalWrite(PIN_LED_ROJO,  poteMinimo ? LOW  : HIGH);
        return;
    }

    if (estadoActual == FUNCIONANDO)
    {
        digitalWrite(PIN_LED_VERDE, HIGH);
        digitalWrite(PIN_LED_ROJO,  LOW);
        return;
    }

    digitalWrite(PIN_LED_VERDE, poteMinimo ? HIGH : LOW);
    digitalWrite(PIN_LED_ROJO,  poteMinimo ? LOW  : HIGH);
}

// ======================================================
// BOTONES TIEMPO — sin cambios
// ======================================================

void manejarBotonesTiempo(bool poteMinimo)
{
    if (estadoActual == FUNCIONANDO || !poteMinimo) return;

    bool lecturaMas   = digitalRead(PIN_BOTON_MAS);
    bool lecturaMenos = digitalRead(PIN_BOTON_MENOS);

    if (ultimoMas == HIGH && lecturaMas == LOW)
        if (minutosSeteados < 10) { minutosSeteados++; tiempoRestante = minutosSeteados * 60UL; }

    if (ultimoMenos == HIGH && lecturaMenos == LOW)
        if (minutosSeteados > 0)  { minutosSeteados--; tiempoRestante = minutosSeteados * 60UL; }

    ultimoMas   = lecturaMas;
    ultimoMenos = lecturaMenos;
}

// ======================================================
// START / STOP — sin cambios lógicos
// ======================================================

void manejarStartStop(bool poteMinimo)
{
    bool lecturaStart = digitalRead(PIN_BOTON_START);

    if (ultimoStart == HIGH && lecturaStart == LOW)
    {
        if (esperandoConfirmacionReset)
        {
            if (poteMinimo)
            {
                esperandoConfirmacionReset = false;
                estadoActual = FUNCIONANDO;
                digitalWrite(PIN_BRAKE, LOW);
                ultimoSegundo = millis();
            }
        }
        else if (estadoActual == ESPERA_SEGURA)
        {
            if (poteMinimo && tiempoRestante > 0)
            {
                estadoActual = FUNCIONANDO;
                digitalWrite(PIN_BRAKE, LOW);
                ultimoSegundo = millis();
            }
            else if (poteMinimo && tiempoRestante == 0)
            {
                mostrarMensajeTiempo     = true;
                mensajeTemporalHasta     = millis() + DURACION_MENSAJE;
            }
        }
        else if (estadoActual == FUNCIONANDO)
        {
            estadoActual      = PAUSA;
            velocidadAplicada = 0;
            motorDetenido     = true;
            pi.reset();        // ← resetear PI al pausar
        }
        else if (estadoActual == PAUSA && poteMinimo)
        {
            estadoActual = FUNCIONANDO;
            digitalWrite(PIN_BRAKE, LOW);
            ultimoSegundo = millis();
        }
    }

    ultimoStart = lecturaStart;
}

// ======================================================
// RESET
// ======================================================

void ejecutarReset()
{
    analogWrite(PIN_PWM, 0);
    estadoActual              = ESPERA_SEGURA;
    minutosSeteados           = 0;
    tiempoRestante            = 0;
    rpmObjetivo               = 0;
    velocidadAplicada         = 0;
    motorDetenido             = true;
    esperandoConfirmacionReset = false;
    mostrarMensajeTiempo      = false;
    cacheLinea0[0]            = '\0';
    cacheLinea1[0]            = '\0';
    pi.reset();                // ← resetear PI
}

void manejarReset()
{
    bool lecturaReset = digitalRead(PIN_BOTON_RESET);

    if (ultimoReset == HIGH && lecturaReset == LOW)
    {
        if (!esperandoConfirmacionReset &&
            (estadoActual == FUNCIONANDO || estadoActual == PAUSA))
        {
            estadoActual      = PAUSA;
            analogWrite(PIN_PWM, 0);
            velocidadAplicada = 0;
            motorDetenido     = true;
            esperandoConfirmacionReset = true;
            pi.reset();
            cacheLinea0[0] = '\0';
            cacheLinea1[0] = '\0';
        }
        else if (esperandoConfirmacionReset)
        {
            ejecutarReset();
        }
    }

    ultimoReset = lecturaReset;
}

// ======================================================
// MOTOR — ahora con PI de RPM
// ======================================================

void manejarMotor()
{
    if (estadoActual != FUNCIONANDO)
    {
        analogWrite(PIN_PWM, 0);
        velocidadAplicada = 0;
        motorDetenido     = true;
        return;
    }

    // --------------------------------------------------
    // TIMESTAMP CONSISTENTE
    // --------------------------------------------------
    unsigned long ahora = millis();
    if (ahora - ultimoCambioPWM < (unsigned long)DELAY_RAMPA) return;

    unsigned long dt = ahora - ultimoCambioPWM;
    ultimoCambioPWM  = ahora;   // ← corregido: antes del cálculo

    // --------------------------------------------------
    // MEDIR RPM
    // --------------------------------------------------
    noInterrupts();
    unsigned long pulsos = contadorPulsos;
    contadorPulsos = 0;
    interrupts();

    rpmMedidas = (pulsos / PULSOS_POR_REV) * (60000.0f / (float)dt);

    // --------------------------------------------------
    // SETPOINT DESDE POTENCIOMETRO
    // --------------------------------------------------
    int lecturaPot = analogRead(PIN_POT);

    if (lecturaPot < UMBRAL_POTE_MINIMO)
        rpmObjetivo = 0.0f;
    else
        rpmObjetivo = (float)map(lecturaPot, UMBRAL_POTE_MINIMO, 1023,
                                 (int)RPM_MIN, (int)RPM_MAX);

    // --------------------------------------------------
    // ZONA MUERTA: pot en mínimo
    // --------------------------------------------------
    if (rpmObjetivo == 0.0f)
    {
        analogWrite(PIN_PWM, 0);
        velocidadAplicada = 0;
        motorDetenido     = true;
        pi.reset();
        return;
    }

    // --------------------------------------------------
    // ARRANQUE ASISTIDO — sin delay(), con timer no bloqueante
    // --------------------------------------------------
    static unsigned long inicioArranque = 0;
    static bool arrancando = false;

    if (motorDetenido && !arrancando)
    {
        // Primer ciclo: lanzar pulso de arranque
        digitalWrite(PIN_BRAKE, LOW);
        analogWrite(PIN_PWM, PWM_ARRANQUE);
        inicioArranque    = ahora;
        arrancando        = true;
        velocidadAplicada = PWM_ARRANQUE;
        return;   // esperar al próximo ciclo
    }

    if (arrancando)
    {
        if (ahora - inicioArranque < (unsigned long)TIEMPO_ARRANQUE)
        {
            // Todavía en pulso de arranque, mantener PWM y salir
            analogWrite(PIN_PWM, PWM_ARRANQUE);
            return;
        }
        else
        {
            // Arranque terminado, pasar al PI
            arrancando    = false;
            motorDetenido = false;
        }
    }

    // --------------------------------------------------
    // SALIDA PI
    // --------------------------------------------------
    velocidadAplicada = (int) pi.output(rpmMedidas, rpmObjetivo);
    velocidadAplicada = constrain(velocidadAplicada, 0, 255);

    analogWrite(PIN_PWM, velocidadAplicada);
}
// ======================================================
// TIMER — sin cambios
// ======================================================

void manejarTimer()
{
    if (estadoActual != FUNCIONANDO) return;

    if (millis() - ultimoSegundo >= 1000)
    {
        ultimoSegundo = millis();

        if (tiempoRestante > 0) tiempoRestante--;

        if (tiempoRestante == 0)
        {
            estadoActual      = ESPERA_SEGURA;
            analogWrite(PIN_PWM, 0);
            velocidadAplicada = 0;
            motorDetenido     = true;
            minutosSeteados   = 0;
            pi.reset();
        }
    }
}

// ======================================================
// LCD — agrega RPM medidas
// ======================================================

void escribirLCD(int fila, const char* texto)
{
    char* cache = (fila == 0) ? cacheLinea0 : cacheLinea1;

    if (strcmp(cache, texto) != 0)
    {
        strncpy(cache, texto, 16);
        cache[16] = '\0';
        lcd.setCursor(0, fila);
        char buffer[17];
        snprintf(buffer, sizeof(buffer), "%-16s", texto);
        lcd.print(buffer);
    }
}

void actualizarLCD()
{
    static unsigned long ultimoLCD = 0;
    if (millis() - ultimoLCD < 200) return;
    ultimoLCD = millis();

    if (mostrarMensajeTiempo)
    {
        if (millis() < mensajeTemporalHasta)
        {
            escribirLCD(0, "    SETEE EL");
            escribirLCD(1, "     TIEMPO ");
            return;
        }
        else
        {
            mostrarMensajeTiempo = false;
            cacheLinea0[0] = '\0';
            cacheLinea1[0] = '\0';
        }
    }

    if (esperandoConfirmacionReset)
    {
        escribirLCD(0, "   RESETEAR?");
        escribirLCD(1, "RST=SI  STA=NO");
        return;
    }

    char linea0[17];
    int minutos  = (int)(tiempoRestante / 60);
    int segundos = (int)(tiempoRestante % 60);
    snprintf(linea0, sizeof(linea0), "%02d:%02d", minutos, segundos);
    escribirLCD(0, linea0);

    char linea1[17];
    if (estadoActual == FUNCIONANDO)
    {
        if (rpmObjetivo == 0)
            snprintf(linea1, sizeof(linea1), "SET VELOCIDAD");
        else
            // ← muestra RPM reales en pantalla
            snprintf(linea1, sizeof(linea1), "%3.0f/%3.0f RPM", rpmMedidas, rpmObjetivo);
    }
    else if (estadoActual == PAUSA)
        snprintf(linea1, sizeof(linea1), "    PAUSADO");
    else
    {
        if (potenciometroEnMinimo())
            snprintf(linea1, sizeof(linea1), " SET TIEMPO");
        else
            snprintf(linea1, sizeof(linea1), "SET VEL MIN");
    }

    escribirLCD(1, linea1);
}

// ======================================================
// DEBUG SERIAL — agrega RPM
// ======================================================

void debugSerial()
{
    static unsigned long ultimoDebug = 0;
    if (millis() - ultimoDebug < 500) return;
    ultimoDebug = millis();

    Serial.print("Estado:");   Serial.print(estadoActual);
    Serial.print(" | Tiempo:"); Serial.print(tiempoRestante);
    Serial.print(" | RPM_obj:"); Serial.print(rpmObjetivo);
    Serial.print(" | RPM_med:"); Serial.print(rpmMedidas);
    Serial.print(" | PWM:");    Serial.print(velocidadAplicada);
    Serial.print(" | Pot:");    Serial.println(analogRead(PIN_POT));
}