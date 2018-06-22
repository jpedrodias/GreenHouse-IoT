/* Project Green House IoT - A Internet das Coisas
   data: 2017/18 @ Colégio Atlântico, Portugal
   Hardware version: 003
*/

#include <Wire.h>
#define HW 1
#define SW 3
#define app_name "Project GreenHouse IoT - Relay Control & i2c Slave"

#define I2Caddress 8   // Endereço deste Slave 
//#define DEBUG_PRINT  // comentado desliga os "prints" de debug
#define ENABLE_pin 6   // Ativa a Board de Relés OU usar Pullup resistors
#define DELAY 1000     // Pausa entre Loops

#define OFF 1   // Lógica da Board de Relés
#define ON 0    // Trucar 1 e 0 dependendo da board

#define AUTO 0  // Estado Automático
#define OPEN 1  // ESTADO Abrir
#define CLOSE 2 // ESTADO Fechar

unsigned int relays_pins[4]   = {9, 10, 11, 12}; // Ligação aos Relés
unsigned int buttons_pins[4]  = {2,  3,  4,  5}; // Ligação aos Interruptores
unsigned int buttons_value[4] = {OFF, OFF, OFF, OFF};    // Estado dos Interruptores
unsigned int relays_value[4]  = {OFF, OFF, OFF, OFF};

unsigned int STATUS_Current[2] = {AUTO, AUTO}; // Estado actual dos dois Relés
unsigned int STATUS_Previus[2] = {AUTO, AUTO}; // Estado dos Relés no Loop anterior
unsigned int INTERNO[2] = {AUTO, AUTO};          // Estado dos pedidos pelos botões
volatile unsigned int EXTERNO[2] = {AUTO, AUTO}; // Estado dos pedidos por I2C


void setup() {

  // Configura as Saída para os Relés e desliga-os (dependente da "lógica" da board)
  for (int i = 0; i < 4; i++) {
    pinMode(relays_pins[i], INPUT_PULLUP);
    pinMode(relays_pins[i], OUTPUT);
    digitalWrite(relays_pins[i], OFF);
  }


  // Configura as Entradas dos Switchs (Botões). Todos em PULLUP com 10K ohms
  for (int i = 0; i < 4; i++) {
    pinMode(buttons_pins[i], INPUT_PULLUP);
  }

  #ifdef ENABLE_pin
    // Importante: Ativar a board de relés, só depois de inicializados os relays
    pinMode(ENABLE_pin, OUTPUT);
    digitalWrite(ENABLE_pin, HIGH);
  #endif

  #ifdef I2Caddress
    // Inicializa a ligação por I2C
    Wire.begin(I2Caddress);
    Wire.onReceive(receiveCallback);
    Wire.onRequest(requestCallback);
  #endif
  
  //#ifdef DEBUG_PRINT
    Serial.begin(9600);
    Serial.println(app_name);
  //#endif

}


