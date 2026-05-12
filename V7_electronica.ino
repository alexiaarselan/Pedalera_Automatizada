// ======================================================
// PEDALERA AUTOMATIZADA - VERSION 8
// ======================================================
// FUNCIONES:
// - Potenciometro controla velocidad
// - Timer configurable 0 a 10 min
// - LCD 16x2 I2C sin parpadeo
// - Start / Pause
// - Reset con confirmacion
// - Emergencia con bloqueo total
// - LEDs de estado (pote + estado logico)
// - Rampa suave corregida
// - Mensaje temporal sin delay()
// - Serial debug
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

const int PIN_DIR = 2;
const int PIN_BRAKE = 3;

const int PIN_BOTON_START = 4;
const int PIN_BOTON_MAS = 5;
const int PIN_BOTON_MENOS = 6;
const int PIN_EMERGENCIA = 7;

const int PIN_STOP = 8;
const int PIN_PWM = 9;

const int PIN_LED_VERDE = 10;
const int PIN_LED_ROJO = 11;

const int PIN_BOTON_RESET = 12;

const int PIN_POT = A0;

// ======================================================
// CONFIGURACION
// ======================================================

const bool DIRECCION_FIJA = 0;

const int PWM_MAX = 40;
const int PWM_MIN = 10;

const int UMBRAL_POTE_MINIMO = 50;

// Arranque suave
const int PWM_ARRANQUE = 20;
const int TIEMPO_ARRANQUE = 120;

// Rampa
const int PASO_RAMPA = 1;
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
unsigned long ultimoSegundo = 0;

int velocidadObjetivo = 0;
int velocidadAplicada = 0;

bool motorDetenido = true;

unsigned long ultimoCambioPWM = 0;

// -------------------------------------------------------
// MENSAJE TEMPORAL SIN delay()
// -------------------------------------------------------
bool mostrarMensajeTiempo = false;
unsigned long mensajeTemporalHasta = 0;

// Cache LCD - evita parpadeo
// Guardamos lo que se mostro por ultima vez
// y solo escribimos si cambio
char cacheLinea0[17] = "";
char cacheLinea1[17] = "";

// ======================================================
// ANTI REBOTE
// ======================================================

bool ultimoStart = HIGH;
bool ultimoMas = HIGH;
bool ultimoMenos = HIGH;
bool ultimoEmergencia = HIGH;
bool ultimoReset = HIGH;

// -------------------------------------------------------
// ESTADO RESET
// Cuando se pulsa reset por primera vez → PAUSA + confirmacion
// Segundo pulso → reseteo total
// -------------------------------------------------------
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
pinMode(PIN_STOP, OUTPUT);
pinMode(PIN_PWM, OUTPUT);

pinMode(PIN_BOTON_START, INPUT_PULLUP);
pinMode(PIN_BOTON_MAS, INPUT_PULLUP);
pinMode(PIN_BOTON_MENOS, INPUT_PULLUP);
pinMode(PIN_EMERGENCIA, INPUT_PULLUP);
pinMode(PIN_BOTON_RESET, INPUT_PULLUP);

pinMode(PIN_LED_VERDE, OUTPUT);
pinMode(PIN_LED_ROJO, OUTPUT);

digitalWrite(PIN_DIR, DIRECCION_FIJA);

// Brake desactivado (activo por alto)
digitalWrite(PIN_BRAKE, LOW);

// STOP activo por bajo → LOW = detenido
digitalWrite(PIN_STOP, LOW);

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

// Brake activo (activo por alto)
digitalWrite(PIN_BRAKE, HIGH);

// STOP desactivado
digitalWrite(PIN_STOP, LOW);

// LED rojo fijo de emergencia
digitalWrite(PIN_LED_VERDE, LOW);
digitalWrite(PIN_LED_ROJO, HIGH);

lcd.clear();
lcd.setCursor(0, 0);
lcd.print("BLOQUEO TOTAL");
lcd.setCursor(0, 1);
lcd.print("APAGUE EL EQUIPO");

Serial.println("!!! EMERGENCIA !!!");

// Enclavamiento total por software
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
// RESET esperando confirmacion → depende del pote
// verde si pote al minimo (puede cancelar y reanudar)
// rojo si pote no esta al minimo
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

