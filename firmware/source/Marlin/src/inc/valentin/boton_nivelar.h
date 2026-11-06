#pragma once
// Banderas y ajustes de la funcion "boton_nivelar". Solo este archivo se edita para esta funcion.
// Cada funcion debe poder apagarse comentando su #define principal.

// Boton "Nivelar y centrar" en el menu principal:
// G28 (si hace falta) -> G29 -> subir a Z seguro -> boquilla al centro -> bajar a Z de trabajo
// -> pantalla de ajuste de Z offset (babystep). Al pulsar, se guarda con M500.
#define BOTON_NIVELAR_CENTRAR

#if ENABLED(BOTON_NIVELAR_CENTRAR)
  // Posicion de la BOQUILLA (no del sensor) en el centro de la cama 220x220.
  #define BNC_CENTRO_X "110"
  #define BNC_CENTRO_Y "110"
  // Z seguro para desplazarse (mm). Igual que Z_CLEARANCE_DEPLOY_PROBE.
  #define BNC_Z_SEGURO "10"
  // Altura intermedia y altura de trabajo final (mm).
  // El offset del sensor se CONSERVA durante todo el proceso (nunca queda un 0 guardado en la EEPROM).
  // Con el offset ya calibrado, Z=0.5 deja la boquilla a ~0.5 mm de la cama y el usuario baja con la rueda.
  #define BNC_Z_INTERMEDIA "2"
  #define BNC_Z_TRABAJO    "0.5"
  // Velocidades (mm/min)
  #define BNC_F_Z_SUBIR   "900"
  #define BNC_F_XY        "4000"
  #define BNC_F_Z_BAJAR   "240"
  #define BNC_F_Z_FINAL   "120"
#endif
