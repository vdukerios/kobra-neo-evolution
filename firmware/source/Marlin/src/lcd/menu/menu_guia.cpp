/**
 * Funcion "guia_ayuda": guia para principiantes (menu "Guia y ayuda") y ayuda por
 * opcion con pulsacion larga del clic de la rueda (GUIA_AYUDA_LARGA).
 * Banderas en src/inc/valentin/guia_ayuda.h. Textos (ingles y espanol) en menu_guia_textos.h;
 * el idioma activo de la interfaz decide cual se muestra (guia_idioma()).
 */

#include "../../inc/MarlinConfigPre.h"

#if HAS_LCD_MENU && ENABLED(GUIA_AYUDA)

#include "menu_item.h"
#include "../tft/tft.h"
#include "../tft/tft_string.h"
#include "menu_guia_textos.h"

//
// Pantalla de texto comun: titulo centrado (fila 0), hasta 6 lineas de texto
// separadas por '\n' (filas 1 a 6) y un pie (fila 7). Cada fila es un lienzo de
// 30 px que se pinta completo, asi no queda nada de la pantalla anterior.
//
static void guia_dibujar(const char *titulo, const uint16_t colorTitulo, const char *texto, const char *pie) {
  char buf[40];

  tft.canvas(0, 0, TFT_WIDTH, 30);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(titulo);
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 5, colorTitulo, tft_string);

  for (uint8_t r = 1; r <= 6; r++) {
    tft.canvas(0, 30 * r, TFT_WIDTH, 30);
    tft.set_background(COLOR_BACKGROUND);
    if (texto && *texto) {
      uint8_t i = 0;
      while (*texto && *texto != '\n' && i < sizeof(buf) - 1) buf[i++] = *texto++;
      buf[i] = '\0';
      if (*texto == '\n') texto++;
      tft_string.set(buf);
      tft.add_text(10, 5, COLOR_WHITE, tft_string);
    }
  }

  tft.canvas(0, 210, TFT_WIDTH, 30);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(pie);
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 5, COLOR_GREY, tft_string);
}

//
// Guia: pantalla de una pagina. Girar la rueda cambia de pagina; clic sale.
//
static uint8_t guia_tema_sel, guia_pagina_sel;

static void guia_pantalla_pagina() {
  const GuiaTema &t = guia_temas[guia_tema_sel];
  const int16_t d = int16_t(ui.encoderPosition);
  if (d) {
    ui.encoderPosition = 0;
    // Girar mas alla de la primera o la ultima pagina tambien SALE (segunda via de salida ademas del clic)
    if (d > 0) { if (guia_pagina_sel + 1 < t.n) guia_pagina_sel++; else { ui.goto_previous_screen(); return; } }
    else if (guia_pagina_sel > 0) guia_pagina_sel--;
    else { ui.goto_previous_screen(); return; }
    ui.refresh(LCDVIEW_REDRAW_NOW);
  }
  if (ui.use_click()) { ui.goto_previous_screen(); return; }
  if (ui.should_draw()) {
    const uint8_t idioma = guia_idioma();
    char pie[56];
    sprintf_P(pie, guia_pie_pagina[idioma], (unsigned int)(guia_pagina_sel + 1), (unsigned int)t.n);
    guia_dibujar(t.titulo[idioma], COLOR_YELLOW, t.paginas[idioma][guia_pagina_sel], pie);
  }
}

static void guia_abrir(const uint8_t tema) {
  guia_tema_sel = tema;
  guia_pagina_sel = 0;
  // SIN push_current_screen(): SUBMENU_P ya apilo el menu de la guia. Un segundo push dejaba esta funcion
  // como "pantalla anterior" y al salir de la pagina volvia a ejecutarse y reabria el tema (no se podia salir).
  ui.goto_screen(guia_pantalla_pagina);
}

#define GUIA_TEMA_ITEM(N) SUBMENU_P(guia_temas[N].titulo[guia_idioma()], []{ guia_abrir(N); })

void menu_guia() {
  START_MENU();
  BACK_ITEM(MSG_BACK);
  GUIA_TEMA_ITEM(0);
  GUIA_TEMA_ITEM(1);
  GUIA_TEMA_ITEM(2);
  GUIA_TEMA_ITEM(3);
  GUIA_TEMA_ITEM(4);
  GUIA_TEMA_ITEM(5);
  GUIA_TEMA_ITEM(6);
  END_MENU();
}

#if ENABLED(GUIA_AYUDA_LARGA)

//
// Ayuda por opcion (pulsacion larga). Cada vez que un menu procesa la opcion
// seleccionada, el macro de menu_item.h llama a guia_ayuda_nota() con su texto.
// Aqui se guarda una copia (48 bytes de RAM) junto con la pantalla a la que
// pertenece, para saber a que opcion se refiere una pulsacion larga.
//
static char guia_etq[48];
static bool guia_etq_ok;
static screenFunc_t guia_etq_pantalla;
static const char *guia_txt_ayuda;
static millis_t guia_ayuda_libre_ms;   // N-09: hasta este instante los giros no cierran la ayuda

void guia_ayuda_nota(const char *p) {
  if (!p) return;
  strncpy(guia_etq, p, sizeof(guia_etq) - 1);
  guia_etq[sizeof(guia_etq) - 1] = '\0';
  guia_etq_ok = true;
  guia_etq_pantalla = ui.currentScreen;
}

void guia_ayuda_olvidar() { guia_etq_ok = false; }

// Solo hay opcion seleccionada valida si el menu actual la anoto en esta pantalla
bool guia_ayuda_disponible() { return guia_etq_ok && guia_etq_pantalla == ui.currentScreen; }

static void guia_pantalla_ayuda() {
  const bool girado = (ui.encoderPosition != 0) && ELAPSED(millis(), guia_ayuda_libre_ms);   // girar la rueda tambien cierra la ayuda
  ui.encoderPosition = 0;
  if (ui.use_click() || girado) { ui.goto_previous_screen(); return; }
  if (ui.should_draw())
    guia_dibujar(guia_etq, COLOR_YELLOW, guia_txt_ayuda, guia_pie_ayuda[guia_idioma()]);
}

// Devuelve false (sin cambiar de pantalla) si la opcion no tiene texto de ayuda
bool guia_ayuda_abrir() {
  guia_txt_ayuda = guia_buscar_ayuda(guia_etq);
  if (!guia_txt_ayuda) return false;
  ui.push_current_screen();
  guia_ayuda_libre_ms = millis() + 300;   // ignorar el giro minimo al soltar la pulsacion larga
  ui.goto_screen(guia_pantalla_ayuda);
  return true;
}

#endif // GUIA_AYUDA_LARGA

#endif // HAS_LCD_MENU && GUIA_AYUDA