// FUNCIONANDO → verde siempre (motor girando OK)
if (estadoActual == FUNCIONANDO)
{
digitalWrite(PIN_LED_VERDE, HIGH);
digitalWrite(PIN_LED_ROJO, LOW);
return;
}

// PAUSA → depende del pote
// verde si pote al minimo (listo para reanudar)
// rojo si pote no esta al minimo
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

// ESPERA_SEGURA → depende del pote
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
// BOTONES DE TIEMPO
// ======================================================

void manejarBotonesTiempo(bool poteMinimo)
{
// Solo se puede editar tiempo
// si el motor NO esta funcionando
// y el pote esta al minimo

if (estadoActual == FUNCIONANDO)
{
return;
}

if (!poteMinimo)
{
return;
}

bool lecturaMas = digitalRead(PIN_BOTON_MAS);
bool lecturaMenos = digitalRead(PIN_BOTON_MENOS);

// SUMAR
if (ultimoMas == HIGH && lecturaMas == LOW)
{
if (minutosSeteados < 10)
{
minutosSeteados++;
tiempoRestante = minutosSeteados * 60UL;

Serial.print("Minutos: ");
Serial.println(minutosSeteados);
}
}

// RESTAR
if (ultimoMenos == HIGH && lecturaMenos == LOW)
{
if (minutosSeteados > 0)
{
minutosSeteados--;
tiempoRestante = minutosSeteados * 60UL;

Serial.print("Minutos: ");
Serial.println(minutosSeteados);
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
// ==================================================
// CANCELA RESET → reanuda desde pausa
// ==================================================

if (esperandoConfirmacionReset)
{
if (poteMinimo)
{
esperandoConfirmacionReset = false;

estadoActual = FUNCIONANDO;

digitalWrite(PIN_BRAKE, LOW);

ultimoSegundo = millis();

// Forzar refresco LCD
cacheLinea0[0] = '\0';
cacheLinea1[0] = '\0';

Serial.println("RESET CANCELADO - REANUDADO");
}
}

// ==================================================
// INICIAR
// ==================================================

else if (estadoActual == ESPERA_SEGURA)
{
if (poteMinimo && tiempoRestante > 0)
{
estadoActual = FUNCIONANDO;

digitalWrite(PIN_BRAKE, LOW);

ultimoSegundo = millis();

Serial.println("MOTOR INICIADO");
}
else if (poteMinimo && tiempoRestante == 0)
{
mostrarMensajeTiempo = true;
mensajeTemporalHasta = millis() + DURACION_MENSAJE;

Serial.println("Sin tiempo seteado");
}
}

// ==================================================
// PAUSA
// ==================================================

else if (estadoActual == FUNCIONANDO)
{
estadoActual = PAUSA;

// Motor se detiene por PWM = 0 en manejarMotor
// PIN_STOP no se usa para control normal

velocidadAplicada = 0;
motorDetenido = true;

Serial.println("PAUSA");
}

// ==================================================
// REANUDAR
// ==================================================

else if (estadoActual == PAUSA)
{
if (poteMinimo)
{
estadoActual = FUNCIONANDO;

digitalWrite(PIN_BRAKE, LOW);

ultimoSegundo = millis();

Serial.println("REANUDADO");
}
}
}

ultimoStart = lecturaStart;
}

// ======================================================
// RESET CON CONFIRMACION
// ======================================================

void ejecutarReset()
{
// Detiene el motor via PWM
analogWrite(PIN_PWM, 0);

// Resetea todas las variables al estado inicial
estadoActual = ESPERA_SEGURA;
minutosSeteados = 0;
tiempoRestante = 0;
velocidadObjetivo = 0;
velocidadAplicada = 0;
motorDetenido = true;
esperandoConfirmacionReset = false;
mostrarMensajeTiempo = false;

// Forzar refresco de LCD
cacheLinea0[0] = '\0';
cacheLinea1[0] = '\0';

Serial.println("RESET EJECUTADO");
}

void manejarReset()
{
bool lecturaReset = digitalRead(PIN_BOTON_RESET);

if (ultimoReset == HIGH && lecturaReset == LOW)
{
// ================================================
// PRIMER PULSO
// Solo actua si esta FUNCIONANDO o en PAUSA
// ================================================

if (!esperandoConfirmacionReset &&
(estadoActual == FUNCIONANDO || estadoActual == PAUSA))
{
// Pausar el motor inmediatamente via PWM
estadoActual = PAUSA;

analogWrite(PIN_PWM, 0);

velocidadAplicada = 0;
motorDetenido = true;
esperandoConfirmacionReset = true;

// LED rojo fijo mientras espera confirmacion
digitalWrite(PIN_LED_VERDE, LOW);
digitalWrite(PIN_LED_ROJO, HIGH);

// Forzar refresco LCD para mostrar ¿RESETEAR?
cacheLinea0[0] = '\0';
cacheLinea1[0] = '\0';

Serial.println("RESET - esperando confirmacion");
}

// ================================================
// SEGUNDO PULSO → confirma reset total
// ================================================

else if (esperandoConfirmacionReset)
{
ejecutarReset();

Serial.println("RESET CONFIRMADO");
}
}

ultimoReset = lecturaReset;
}

// ======================================================
// MOTOR
// ======================================================

void manejarMotor()
{
if (estadoActual != FUNCIONANDO)
{
analogWrite(PIN_PWM, 0);

// Garantiza re-arranque consistente siempre
// que se retome desde cualquier estado no-FUNCIONANDO
motorDetenido = true;

return;
}

int lecturaPot = analogRead(PIN_POT);

// Mapeo respeta umbral minimo del pote
// Cuando el pote esta arriba del umbral mapea de PWM_MIN a PWM_MAX
// Asi el motor siempre recibe al menos PWM_MIN cuando esta en marcha
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

// ARRANQUE ASISTIDO
// Impulso inicial para vencer la inercia
// nunca supera el objetivo real
if (motorDetenido && velocidadObjetivo > 0)
{
int impulso = min(PWM_ARRANQUE, velocidadObjetivo);

analogWrite(PIN_PWM, impulso);

delay(TIEMPO_ARRANQUE);

velocidadAplicada = impulso;
motorDetenido = false;
}

// RAMPA
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

// FIN DEL TIEMPO
if (tiempoRestante == 0)
{
estadoActual = ESPERA_SEGURA;

analogWrite(PIN_PWM, 0);

velocidadAplicada = 0;
motorDetenido = true;
minutosSeteados = 0;

Serial.println("TIEMPO FINALIZADO");
}
}
}

