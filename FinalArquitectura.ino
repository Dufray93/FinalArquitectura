#include "AsyncTaskLib.h"
#include <LiquidCrystal.h>
#include "StateMachineLib.h"
#include <Keypad.h>
#include <EEPROM.h>
#include <Wire.h>

const int rs = 11, en = 12, d4 = 31, d5 = 32, d6 = 33, d7 = 34;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//Initialize Keypad
#define ROWS 4
#define COLS 3
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {8, 7, 6}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

//seguridad
char pass[7];
char rPass[7] = "888888";
byte contPos = 0;
int intentos = 0;

//led rgb
int pinR = 46;
int pinG = 47;
int pinB = 48;

//fotosensor
const int photocellPin = A1;
int foto_value = 0;
int luz;
void fotoresistor(void);

//temperatura
#define analogPin A0
#define beta 4090
#define resistance 10
float temp_value = 0.0;
byte tmpAlta;
byte tmpBaja;
void temperatura(void);
float tempTem = 0;

//encoder
#define clkPin 42
#define dtPin 41
#define swPin 40
//int opc = 0;
int encoder(int encoderValX, int rango);

//Variables
int sonido = 0;
const int micPin  = A3;

//Variables para EEPROM
const int dirTmpAlta = 0;
const int dirTmpBaja = 1;
const int dirLuz = 2;
const int dirSonido = 6;

//BUZZER

#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494


#define PULSADOR 23    // pulsador en pin 2
#define BUZZER_PASIVO 24
int melodia[] = {   // array con las notas de la melodia
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_F4, NOTE_E4, NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_G4, NOTE_F4, NOTE_C4, NOTE_C4, NOTE_C4, NOTE_A4, NOTE_F4, NOTE_E4, NOTE_D4, NOTE_AS4, NOTE_AS4, NOTE_A4, NOTE_F4, NOTE_G4, NOTE_F4
};

int duraciones[] = {    // array con la duracion de cada nota
  8, 8, 4, 4, 4, 2, 8, 8, 4, 4, 4, 2, 8, 8, 4, 4, 4, 4, 4, 8, 8, 4, 4, 4, 2
};

enum estado
{
  estadoA,
  estadoB,
  estadoC,
  estadoD
};

// menu
int opc;
String arrayMenu[] = {"Temp Alta", "Temp baja", "Luz", "Sonido", "Reset"};
int bandera = 0;
int valor;

byte caracter[8] = {
  0b00000,
  0b00100,
  0b00010,
  0b11111,
  0b00010,
  0b00100,
  0b00000,
  0b00000
};
void funcionEstadoA();
void funcionEstadoB();
void funcionEstadoC();
void funcionEstadoD();

// tareas asincronicas, estados
AsyncTask asyncTaskInit(10, true, funcionEstadoA);
AsyncTask asyncTaskConfig(10, true, funcionEstadoB);
AsyncTask asyncTaskRun(10, true, funcionEstadoC);
AsyncTask asyncTaskAlarm(10, true, funcionEstadoD);

enum Estado
{
  inicio = 0,
  config = 1,
  run = 2,
  alarm = 3
};

enum Entrada
{
  Reset = 0,
  Adelante = 1,
  Atras = 2,
  Unknown = 3,
};
Entrada entrada;
StateMachine stateMachine(4, 7);

