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

static const uint16_t GUIA_TEXT_WIDTH = 284;

#if defined(KNE_UI_CARD)
  #define GUIA_CARD_BG KNE_UI_CARD
  #define GUIA_BADGE_BG KNE_UI_SELECT
#else
  #define GUIA_CARD_BG COLOR_SELECTION_BG
  #define GUIA_BADGE_BG COLOR_BACKGROUND
#endif

static void guia_tarjeta(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color) {
  #if ENABLED(PANTALLA_NUEVA)
    tft.add_round(x, y, w, h, r, color);
  #else
    tft.add_bar(x, y, w, h, color);
  #endif
}

// The display font has proportional glyphs: wrap and center in pixels, not characters.
// All guide/help copy is ASCII; the simulator reads the same pages, icons and font.
static const char *guia_siguiente_linea(const char *p, char *out, const uint16_t maxw) {
  while (*p == ' ') p++;
  uint8_t n = 0, espacio = 0;
  uint16_t ancho = 0;
  while (p[n] && p[n] != '\n' && n < 63) {
    const uint8_t w = tft_string.glyph(uint8_t(p[n]))->DWidth;
    if (ancho + w > maxw) break;
    if (p[n] == ' ') espacio = n;
    ancho += w;
    n++;
  }
  if (p[n] && p[n] != '\n' && espacio) n = espacio;
  if (!n && *p && *p != '\n') n = 1;
  memcpy(out, p, n);
  out[n] = 0;
  p += n;
  if (*p == '\n') p++;
  while (*p == ' ') p++;
  return p;
}

static void guia_texto_centrado(const char *s, uint16_t y, const uint16_t color) {
  char titulo[64];
  strncpy(titulo, s, sizeof(titulo) - 1);
  titulo[sizeof(titulo) - 1] = 0;
  tft_string.set(titulo);
  if (tft_string.width() > GUIA_TEXT_WIDTH) {
    uint8_t n = strlen(titulo);
    do {
      if (n) n--;
      titulo[n] = 0;
      tft_string.set(titulo);
      tft_string.add("...");
    } while (n && tft_string.width() > GUIA_TEXT_WIDTH);
  }
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), y, color, tft_string, GUIA_TEXT_WIDTH);
}

static void guia_dibujar(const char *titulo, const uint16_t colorTitulo, const char *texto, const char *pie, const uint8_t icono = GUIA_ICON_TOTAL) {
  char buf[64];
  tft.canvas(0, 0, TFT_WIDTH, 36);
  tft.set_background(COLOR_BACKGROUND);
  guia_tarjeta(8, 4, 304, 26, 6, GUIA_CARD_BG);
  tft.add_bar(8, 10, 3, 14, COLOR_MENU_VALUE);
  guia_texto_centrado(titulo, 7, colorTitulo);

  const bool ilustrada = icono < GUIA_ICON_TOTAL;
  tft.canvas(0, 36, TFT_WIDTH, 172);
  tft.set_background(COLOR_BACKGROUND);
  guia_tarjeta(8, 2, 304, 168, 8, GUIA_CARD_BG);
  if (ilustrada) {
    guia_tarjeta(138, 8, 44, 40, 6, GUIA_BADGE_BG);
    // Coalesce lit pixels into horizontal runs to keep the existing draw queue small.
    for (uint8_t y = 0; y < 32; y++) {
      const uint32_t row = guia_iconos[icono][y];
      for (uint8_t x = 0; x < 32;) {
        if (!(row & (1UL << (31 - x)))) { x++; continue; }
        const uint8_t begin = x;
        do { x++; } while (x < 32 && (row & (1UL << (31 - x))));
        tft.add_bar((TFT_WIDTH - 32) / 2 + begin, 12 + y, x - begin, 1, COLOR_MENU_VALUE);
      }
    }
  }

  const uint16_t top = ilustrada ? 54 : 12, height = ilustrada ? 110 : 148;
  const uint8_t max_lineas = height / 22;
  const char *p = texto ? texto : "";
  uint8_t lineas = 0;
  while (*p && lineas < max_lineas) { p = guia_siguiente_linea(p, buf, GUIA_TEXT_WIDTH); lineas++; }
  p = texto ? texto : "";
  const uint16_t offset = top + (height - lineas * 22) / 2;
  for (uint8_t r = 0; r < lineas; r++) {
    p = guia_siguiente_linea(p, buf, GUIA_TEXT_WIDTH);
    guia_texto_centrado(buf, offset + r * 22, COLOR_MENU_TEXT);
  }

  tft.canvas(0, 208, TFT_WIDTH, 32);
  tft.set_background(COLOR_BACKGROUND);
  guia_tarjeta(8, 4, 304, 24, 6, GUIA_CARD_BG);
  guia_texto_centrado(pie, 6, COLOR_MENU_VALUE);
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
    guia_dibujar(t.titulo[idioma], COLOR_MENU_VALUE, t.paginas[idioma][guia_pagina_sel], pie, t.iconos[guia_pagina_sel]);
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
    guia_dibujar(guia_etq, COLOR_MENU_VALUE, guia_txt_ayuda, guia_pie_ayuda[guia_idioma()]);
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