// ======================================================
// LCD - SIN PARPADEO
// Solo escribe en una fila si el contenido cambio
// ======================================================

void escribirLCD(int fila, const char* texto)
{
char* cache = (fila == 0) ? cacheLinea0 : cacheLinea1;

if (strcmp(cache, texto) != 0)
{
// Contenido distinto → actualizar
strncpy(cache, texto, 16);
cache[16] = '\0';

lcd.setCursor(0, fila);

// Escribir exactamente 16 caracteres
// (relleno con espacios para borrar residuos)
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

// ============================
// MENSAJE TEMPORAL (sin delay)
// Tiene prioridad sobre todo
// ============================

if (mostrarMensajeTiempo)
{
if (millis() < mensajeTemporalHasta)
{
escribirLCD(0, " SETEE EL");
escribirLCD(1, " TIEMPO ");
return;
}
else
{
// Tiempo del mensaje vencido
mostrarMensajeTiempo = false;

// Forzar refresco limpiando cache
cacheLinea0[0] = '\0';
cacheLinea1[0] = '\0';
}
}

// ============================
// CONFIRMACION RESET
// Tiene prioridad sobre todo excepto mensaje temporal
// ============================

if (esperandoConfirmacionReset)
{
escribirLCD(0, " RESETEAR?");
escribirLCD(1, "RST=SI STA=NO");
return;
}

// ============================
// LINEA 1: tiempo MM:SS
// ============================

char linea0[17];
int minutos = (int)(tiempoRestante / 60);
int segundos = (int)(tiempoRestante % 60);

snprintf(linea0, sizeof(linea0), "%02d:%02d", minutos, segundos);

escribirLCD(0, linea0);

// ============================
// LINEA 2: estado
// ============================

char linea1[17];

if (estadoActual == FUNCIONANDO)
{
if (velocidadObjetivo == 0)
{
snprintf(linea1, sizeof(linea1), "SET VELOCIDAD");
}
else
{
snprintf(linea1, sizeof(linea1), " ACTIVO");
}
}
else if (estadoActual == PAUSA)
{
snprintf(linea1, sizeof(linea1), " PAUSADO");
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