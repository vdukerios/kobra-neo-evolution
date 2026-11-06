#pragma once
// Banderas y ajustes de la funcion "guia_ayuda". Solo este archivo se edita para esta funcion.
// Cada funcion debe poder apagarse comentando su #define principal.

// Guia para principiantes: menu "Guia y ayuda" en el menu principal (menu_guia.cpp)
#define GUIA_AYUDA

// Ayuda por opcion: mantener el clic de la rueda GUIA_AYUDA_MS ms sobre una opcion de menu.
// El clic corto pasa a aplicarse al SOLTAR el boton (no al pulsarlo) en los menus.
// Requiere GUIA_AYUDA. Comentar para quitar solo la pulsacion larga.
#define GUIA_AYUDA_LARGA
#define GUIA_AYUDA_MS 800

#if BOTH(GUIA_AYUDA, GUIA_AYUDA_LARGA)
  void guia_ayuda_nota(const char *plabel);   // la opcion seleccionada de un menu se anota aqui
  void guia_ayuda_olvidar();                  // al cambiar de pantalla
  bool guia_ayuda_disponible();               // hay opcion anotada en la pantalla actual
  bool guia_ayuda_abrir();                    // abre la ayuda; false si la opcion no tiene
  #define GUIA_AYUDA_NOTA(COND, P) do{ if (COND) guia_ayuda_nota(P); }while(0)
  #define GUIA_AYUDA_OLVIDAR(COND) do{ if (COND) guia_ayuda_olvidar(); }while(0)   // seleccion sobre un elemento sin anotar
#else
  #define GUIA_AYUDA_NOTA(COND, P) NOOP
  #define GUIA_AYUDA_OLVIDAR(COND) NOOP
#endif