void loop() {

  // Faz leitura dos valores que estão nos Swiths
  for (int i = 0; i < 4; i++) {
    buttons_value[i] = digitalRead(buttons_pins[i]);
  }

  // Decisão em função do valores dos botões (LOCAL)
  for (int i = 0; i < 2; i++) {
    if ((buttons_value[2 * i] == OFF) and (buttons_value[2 * i + 1] == OFF)) {
      INTERNO[i] = AUTO;
    }
    if ((buttons_value[2 * i] == ON ) and (buttons_value[2 * i + 1] == OFF)) {
      INTERNO[i] = OPEN;
    }
    if ((buttons_value[2 * i] == OFF) and (buttons_value[2 * i + 1] == ON )) {
      INTERNO[i] = CLOSE;
    }
    if ((buttons_value[2 * i] == ON ) and (buttons_value[2 * i + 1] == ON )) {
      INTERNO[i] = CLOSE; // Nunca podem estar o dois ON
    }
  }

  // Se os valores LOCAL forem AUTO, então aceita os valores EXTERNOS
  for (int i = 0; i < 2; i++) {
    if (INTERNO[i] == AUTO) {
      STATUS_Current[i] = EXTERNO[i];
    }
    else {
      STATUS_Current[i] = INTERNO[i];
    }
  }

  // Desligar os "dois" Relés quando é necessário mudar de direção. Problema apenas se os motores forem AC.
  for (int i = 0; i < 2; i++) {
    if (STATUS_Current[i] != STATUS_Previus[i]) {
      digitalWrite(relays_pins[2 * i], OFF); digitalWrite(relays_pins[2 * i + 1], OFF);
      #ifdef DEBUG_PRINT
        Serial.println("Mudança de Estado");
      #endif
      delay(DELAY / 2); // Pausa necessária para a alavanca do Relé conseguir desarmar. Senão, dá-se curto-circuito.
    }
  }

  // Preparar saída
  for (int i = 0; i < 2; i++) {
    if (STATUS_Current[i] == OPEN ) {
      relays_value[2 * i] = OFF;
      relays_value[2 * i + 1] = ON;
    }
    if (STATUS_Current[i] == CLOSE) {
      relays_value[2 * i] = ON;
      relays_value[2 * i + 1] = OFF;
    }
    if (STATUS_Current[i] == AUTO ) {
      relays_value[2 * i] = OFF;
      relays_value[2 * i + 1] = OFF;
    }
  }

  // Escreve os valores nos Relays
  for (int i = 0; i < 4; i++) { digitalWrite(relays_pins[i], relays_value[i]); }

  #ifdef DEBUG_PRINT
    Serial.print(" B: "); 
    for (int i = 0; i < 4; i++) {
      Serial.print(buttons_value[i]);
    }
    Serial.print(" R: ");
    for (int i = 0; i < 4; i++) {
      Serial.print(relays_value[i]);
    }
    Serial.print(" INT: ");
    for (int i = 0; i < 2; i++) {
      Serial.print(INTERNO[i]);
    }
    Serial.print(" EXT: ");
    for (int i = 0; i < 2; i++) {
      Serial.print(EXTERNO[i]);
    }
    Serial.print(" Status: ");
    for (int i = 0; i < 2; i++) {
      Serial.print(STATUS_Current[i]);
    }
    Serial.print(" Previus: ");
    for (int i = 0; i < 2; i++) {
      Serial.print(STATUS_Previus[i]);
    }
    Serial.println();
  #endif

  // Memorizar estado anterior, para ser usado na detecção de mudanças de direção.
  for (int i = 0; i < 2; i++) {
    STATUS_Previus[i] = STATUS_Current[i];
  }
  delay(DELAY);
}

// I2C Callback para tratar os dados recebidos do mestre (master).
void receiveCallback(int bytes) {
  //Se tiver dados recebidos
  if (Wire.available() != 0) {
    String data_in = "";  //Dado recebido do mestre
    for (int i = 0; i < bytes; i++) {
      data_in += (char)Wire.read();
    }
    
    #ifdef DEBUG_PRINT
        Serial.print("Recebido: ");
        Serial.println(data_in);
    #endif

    if (data_in == "O*")      { EXTERNO[0] = OPEN ; EXTERNO[1] = OPEN;  }
    else if (data_in == "C*") { EXTERNO[0] = CLOSE; EXTERNO[1] = CLOSE; }
    else if (data_in == "L*") { EXTERNO[0] = AUTO ; EXTERNO[1] = AUTO;  }
    else if (data_in == "O1") { EXTERNO[0] = OPEN ; }
    else if (data_in == "C1") { EXTERNO[0] = CLOSE; }
    else if (data_in == "L1") { EXTERNO[0] = AUTO ; }
    else if (data_in == "O2") { EXTERNO[1] = OPEN ; }
    else if (data_in == "C2") { EXTERNO[1] = CLOSE; }
    else if (data_in == "L2") { EXTERNO[1] = AUTO ; }
  }
}

// I2C Callback para tratar os pedidos de dados recebidos do mestre (master).
void requestCallback(int bytes) {
  //Devolve para o mestre os valores dos relays e switchs
  for (int i = 0; i < 2; i++) { Wire.write(INTERNO[i]); }
  for (int i = 0; i < 2; i++) { Wire.write(EXTERNO[i]); }
  for (int i = 0; i < 2; i++) { Wire.write(STATUS_Current[i]); }
}