void setupStateMachine() {
  // Add transitions

  //primer ciclo de estado
  stateMachine.AddTransition(inicio, config, []() {
    return entrada == Adelante;
  });

  //segundo ciclo de estado
  stateMachine.AddTransition(config, inicio, []() {
    return entrada == Atras;
  });
  stateMachine.AddTransition(config, run, []() {
    return entrada == Adelante;
  });

  //tercer ciclo de estado
  stateMachine.AddTransition(run, config, []() {
    return entrada == Atras;
  });
  stateMachine.AddTransition(run, alarm, []() {
    return entrada == Adelante;
  });

  //cuarto ciclo de estado
  stateMachine.AddTransition(alarm, config, []() {
    return entrada == Adelante;
  });

  // Add actions
  stateMachine.SetOnEntering(run, salidaA);
  stateMachine.SetOnEntering(config, salidaB);
  stateMachine.SetOnEntering(run, salidaC);
  stateMachine.SetOnEntering(alarm, salidaD);

  stateMachine.SetOnLeaving(run, []() {
    Serial.println("Saliendo de iniciar...");
  });
  stateMachine.SetOnLeaving(config, []() {
    Serial.println("Saliendo de configuracion...");
  });
  stateMachine.SetOnLeaving(run, []() {
    Serial.println("Saliendo de ejecutar...");
  });
  stateMachine.SetOnLeaving(alarm, []() {
    Serial.println("Saliendo de alarma...");
  });
}


AsyncTask asyncTaskTemp(2000, true, temperatura);
AsyncTask asyncTaskFoto(1000, true, fotoresistor);
AsyncTask asyncTaskEncod(500, true, encoder);
/*****************/
void setup()
{
  Serial.begin(9600);
  Serial.println("Inicindo maquina de estados...");
  delay(1000);
  setupStateMachine();
  Serial.println("Maquina iniciada.");
  delay(1000);
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Clave:");
  
  pinMode(clkPin, INPUT);
  pinMode(dtPin, INPUT);
  pinMode(swPin, INPUT);
  digitalWrite(swPin, HIGH);

  pinMode(swPin, INPUT_PULLUP);
  pinMode(BUZZER_PASIVO, OUTPUT);

  //pinMode(micPin, INPUT);

  iniciarValEEPROM();
  

  asyncTaskTemp.Start();
  asyncTaskFoto.Start();

  stateMachine.SetState(inicio, false, true);

}

void loop()
{
  stateMachine.Update();
  actualizarEstado();
}
void actualizarEstado() {
  int currentState = stateMachine.GetState();

  switch (currentState) {
    case inicio:
      funcionEstadoA();
      break;
    case config:
      funcionEstadoB();
      break;
    case run:
      funcionEstadoC();
      break;
    case alarm:
      funcionEstadoD();
      break;
  }
}
void salidaA() {
  asyncTaskInit.Start();
}
void salidaB() {
  asyncTaskAlarm.Stop();
  asyncTaskInit.Stop();
  asyncTaskConfig.Start();
}
void salidaC() {
  asyncTaskConfig.Stop();
  asyncTaskRun.Start();
}
void salidaD() {
  asyncTaskRun.Stop();
  asyncTaskAlarm.Start();
}

