// ======================================================
// PEDALERA AUTOMATIZADA - VERSION 10
// ======================================================
// CAMBIOS IMPORTANTES:
//
// - Zona muerta REAL del potenciometro
// - Sin pico peligroso al arrancar
// - Arranque asistido SOLO cuando el usuario
//   realmente pide movimiento
// - PWM minimo aumentado para que el motor
//   arranque sin empujon
// ======================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

// ======================================================
// LCD
// ======================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================================
// PINES
// ======================================================

const int PIN_DIR           = 2;
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

// ======================================================
// CONFIGURACION
// ======================================================

const bool DIRECCION_FIJA = 0;

// ------------------------------------------------------
// PWM
// ------------------------------------------------------

const int PWM_MAX = 40;
const int PWM_MIN = 20;

// ------------------------------------------------------
// ZONA MUERTA REAL
// ------------------------------------------------------

const int UMBRAL_POTE_MINIMO = 110;

// ------------------------------------------------------
// ARRANQUE ASISTIDO
// ------------------------------------------------------

const int PWM_ARRANQUE    = 32;
const int TIEMPO_ARRANQUE = 80;

// ------------------------------------------------------
// RAMPA
// ------------------------------------------------------

const int PASO_RAMPA  = 1;
const int DELAY_RAMPA = 40;

// Duracion mensaje temporal (ms)
const unsigned long DURACION_MENSAJE = 1500;

// ======================================================
// ESTADOS
// ======================================================

enum Estado
{
    ESPERA_SEGURA,
    FUNCIONANDO,
    PAUSA,
    EMERGENCIA
};

Estado estadoActual = ESPERA_SEGURA;

// ======================================================
// VARIABLES
// ======================================================

int minutosSeteados = 0;

unsigned long tiempoRestante = 0;
unsigned long ultimoSegundo  = 0;

int velocidadObjetivo = 0;
int velocidadAplicada = 0;

bool motorDetenido = true;

unsigned long ultimoCambioPWM = 0;

// -------------------------------------------------------
// MENSAJE TEMPORAL SIN delay()
// -------------------------------------------------------

bool mostrarMensajeTiempo = false;
unsigned long mensajeTemporalHasta = 0;

// -------------------------------------------------------
// CACHE LCD
// -------------------------------------------------------

char cacheLinea0[17] = "";
char cacheLinea1[17] = "";

// ======================================================
// ANTI REBOTE
// ======================================================

bool ultimoStart      = HIGH;
bool ultimoMas        = HIGH;
bool ultimoMenos      = HIGH;
bool ultimoEmergencia = HIGH;
bool ultimoReset      = HIGH;

// ======================================================
// RESET
// ======================================================

bool esperandoConfirmacionReset = false;

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
    pinMode(PIN_DIR, OUTPUT);
    pinMode(PIN_BRAKE, OUTPUT);
    pinMode(PIN_PWM, OUTPUT);

    pinMode(PIN_BOTON_START, INPUT_PULLUP);
    pinMode(PIN_BOTON_MAS, INPUT_PULLUP);
    pinMode(PIN_BOTON_MENOS, INPUT_PULLUP);
    pinMode(PIN_EMERGENCIA, INPUT_PULLUP);
    pinMode(PIN_BOTON_RESET, INPUT_PULLUP);

    pinMode(PIN_LED_VERDE, OUTPUT);
    pinMode(PIN_LED_ROJO, OUTPUT);

    digitalWrite(PIN_DIR, DIRECCION_FIJA);

    digitalWrite(PIN_BRAKE, LOW);

    analogWrite(PIN_PWM, 0);

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

        digitalWrite(PIN_LED_VERDE, LOW);
        digitalWrite(PIN_LED_ROJO, HIGH);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("BLOQUEO TOTAL");
        lcd.setCursor(0, 1);
        lcd.print("APAGUE EL EQUIPO");

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
    int lectura = analogRead(PIN_POT);

    return (lectura < UMBRAL_POTE_MINIMO);
}

// ======================================================
// LEDS
// ======================================================

