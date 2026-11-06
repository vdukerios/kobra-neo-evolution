#pragma once
// Banderas y ajustes de la funcion "modos_motor". Solo este archivo se edita para esta funcion.
// Cada funcion debe poder apagarse comentando su #define principal.
//
// MODO DE MOTORES (Configuracion > Modo de motores). Afecta SOLO a los motores X e Y
// (el extrusor y Z no cambian). Tres modos:
//  - SILENCIOSO: stealthChop siempre, corriente como de fabrica. Es el modo de arranque.
//  - MIXTO:      stealthChop a baja velocidad y spreadCycle por encima del umbral.
//  - ALTA VELOCIDAD (antes "Fuerza"):     spreadCycle permanente (mas torque a alta velocidad/aceleracion, mas ruido).
// El modo actual NO se guarda como variable aparte: se deduce del estado de los drivers
// (stealthChop si/no, umbral hibrido y corriente), que SI se guardan en la EEPROM con M500
// (settings.cpp restaura corriente y stealthChop de X/Y al arrancar; el umbral hibrido ya
// se restauraba). Al elegir un modo se aplica en caliente y se guarda solo (M500).
#define MODOS_MOTOR

#ifdef MODOS_MOTOR
  // Umbral hibrido XY en mm/s (por encima de este valor el driver pasa a spreadCycle).
  // El campo almacenado es de 8 bits: 255 es el maximo. Con 255 mm/s solo los
  // desplazamientos mas rapidos que eso (el tope de la maquina es 300) saltarian a spreadCycle.
  // Debe coincidir con X_HYBRID_THRESHOLD / Y_HYBRID_THRESHOLD de Configuration_adv.h
  // (arranque en SILENCIOSO).
  #define MODOS_MOTOR_UMBRAL_SILENCIOSO   255   // [mm/s] "nunca" spreadCycle en la practica
  #define MODOS_MOTOR_UMBRAL_MIXTO         60   // [mm/s] silencioso por debajo, spreadCycle por encima
  #define MODOS_MOTOR_UMBRAL_FUERZA       255   // [mm/s] sin efecto (spreadCycle permanente); valor valido para guardar

  // Corriente RMS por modo para X e Y [mA].
  #define MODOS_MOTOR_CORRIENTE_SILENCIOSO 800  // como de fabrica
  #define MODOS_MOTOR_CORRIENTE_MIXTO      800
  // ATENCION: en ALTA VELOCIDAD el usuario debe vigilar la temperatura de los motores X e Y.
  // El limite del TMC2208 sin ventilacion es ~1100-1200 mA RMS; no subir de ahi.
  #define MODOS_MOTOR_CORRIENTE_FUERZA     900

  // Limites de cordura al restaurar la corriente desde la EEPROM (fuera de rango = valor de fabrica).
  #define MODOS_MOTOR_CORRIENTE_MIN        400
  #define MODOS_MOTOR_CORRIENTE_MAX       1200
#endif