void funcionEstadoA() {
  sisSeguridad();
}
void funcionEstadoB() {
  entrada = Entrada::Unknown;
  menu();
}
void funcionEstadoC() {  
  limpiaAbajo();
  asyncTaskTemp.Update();
  delay(3000);
  asyncTaskFoto.Update();
  if(tempTem > 40){
    entrada = Entrada::Adelante;
  }
  
}
void funcionEstadoD() {
  
  buzzerAlarma();
  entrada = Entrada::Adelante;
}
//---------------------------------------------------------------------- SEGURIDAD -----------------------------
void sisSeguridad() {
  
  char key = keypad.getKey(); 
  if (key) {
    lcd.setCursor(contPos, 1);
    lcd.print(key);
    pass[contPos] = key;
    contPos++;
  }

  if (contPos == 6) {
    byte validar = 0;
    for (int i = 0; i < 6; i++) {
      if (pass[i] == rPass[i]) {
        validar++;
      }
    }

    if (validar == 6) {
      lcd.setCursor(0, 1);
      lcd.print("Clave correcta!");
      color(0, 255, 0);
      delay(1000);
      color(0, 0, 0);
      intentos = 0;
      lcd.clear();
      entrada = Entrada::Adelante;

    } else {
      claveIncorrecta();
      intentos++;
    }
    contPos = 0;
  }

  if (intentos == 3) {
    sisBloqueado();
    intentos = 0;
  }
}
void claveIncorrecta() {
  mostrar(0, 1, "Error...");
  color(255, 255, 0);
  if (intentos < 2) {
    mostrar(0, 1, "Intente de nuevo!");
    color(0, 0, 0);
  }
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Clave:");
  
}
void sisBloqueado() {
  mostrar(0, 1, "Sistema bloqueado");
  color(255, 0, 0);
  delay(5000);
  color(0, 0, 0);
  delay(2000);
}
void mostrar(int c, int f, char message[]) {
  lcd.setCursor(c, f);
  lcd.print(message);
}
void mostrarCaracter(int pos) {
  lcd.createChar(1, caracter);
  lcd.setCursor(0, pos);
  lcd.write((byte)1);
}
//------------------------------------------------------------------------ MENU-----------------------------------
void menu() {
  temperatura();
  color(0,0,0);

  if (bandera == 0) {
    opc = encoder(opc, -1);
    Serial.println(opc);
  }
  btn_encoder();

  if (opc == 0 || opc == 1) {

    if (bandera == 1) {

      if (opc == 0) {
        limpiaAbajo();
        compTemp();
        tempAlta();
        if(tempTem > 40){
          entrada = Entrada::Adelante;
        }

      } else if (opc == 1) {
        limpiaArriba();
        compTemp();
        tempBaja();
        if(tempTem > 40){
          entrada = Entrada::Adelante;
        }
      }
      limpiaAbajo();
    } else {
      limpiaArriba();
      if (opc == 0) {
        lcd.setCursor(0, 2);
        lcd.print(" ");
        mostrarCaracter(0);
      } else if (opc == 1) {
        lcd.setCursor(0, 0);
        lcd.print(" ");
        mostrarCaracter(2);
      }
      lcd.setCursor(1, 0);
      lcd.print(arrayMenu[0]);
      lcd.setCursor(1, 1);
      lcd.print(arrayMenu[1]);
    }

    // CAMBIO A PAGINA 1

  } else if (opc == 2 || opc == 3) {
    if (bandera == 1) {

      if (opc == 2) {
        limpiaAbajo();
        compTemp();
        tempBaja();
        if(tempTem > 40){
          entrada = Entrada::Adelante;
        }
      } else if (opc == 3) {
        limpiaArriba();
        lcd.setCursor(0, 0);
        mostrarCaracter(0);
        lcd.print(" Luz");
        limpiaAbajo();
        Luz();
      }

    } else {
      lcd.setCursor(0, 0);
      lcd.print("                ");

      if (opc == 2) {
        lcd.setCursor(0, 2);
        lcd.print(" ");
        mostrarCaracter(0);
      } else if (opc == 3) {
        lcd.setCursor(0, 0);
        lcd.print(" ");
        mostrarCaracter(2);
      }
      lcd.setCursor(1, 0);
      lcd.print(arrayMenu[1]);
      lcd.setCursor(1, 1);
      lcd.print("                ");
      lcd.setCursor(1, 1);
      lcd.print(arrayMenu[2]);
    }

    // CAMBIO A PAGINA 2
  } else if (opc == 4 || opc == 5) {

    if (bandera == 1) {
      if (opc == 4) {
        limpiaAbajo();
        Luz();

      } else if (opc == 5) {
        limpiaArriba();

        lcd.setCursor(0, 0);
        mostrarCaracter(0);
        lcd.print("Sonido ");
        limpiaAbajo();
        //micSonido();
      }

    } else {
      limpiaArriba();
      if (opc == 4) {
        lcd.setCursor(0, 2);
        lcd.print(" ");
        mostrarCaracter(0);
      } else if (opc == 5) {
        lcd.setCursor(0, 0);
        lcd.print(" ");
        mostrarCaracter(2);
      }
      lcd.setCursor(1, 0);
      lcd.print(arrayMenu[2]);
      lcd.setCursor(1, 1);
      lcd.print("                ");
      lcd.setCursor(1, 1);
      lcd.print(arrayMenu[3]);
    }

  }
  else if (opc == 6 || opc == 7) {
    if (bandera == 1) {
      if (opc == 6) {
        limpiaAbajo();
        buzzerAlarma();

      } else if (opc == 7) {
        limpiaArriba();
        lcd.setCursor(0, 0);
        mostrarCaracter(0);
        lcd.print("Reset");
        lcd.setCursor(0, 1);
        lcd.print("  COMPLETADO!");
        reset();
      }

    } else {
      limpiaArriba();
      if (opc == 6) {
        lcd.setCursor(0, 2);
        lcd.print(" ");
        mostrarCaracter(0);
      } else if (opc == 7) {
        lcd.setCursor(0, 0);
        lcd.print(" ");
        mostrarCaracter(2);
      }
      lcd.setCursor(1, 0);
      lcd.print(arrayMenu[3]);
      lcd.setCursor(1, 1);
      lcd.print("                ");
      lcd.setCursor(1, 1);
      lcd.print(arrayMenu[4]);
    }
  }
}
// ------------------------------------------------------------------ FUNCIONES SENSORES--------------------------------
void tempAlta() {
  
  valor = tmpAlta;
  valor = encoder(valor, 0);
  lcd.setCursor(12, 0);
  lcd.print( valor );
  lcd.setCursor(14, 0);
  lcd.print(" C");
  tmpAlta = valor;
  EEPROM.write(dirTmpAlta, tmpAlta);
}
void tempBaja() {
  lcd.setCursor(0, 0);
  mostrarCaracter(0);
  lcd.print(" Temp Baja ");
  valor = tmpBaja;
  valor = encoder(valor, 1);
  lcd.setCursor(12, 0);
  lcd.print( valor );
  lcd.setCursor(14, 0);
  lcd.print("C");
  tmpBaja = valor;
  EEPROM.write(dirTmpBaja, tmpBaja);
}
void Luz() {
  valor = luz;
  valor = encoder(valor, 2);
  lcd.setCursor(6, 0);
  lcd.print( valor );
  lcd.setCursor(9, 0);
  lcd.print(" lm");
  luz = valor;
  EEPROM.put(dirLuz, luz);
}
void Sonido() {
  valor = sonido;
  valor = encoder(valor, 3);
  lcd.setCursor(9, 0);
  lcd.print( valor );
  lcd.setCursor(11, 0);
  lcd.print(" dB");
  sonido = valor;
  EEPROM.write(dirSonido, sonido);
}
void reset() {
  tmpAlta = 25;
  EEPROM.write(dirTmpAlta, tmpAlta);
  tmpBaja = 18;
  EEPROM.write(dirTmpBaja, tmpBaja);
  luz = 300;
  EEPROM.put(dirLuz, luz);
  sonido = 30;
  EEPROM.write(dirSonido, Sonido);
}
void iniciarValEEPROM() {
  tmpAlta = EEPROM.read(dirTmpAlta);
  tmpBaja = EEPROM.read(dirTmpBaja);
  luz = EEPROM.get(dirLuz, luz);
  sonido = EEPROM.read(dirSonido);
}
//---------------------------------------------------------------------- SENSORES---------------------------
void fotoresistor(void) {

  foto_value = analogRead(photocellPin);
  lcd.setCursor(0, 0);
  lcd.print("Photocell:");
  lcd.setCursor(11, 0);
  lcd.print(foto_value);

  if (foto_value > 700) {
    color(0, 255, 255);
    delay(500);
    color(0, 0, 0);
  } else {
    color(0, 255, 0);
    delay(500);
    color(0, 0, 0);
  }
  delay(1000);
  lcd.setCursor(11, 0);
  lcd.print(" ");
}
void temperatura(void) {

  long a = 1023 - analogRead(analogPin);
  float temp_value = beta / (log((1025.0 * 10 / a - 10) / 10) + beta / 298.0) - 273.0;
  float tempF = temp_value + 273.15;
  tempTem = temp_value;
  /*
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temp_value);
  lcd.print(" C");
  if (temp_value > 30) { // 25
    color(0, 255, 255);

  } else if (temp_value < 27) { //18
    color(0, 0, 255);
 
  } else if (temp_value < 30 && temp_value > 27) {
    color(0, 255, 0);

  }

  delay(200);
  */
}
void compTemp(){
  lcd.setCursor(0, 1);
  lcd.print("Temp Act: ");
  lcd.print(tempTem);
  lcd.print(" C");
  if (tempTem > 30) { // 25
    color(0, 255, 255);
  } else if (tempTem < 27) { //18
    color(0, 0, 255);
  } else if (tempTem < 30 && tempTem > 27) {
    color(0, 255, 0);
  }
}
//*********************************************************************** ENCODER -------------------------------------------
void btn_encoder() {

  if (digitalRead(swPin) == LOW) {
    if (bandera == 0) {
      bandera = 1;
    } else {
      bandera = 0;
    }
  }
}
int encoder(int encoderValX, int rango) {
  //int encoderValX;
  int change = getEncoderTurn();
  encoderValX = encoderValX + change;

  if (rango == -1) {
    if (encoderValX >= 8) {
      encoderValX = 7;
    } else if (encoderValX <= -1) {
      encoderValX = 0;
    }

  } else if (rango == 0) {
    if (encoderValX >= 51) {
      encoderValX = 50;
    } else if (encoderValX <= 24) {
      encoderValX = 25;
    }
  } else if (rango == 1) {
    if (encoderValX >= 19) {
      encoderValX = 18;
    } else if (encoderValX <= -1) {
      encoderValX = 0;
    }
  } else if (rango == 2) {
    if (encoderValX >= 301) {
      encoderValX = 300;
    } else if (encoderValX <= -1) {
      encoderValX = 0;
    }
  } else if (rango == 3) {
    if (encoderValX >= 31) {
      encoderValX = 30;
    } else if (encoderValX <= -1) {
      encoderValX = 0;
    }
  }

  return encoderValX;
}
int getEncoderTurn(void) {
  static int oldA = HIGH;
  static int oldB = HIGH;
  int result = 0;
  int newA = digitalRead(clkPin);
  int newB = digitalRead(dtPin);
  if (newA != oldA || newB != oldB)
  {
    if (oldA == HIGH && newA == LOW)
    {
      result = (oldB * 2 - 1);
    }
  }
  oldA = newA;
  oldB = newB;
  return result;
}
void buzzerAlarma() {
  if (digitalRead(swPin) == LOW) {   // si se ha presionadl el pulsador
    for (int i = 0; i < 25; i++) {      // bucle repite 25 veces
      int duracion = 1000 / duraciones[i];    // duracion de la nota en milisegundos
      tone(BUZZER_PASIVO, melodia[i], duracion);  // ejecuta el tono con la duracion
      int pausa = duracion * 1.30;      // calcula pausa
      delay(pausa);         // demora con valor de pausa
      noTone(BUZZER_PASIVO);        // detiene reproduccion de tono
      //Serial.println(swPin);
    }
  }
}
void micSonido( ) {
  
  sonido = analogRead(micPin);
  Serial.print(F("mic val ")); 
  Serial.println(sonido);
  if (sonido > 600) {
    mostrar(1,0, sonido);
  }
}
void limpiaArriba() {
  lcd.setCursor(0, 0);
  lcd.print("                 ");
}
void limpiaAbajo() {
  lcd.setCursor(0, 1);
  lcd.print("                 ");
}
// Rojo 255,0,0 Amarillo 255,255,0 verde 0,255,0
void color(int R, int G, int B) {
  analogWrite(pinR, R);
  analogWrite(pinG, G);
  analogWrite(pinB, B);
}