void actualizarLEDs(bool poteMinimo)
{
    if (esperandoConfirmacionReset)
    {
        if (poteMinimo)
        {
            digitalWrite(PIN_LED_VERDE, HIGH);
            digitalWrite(PIN_LED_ROJO, LOW);
        }
        else
        {
            digitalWrite(PIN_LED_VERDE, LOW);
            digitalWrite(PIN_LED_ROJO, HIGH);
        }

        return;
    }

    if (estadoActual == FUNCIONANDO)
    {
        digitalWrite(PIN_LED_VERDE, HIGH);
        digitalWrite(PIN_LED_ROJO, LOW);

        return;
    }

    if (estadoActual == PAUSA)
    {
        if (poteMinimo)
        {
            digitalWrite(PIN_LED_VERDE, HIGH);
            digitalWrite(PIN_LED_ROJO, LOW);
        }
        else
        {
            digitalWrite(PIN_LED_VERDE, LOW);
            digitalWrite(PIN_LED_ROJO, HIGH);
        }

        return;
    }

    if (poteMinimo)
    {
        digitalWrite(PIN_LED_VERDE, HIGH);
        digitalWrite(PIN_LED_ROJO, LOW);
    }
    else
    {
        digitalWrite(PIN_LED_VERDE, LOW);
        digitalWrite(PIN_LED_ROJO, HIGH);
    }
}

// ======================================================
// BOTONES TIEMPO
// ======================================================

void manejarBotonesTiempo(bool poteMinimo)
{
    if (estadoActual == FUNCIONANDO)
    {
        return;
    }

    if (!poteMinimo)
    {
        return;
    }

    bool lecturaMas   = digitalRead(PIN_BOTON_MAS);
    bool lecturaMenos = digitalRead(PIN_BOTON_MENOS);

    if (ultimoMas == HIGH && lecturaMas == LOW)
    {
        if (minutosSeteados < 10)
        {
            minutosSeteados++;

            tiempoRestante = minutosSeteados * 60UL;
        }
    }

    if (ultimoMenos == HIGH && lecturaMenos == LOW)
    {
        if (minutosSeteados > 0)
        {
            minutosSeteados--;

            tiempoRestante = minutosSeteados * 60UL;
        }
    }

    ultimoMas = lecturaMas;
    ultimoMenos = lecturaMenos;
}

// ======================================================
// START / STOP
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
                mostrarMensajeTiempo = true;

                mensajeTemporalHasta = millis() + DURACION_MENSAJE;
            }
        }

        else if (estadoActual == FUNCIONANDO)
        {
            estadoActual = PAUSA;

            velocidadAplicada = 0;

            motorDetenido = true;
        }

        else if (estadoActual == PAUSA)
        {
            if (poteMinimo)
            {
                estadoActual = FUNCIONANDO;

                digitalWrite(PIN_BRAKE, LOW);

                ultimoSegundo = millis();
            }
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

    estadoActual = ESPERA_SEGURA;

    minutosSeteados = 0;

    tiempoRestante = 0;

    velocidadObjetivo = 0;

    velocidadAplicada = 0;

    motorDetenido = true;

    esperandoConfirmacionReset = false;

    mostrarMensajeTiempo = false;

    cacheLinea0[0] = '\0';
    cacheLinea1[0] = '\0';
}

