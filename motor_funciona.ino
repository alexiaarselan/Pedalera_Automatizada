// ==========================================

// PEDALERA AUTOMATIZADA - ARRANQUE SUAVE

// CON VISUALIZACIÓN EN SERIAL PLOTTER

// ==========================================

// -------- PINES --------

const int PIN_DIR = 2;

const int PIN_PWM = 9;

const int PIN_BRAKE = 3;

const int PIN_POT = A0;

const int PIN_BOTON = 7;

// -------- CONFIGURACIÓN --------

const int VELOCIDAD_LENTA = 10;

const int VELOCIDAD_MAX = 15;

const bool DIRECCION_FIJA = 0;

// Arranque asistido

const int PWM_ARRANQUE = 20; // suficiente torque

const int TIEMPO_ARRANQUE = 120; // ms

// Rampa suave

const int PASO_RAMPA = 1;

const int DELAY_RAMPA = 40; // ms

// -------- VARIABLES --------

int velocidadObjetivo = 0;

int velocidadAplicada = 0;

bool sistemaBloqueado = false;

bool motorDetenido = true;

unsigned long ultimoCambio = 0;

// ==========================================

// SETUP

// ==========================================

void setup()

{

pinMode(PIN_DIR, OUTPUT);

pinMode(PIN_PWM, OUTPUT);

pinMode(PIN_BRAKE, OUTPUT);

pinMode(PIN_BOTON, INPUT_PULLUP);

digitalWrite(PIN_DIR, DIRECCION_FIJA);

digitalWrite(PIN_BRAKE, LOW);

Serial.begin(9600);

}

// ==========================================

// LOOP

// ==========================================

void loop()

{

// -----------------------------

// BOTÓN BRAKE DEFINITIVO

// -----------------------------

if (digitalRead(PIN_BOTON) == LOW)

{

sistemaBloqueado = true;

}

if (sistemaBloqueado)

{

analogWrite(PIN_PWM, 0);

digitalWrite(PIN_BRAKE, HIGH);

while (true)

{

}

}

// -----------------------------

// LECTURA DEL POT

// -----------------------------

int lecturaPot = analogRead(PIN_POT);

velocidadObjetivo = map(

lecturaPot,

0,

1023,

VELOCIDAD_LENTA,

VELOCIDAD_MAX

);

// -----------------------------

// ARRANQUE ASISTIDO

// -----------------------------

if (motorDetenido && velocidadObjetivo > 0)

{

analogWrite(PIN_PWM, PWM_ARRANQUE);

delay(TIEMPO_ARRANQUE);

velocidadAplicada = PWM_ARRANQUE;

motorDetenido = false;

}

// -----------------------------

// RAMPA SUAVE

// -----------------------------

if (millis() - ultimoCambio >= DELAY_RAMPA)

{

ultimoCambio = millis();

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

// -----------------------------

// APLICAR PWM

// -----------------------------

analogWrite(PIN_PWM, velocidadAplicada);

// -----------------------------

// SERIAL PLOTTER

// Escalado para mejor visualización

// -----------------------------

Serial.print("Pot:");

Serial.print(lecturaPot);

Serial.print(" Obj:");

Serial.print(velocidadObjetivo * 50);

Serial.print(" PWM:");

Serial.println(velocidadAplicada * 50);

}