void manejarReset()
{
    bool lecturaReset = digitalRead(PIN_BOTON_RESET);

    if (ultimoReset == HIGH && lecturaReset == LOW)
    {
        if (!esperandoConfirmacionReset &&
            (estadoActual == FUNCIONANDO || estadoActual == PAUSA))
        {
            estadoActual = PAUSA;

            analogWrite(PIN_PWM, 0);

            velocidadAplicada = 0;

            motorDetenido = true;

            esperandoConfirmacionReset = true;

            digitalWrite(PIN_LED_VERDE, LOW);
            digitalWrite(PIN_LED_ROJO, HIGH);

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
// MOTOR
// ======================================================

void manejarMotor()
{
    // --------------------------------------------------
    // SI NO ESTA FUNCIONANDO
    // --------------------------------------------------

    if (estadoActual != FUNCIONANDO)
    {
        analogWrite(PIN_PWM, 0);

        velocidadAplicada = 0;

        motorDetenido = true;

        return;
    }

    // --------------------------------------------------
    // LEER POTENCIOMETRO
    // --------------------------------------------------

    int lecturaPot = analogRead(PIN_POT);

    // --------------------------------------------------
    // ZONA MUERTA REAL
    // --------------------------------------------------

    if (lecturaPot < UMBRAL_POTE_MINIMO)
    {
        velocidadObjetivo = 0;
    }
    else
    {
        velocidadObjetivo = map(
            lecturaPot,
            UMBRAL_POTE_MINIMO,
            1023,
            PWM_MIN,
            PWM_MAX
        );
    }

    // --------------------------------------------------
    // SI ESTA EN ZONA MUERTA
    // --------------------------------------------------

    if (velocidadObjetivo == 0)
    {
        analogWrite(PIN_PWM, 0);

        velocidadAplicada = 0;

        motorDetenido = true;

        return;
    }

    // --------------------------------------------------
    // ARRANQUE ASISTIDO SEGURO
    // --------------------------------------------------

    if (motorDetenido && velocidadObjetivo > 0)
    {
        analogWrite(PIN_PWM, PWM_ARRANQUE);

        delay(TIEMPO_ARRANQUE);

        velocidadAplicada = PWM_ARRANQUE;

        motorDetenido = false;
    }

    // --------------------------------------------------
    // RAMPA SUAVE
    // --------------------------------------------------

    if (millis() - ultimoCambioPWM >= DELAY_RAMPA)
    {
        ultimoCambioPWM = millis();

        if (velocidadAplicada < velocidadObjetivo)
        {
            velocidadAplicada += PASO_RAMPA;
        }
        else if (velocidadAplicada > velocidadObjetivo)
        {
            velocidadAplicada -= PASO_RAMPA;
        }
    }

    if (velocidadAplicada < 0)
    {
        velocidadAplicada = 0;
    }

    analogWrite(PIN_PWM, velocidadAplicada);
}

// ======================================================
// TIMER
// ======================================================

void manejarTimer()
{
    if (estadoActual != FUNCIONANDO)
    {
        return;
    }

    if (millis() - ultimoSegundo >= 1000)
    {
        ultimoSegundo = millis();

        if (tiempoRestante > 0)
        {
            tiempoRestante--;
        }

        if (tiempoRestante == 0)
        {
            estadoActual = ESPERA_SEGURA;

            analogWrite(PIN_PWM, 0);

            velocidadAplicada = 0;

            motorDetenido = true;

            minutosSeteados = 0;
        }
    }
}

// ======================================================
// LCD
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

    if (millis() - ultimoLCD < 200)
    {
        return;
    }

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

    int minutos = (int)(tiempoRestante / 60);

    int segundos = (int)(tiempoRestante % 60);

    snprintf(linea0, sizeof(linea0), "%02d:%02d", minutos, segundos);

    escribirLCD(0, linea0);

    char linea1[17];

    if (estadoActual == FUNCIONANDO)
    {
        if (velocidadObjetivo == 0)
        {
            snprintf(linea1, sizeof(linea1), "SET VELOCIDAD");
        }
        else
        {
            snprintf(linea1, sizeof(linea1), "     ACTIVO");
        }
    }
    else if (estadoActual == PAUSA)
    {
        snprintf(linea1, sizeof(linea1), "    PAUSADO");
    }
    else
    {
        if (potenciometroEnMinimo())
        {
            snprintf(linea1, sizeof(linea1), " SET TIEMPO");
        }
        else
        {
            snprintf(linea1, sizeof(linea1), "SET VEL MIN");
        }
    }

    escribirLCD(1, linea1);
}

// ======================================================
// DEBUG SERIAL
// ======================================================

void debugSerial()
{
    static unsigned long ultimoDebug = 0;

    if (millis() - ultimoDebug < 500)
    {
        return;
    }

    ultimoDebug = millis();

    Serial.print("Estado: ");
    Serial.print(estadoActual);

    Serial.print(" | Tiempo: ");
    Serial.print(tiempoRestante);

    Serial.print(" | PWM: ");
    Serial.print(velocidadAplicada);

    Serial.print(" | Obj: ");
    Serial.print(velocidadObjetivo);

    Serial.print(" | Pot: ");
    Serial.println(analogRead(PIN_POT));
}

