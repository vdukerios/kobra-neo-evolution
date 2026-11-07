/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "probe.h"

#include "../../inc/MarlinConfigPre.h"
#include "bsp_spi_tft.h"
#include "dgus_Tunes.h"
using namespace Anycubic;


#if HAS_UI_320x240

#include "ui_common.h"

#include "../marlinui.h"
#include "../menu/menu.h"
#include "../../libs/numtostr.h"

#include "../../sd/cardreader.h"
#include "../../module/temperature.h"
#include "../../module/printcounter.h"
#include "../../module/planner.h"
#include "../../module/motion.h"

#if DISABLED(LCD_PROGRESS_BAR) && BOTH(FILAMENT_LCD_DISPLAY, SDSUPPORT)
  #include "../../feature/filwidth.h"
  #include "../../gcode/parser.h"
#endif

#if ENABLED(AUTO_BED_LEVELING_UBL)
  #include "../../feature/bedlevel/bedlevel.h"
#endif

#if HAS_MODO_NOCHE

  static bool noche_pantalla_negra = false; // La pantalla ya se pinto de negro

  #if ENABLED(NOCHE_LUZ_FONDO)
    // Luz de fondo: BL_ON es PORT_SetBits(PC0); apagar es el nivel contrario.
    void MarlinUI::noche_luz(const bool encendida) {
      if (encendida) { BL_ON; }
      else { PORT_ResetBits(BL_port, BL_pin); }
    }
  #endif

#endif

void MarlinUI::tft_idle() {
  #if HAS_MODO_NOCHE
    if (noche_oscura()) {
      if (!noche_pantalla_negra) {
        tft.queue.reset();
        tft.fill(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK);
        tft.queue.sync();                 // Pinta el negro una sola vez
        noche_pantalla_negra = true;
        #if ENABLED(NOCHE_LUZ_FONDO)
          noche_luz(false);
        #endif
      }
      else
        tft.queue.reset();                // Descarta todo lo que la UI haya encolado
      return;
    }
    else if (noche_pantalla_negra) {
      noche_pantalla_negra = false;
      #if ENABLED(NOCHE_LUZ_FONDO)
        noche_luz(true);
      #endif
    }
  #endif

  #if ENABLED(TOUCH_SCREEN)
    if (TERN0(HAS_TOUCH_SLEEP, lcd_sleep_task())) return;
    if (draw_menu_navigation) {
      add_control(48, 206, PAGE_UP, imgPageUp, encoderTopLine > 0);
      add_control(240, 206, PAGE_DOWN, imgPageDown, encoderTopLine + LCD_HEIGHT < screen_items);
      add_control(144, 206, BACK, imgBack);
      draw_menu_navigation = false;
    }
  #endif

  tft.queue.async();
  TERN_(TOUCH_SCREEN, touch.idle());
}

#if ENABLED(SHOW_BOOTSCREEN)

  void MarlinUI::show_bootscreen() {
//    tft.queue.reset();

//    tft.canvas(0, 0, TFT_WIDTH, TFT_HEIGHT);
//    #if ENABLED(BOOT_MARLIN_LOGO_SMALL)
//      #define BOOT_LOGO_W 195   // MarlinLogo195x59x16
//      #define BOOT_LOGO_H  59
//      #define SITE_URL_Y (TFT_HEIGHT - 46)
//      tft.set_background(COLOR_BACKGROUND);
//    #else
//      #define BOOT_LOGO_W TFT_WIDTH   // MarlinLogo320x240x16
//      #define BOOT_LOGO_H TFT_HEIGHT
//      #define SITE_URL_Y (TFT_HEIGHT - 52)
//    #endif
//    tft.add_image((TFT_WIDTH - BOOT_LOGO_W) / 2, (TFT_HEIGHT - BOOT_LOGO_H) / 2, imgBootScreen);
//    #ifdef WEBSITE_URL
//      tft_string.set(WEBSITE_URL);
//      tft.add_text(tft_string.center(TFT_WIDTH), SITE_URL_Y, COLOR_WEBSITE_URL, tft_string);
//    #endif

//    tft.queue.sync();
         Show_logo_secreen();
		 delay(100);
		 BL_ON;
		 //delay(1000);
     //    AC_PlayTune(BEEPER_PIN, Anycubic_PowerOn, 1); 
        
  }

  void MarlinUI::bootscreen_completion(const millis_t sofar) {
    if ((BOOTSCREEN_TIMEOUT) > sofar) safe_delay((BOOTSCREEN_TIMEOUT) - sofar);
    clear_lcd();
  }

#endif

void MarlinUI::draw_kill_screen() {
  tft.queue.reset();
  tft.fill(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_KILL_SCREEN_BG);

  tft.canvas(0, 60, TFT_WIDTH, 25);
  tft.set_background(COLOR_KILL_SCREEN_BG);
  tft_string.set(status_message);
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 3, COLOR_KILL_SCREEN_TEXT, tft_string);

  tft.canvas(0, 100, TFT_WIDTH, 25);
  tft.set_background(COLOR_KILL_SCREEN_BG);
  tft_string.set(GET_TEXT(MSG_HALTED));
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 3, COLOR_KILL_SCREEN_TEXT, tft_string);

  tft.canvas(0, 140, TFT_WIDTH, 25);
  tft.set_background(COLOR_KILL_SCREEN_BG);
  tft_string.set(GET_TEXT(MSG_PLEASE_RESET));
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 3, COLOR_KILL_SCREEN_TEXT, tft_string);

  tft.queue.sync();
}

void draw_heater_status(uint16_t x, uint16_t y, const int8_t Heater,char flag) {
  MarlinImage image = imgHotEnd;
  uint16_t Color;
  celsius_t currentTemperature, targetTemperature;

  if (Heater >= 0) { // HotEnd
    currentTemperature = thermalManager.degHotend(Heater);
	//if(currentTemperature == 0) currentTemperature = -15;
    targetTemperature = thermalManager.degTargetHotend(Heater);
  }
  #if HAS_HEATED_BED
    else if (Heater == H_BED) {
      currentTemperature = thermalManager.wholeDegBed();
      targetTemperature = thermalManager.degTargetBed();
    }
  #endif
  #if HAS_TEMP_CHAMBER
    else if (Heater == H_CHAMBER) {
      currentTemperature = thermalManager.wholeDegChamber();
      #if HAS_HEATED_CHAMBER
        targetTemperature = thermalManager.degTargetChamber();
      #else
        targetTemperature = ABSOLUTE_ZERO;
      #endif
    }
  #endif
  #if HAS_TEMP_COOLER
    else if (Heater == H_COOLER) {
      currentTemperature = thermalManager.wholeDegCooler();
      targetTemperature = TERN(HAS_COOLER, thermalManager.degTargetCooler(), ABSOLUTE_ZERO);
    }
  #endif
  else return;

  TERN_(TOUCH_SCREEN, if (targetTemperature >= 0) touch.add_control(HEATER, x, y, 64, 100, Heater));
  tft.canvas(x, y, 66, 100);
  tft.set_background(COLOR_BACKGROUND);
  if(flag)
  {
     tft.add_rectangle(0, 0, 66, 100, COLOR_AXIS_HOMED);
  }
  Color = currentTemperature < 0 ? COLOR_INACTIVE : COLOR_COLD;

  if (Heater >= 0) { // HotEnd
    if (currentTemperature >= 50) Color = COLOR_HOTEND;
  }
  #if HAS_HEATED_BED
    else if (Heater == H_BED) {
      if (currentTemperature >= 50) Color = COLOR_HEATED_BED;
      image = targetTemperature > 0 ? imgBedHeated : imgBed;
    }
  #endif
  #if HAS_TEMP_CHAMBER
    else if (Heater == H_CHAMBER) {
      if (currentTemperature >= 50) Color = COLOR_CHAMBER;
      image = targetTemperature > 0 ? imgChamberHeated : imgChamber;
    }
  #endif
  #if HAS_TEMP_COOLER
    else if (Heater == H_COOLER) {
      if (currentTemperature <= 26) Color = COLOR_COLD;
      if (currentTemperature > 26) Color = COLOR_RED;
      image = targetTemperature > 26 ? imgCoolerHot : imgCooler;
    }
  #endif

  tft.add_image(1, 18, image, Color);

  tft_string.set((uint8_t *)i16tostr3rj(currentTemperature));
  //tft_string.add(LCD_STR_DEGREE);
  tft_string.trim();
  tft.add_text(tft_string.center(64) + 2, 72, Color, tft_string);

  if (targetTemperature >= 0) {
    tft_string.set((uint8_t *)i16tostr3rj(targetTemperature));
    //tft_string.add(LCD_STR_DEGREE);
    tft_string.trim();
    tft.add_text(tft_string.center(64) + 2, 8, Color, tft_string);
  }
}

void draw_fan_status(uint16_t x, uint16_t y, const bool blink,char flag) {
  TERN_(TOUCH_SCREEN, touch.add_control(FAN, x, y, 64, 100));
  tft.canvas(x, y, 66, 100);
  tft.set_background(COLOR_BACKGROUND);
  if(flag)
  {
     tft.add_rectangle(0, 0, 66, 100, COLOR_AXIS_HOMED);
  }
  uint8_t fanSpeed = thermalManager.fan_speed[0];
  MarlinImage image;

  if (fanSpeed >= 127)
    image = blink ? imgFanFast1 : imgFanFast0;
  else if (fanSpeed > 0)
    image = blink ? imgFanSlow1 : imgFanSlow0;
  else
    image = imgFanIdle;

  tft.add_image(1, 10, image, COLOR_FAN);

  tft_string.set((uint8_t *)ui8tostr4pctrj(thermalManager.fan_speed[0]));
  tft_string.trim();
  tft.add_text(tft_string.center(64) + 6, 72, COLOR_FAN, tft_string);
}

#if ENABLED(PANTALLA_NUEVA)

// ---------------------------------------------------------------------------
// Pantallas de estado rediseniadas (inicio en reposo e impresion).
// Se dibuja SIEMPRE en franjas horizontales de ancho completo que cubren los
// 240 px de alto: asi no quedan restos de otra pagina y no hace falta borrar.
// Textos con acentos: UTF-8 (tft_string los convierte). Sin sprintf: texto a mano.
// ---------------------------------------------------------------------------

#if ENABLED(MINIATURA_GCODE)
  #include "../../feature/gcode_thumb.h"
#endif

extern uint8_t pantalla_inicio_foco;
extern uint8_t pantalla_pagina;   // definida en marlinui.cpp (la cambia la rueda)

#define PN_CARD   KNE_UI_CARD
#define PN_TRACK  KNE_UI_TRACK
#define PN_ACC    KNE_UI_ACC
#define PN_DIM    KNE_UI_DIM
#define PN_TXT    KNE_UI_TEXT
#define PN_WARN   KNE_UI_WARN
#define PN_COLD   KNE_UI_COLD
#define PN_HOT    KNE_UI_HOT

static void pn_band(uint16_t y, uint16_t h) {
  tft.canvas(0, y, TFT_WIDTH, h);
  tft.set_background(COLOR_BACKGROUND);
}

static void pn_txt(uint16_t x, uint16_t y, uint16_t c, const char *s, uint16_t maxw = 0) {
  tft_string.set(s);
  tft.add_text(x, y, c, tft_string, maxw);
}
static void pn_txt_c(uint16_t cx, uint16_t y, uint16_t c, const char *s) {   // centrado en cx
  tft_string.set(s);
  const uint16_t h = tft_string.width() / 2;
  tft.add_text(h > cx ? 0 : cx - h, y, c, tft_string);
}
static void pn_txt_r(uint16_t rx, uint16_t y, uint16_t c, const char *s) {   // alineado a la derecha en rx
  tft_string.set(s);
  const uint16_t w = tft_string.width();
  tft.add_text(w > rx ? 0 : rx - w, y, c, tft_string);
}
// Recorta s con "..." para que quepa en maxw pixeles (devuelve s tal cual si ya cabe).
// Usa un buffer propio: valido solo hasta la siguiente llamada. Corta siempre al inicio de un caracter.
static const char* pn_fit(const char *s, uint16_t maxw) {
  static char out[MAX_STRING_LENGTH + 1];
  uint16_t total = 0, len = 0;
  for (const char *p = s; *p; p++, len++)
    if ((uint8_t(*p) & 0xC0) != 0x80) total += tft_string.glyph(uint8_t(*p))->DWidth;
  if (total <= maxw && len <= MAX_STRING_LENGTH) return s;
  const uint16_t dots = 3 * tft_string.glyph(uint8_t('.'))->DWidth;
  uint16_t w = 0;
  uint8_t cut = 0;
  for (uint8_t i = 0; s[i] && i < MAX_STRING_LENGTH - 3; i++) {
    if ((uint8_t(s[i]) & 0xC0) == 0x80) continue;   // continuacion de un caracter multibyte
    if (w + dots > maxw) break;
    cut = i;
    w += tft_string.glyph(uint8_t(s[i]))->DWidth;
  }
  for (uint8_t j = 0; j < cut; j++) out[j] = s[j];
  out[cut] = '.'; out[cut + 1] = '.'; out[cut + 2] = '.'; out[cut + 3] = 0;
  return out;
}
static void pn_big_c(uint16_t cx, uint16_t y, uint16_t c, const char *s) {   // doble tamano, centrado
  tft_string.set(s);
  const uint16_t h = tft_string.width();   // mitad del ancho doble = ancho simple
  tft.add_text_big(h > cx ? 0 : cx - h, y, c, tft_string);
}

static char* pn_u(char *p, uint32_t v, uint8_t digits = 0) {
  char t[11];
  uint8_t n = 0;
  do { t[n++] = '0' + (v % 10); v /= 10; } while (v);
  while (n < digits) t[n++] = '0';
  while (n) *p++ = t[--n];
  return p;
}
static char* pn_s(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char* pn_dec(char *p, float v, uint8_t dec) {   // -1,25 (coma decimal)
  const bool neg = v < 0;
  if (neg) v = -v;
  const uint16_t sc = dec == 2 ? 100 : 10;
  const uint32_t n = uint32_t(v * sc + 0.5f);
  if (neg && n) *p++ = '-';
  p = pn_u(p, n / sc);
  *p++ = ',';
  return pn_u(p, n % sc, dec);
}
static char* pn_time(char *p, uint32_t s, bool secs) {
  const uint32_t h = s / 3600, m = (s / 60) % 60;
  if (h) { p = pn_u(p, h); p = pn_s(p, "h "); p = pn_u(p, m, 2); *p++ = 'm'; }
  else   { p = pn_u(p, m); *p++ = 'm'; if (secs) { *p++ = ' '; p = pn_u(p, s % 60, 2); *p++ = 's'; } }
  return p;
}

static uint16_t pn_r(float v) { return v < 0 ? 0 : uint16_t(v + 0.5f); }
static uint16_t pn_tcol(float cur, int16_t tgt) {
  if (tgt > 0) return cur < tgt - 2 ? PN_WARN : PN_HOT;
  return cur >= 50 ? PN_HOT : PN_COLD;
}

// Barra redondeada (pista + relleno) con f entre 0 y 1.
static void pn_bar(uint16_t x, uint16_t y, uint16_t w, uint8_t h, float f, uint16_t col) {
  tft.add_round(x, y, w, h, h / 2, PN_TRACK);
  if (f > 0) {
    if (f > 1) f = 1;
    uint16_t fw = uint16_t(w * f);
    if (fw < h) fw = h;
    tft.add_round(x, y, fw, h, h / 2, col);
  }
}

static uint32_t pn_restante(uint8_t pct) {
  // 1) El G-code trae M73 R (script AgregarM73 de Cura): es la mejor estimacion; get_remaining_time() ya aplica
  //    el factor aprendido.
  #if BOTH(HAS_PRINT_PROGRESS, LCD_SET_PROGRESS_MANUALLY) && ENABLED(SHOW_REMAINING_TIME) && ENABLED(USE_M73_REMAINING_TIME)
    if (ui.remaining_time) return ui.get_remaining_time();
  #endif
  // 2) Sin M73: el tiempo ";TIME:" de la cabecera de Cura, corregido por el factor aprendido, repartido segun
  //    el avance del archivo. NO se usa la extrapolacion "lo que llevo x lo que falta / lo que llevo": cuenta el
  //    calentamiento y la espera inicial y se dispara (1:30 saltaba a 2 h).
  #if ENABLED(MINIATURA_GCODE)
    uint32_t est = GcodeThumb::estimated_seconds();
    if (est) {
      #ifdef APRENDIZAJE_TIEMPOS
        est = uint32_t(float(est) * tiempo_factor() + 0.5f);
      #endif
      const uint32_t r = uint32_t((uint64_t)est * (100 - (pct > 100 ? 100 : pct)) / 100);
      return r ? r : 1;
    }
  #endif
  // 3) Sin ninguna estimacion del archivo: extrapolar con lo transcurrido (solo con avance).
  const duration_t el = print_job_timer.duration();
  return pct ? uint32_t(el.value * (100 - pct) / pct) : 0;
}

static void pn_card_temp(uint16_t x, const char *label, float cur, int16_t tgt, float maxv, uint16_t accent) {
  char b[24];
  tft.add_round(x, 6, 148, 64, 6, PN_CARD);
  tft.add_bar(x + 10, 6, 128, 2, accent);
  pn_txt(x + 10, 9, PN_DIM, label, 128);
  char *p = pn_u(b, pn_r(cur));
  p = pn_s(p, " / ");
  p = pn_u(p, tgt);
  *pn_s(p, " C") = 0;
  pn_txt(x + 10, 31, accent, b, 128);
  pn_bar(x + 10, 62, 128, 3, tgt > 0 ? cur / tgt : cur / maxv, accent);
}

static void pn_inicio_button(uint16_t x, uint8_t foco, const char *label, uint16_t color) {
  const bool selected = pantalla_inicio_foco == foco;
  tft.add_round(x, 3, 70, 48, 5, selected ? PN_TXT : PN_TRACK);
  tft.add_round(x + 2, 5, 66, 44, 4, color);
  if (!foco) {
    for (uint8_t j = 0; j < 3; j++) tft.add_bar(x + 27, 10 + j * 4, 16, 2, PN_TXT);
  }
  else if (foco == 1) {
    tft.add_bar(x + 26, 12, 3, 10, PN_TXT);
    tft.add_bar(x + 33, 8, 3, 14, PN_TXT);
    tft.add_bar(x + 40, 12, 3, 10, PN_TXT);
  }
  else {
    tft.add_bar(x + 34, 8, 2, 13, PN_TXT);
    const uint16_t y = foco == 2 ? 18 : 8;
    tft.add_bar(x + 29, y, 12, 2, PN_TXT);
    tft.add_bar(x + 31, foco == 2 ? 20 : 6, 8, 2, PN_TXT);
  }
  pn_txt_c(x + 35, 26, PN_TXT, pn_fit(label, 66));
}

// ------------------------------- INICIO ------------------------------------
static void pn_inicio(const char *msg) {
  char b[24];
  const float cn = thermalManager.degHotend(0), cb = float(thermalManager.wholeDegBed());
  const int16_t tn = int16_t(thermalManager.degTargetHotend(0)), tb = int16_t(thermalManager.degTargetBed());
  const bool heating = (tn > 0 && cn < tn - 2) || (tb > 0 && cb < tb - 2),
             cooling = !tn && !tb && (cn > 45 || cb > 45),
             ready = !heating && (tn > 0 || tb > 0);
  const uint16_t stc = heating ? PN_WARN : cooling ? PN_COLD : ready ? PN_HOT : PN_ACC;

  pn_band(0, 22);
  if (msg[0]) pn_txt(8, 0, PN_TXT, pn_fit(msg, 218));
  else {
    pn_txt(8, 0, PN_TXT, "KNE");
    pn_txt(54, 0, stc, pn_fit(heating ? GET_TEXT(MSG_KNE_HEATING) : cooling ? GET_TEXT(MSG_KNE_COOLING) : ready ? GET_TEXT(MSG_KNE_READY) : GET_TEXT(MSG_KNE_IDLE), 164));
  }
  pn_txt_r(312, 0, card.isMounted() ? PN_ACC : PN_DIM,
    card.isMounted() ? GET_TEXT(MSG_KNE_SD_READY) : GET_TEXT(MSG_KNE_NO_SD));

  pn_band(22, 74);
  pn_card_temp(8, GET_TEXT(MSG_KNE_NOZZLE), cn, tn, 250, PN_HOT);
  pn_card_temp(164, GET_TEXT(MSG_KNE_BED), cb, tb, 100, PN_COLD);

  pn_band(96, 46);
  tft.add_round(8, 2, 148, 40, 5, pantalla_inicio_foco == 5 ? PN_ACC : PN_TRACK);
  tft.add_round(10, 4, 144, 36, 4, PN_CARD);
  tft.add_round(164, 2, 148, 40, 5, PN_CARD);
  pn_txt(18, 2, PN_DIM, GET_TEXT(MSG_KNE_Z_OFFSET));
  pn_txt(174, 2, PN_DIM, GET_TEXT(MSG_KNE_Z_HEIGHT));
  { char *p = pn_dec(b, probe.offset.z, 2); *pn_s(p, " mm") = 0; }
  pn_txt(18, 21, PN_ACC, b);
  if (axis_should_home(Z_AXIS)) pn_txt(174, 21, PN_DIM, GET_TEXT(MSG_KNE_NOT_HOMED));
  else { char *p = pn_dec(b, LOGICAL_Z_POSITION(current_position.z), 1); *pn_s(p, " mm") = 0; pn_txt(174, 21, PN_TXT, b); }

  pn_band(142, 54);
  pn_inicio_button(8, 0, GET_TEXT(MSG_KNE_QUICK_MENU), 0x2253);
  pn_inicio_button(86, 1, GET_TEXT(MSG_KNE_QUICK_PREHEAT), 0xB246);
  pn_inicio_button(164, 2, GET_TEXT(MSG_KNE_QUICK_LOAD), 0x0472);
  pn_inicio_button(242, 3, GET_TEXT(MSG_KNE_QUICK_UNLOAD), 0x6A91);

  pn_band(196, 32);
  tft.add_round(8, 3, 304, 26, 5, pantalla_inicio_foco == 4 ? PN_TXT : PN_TRACK);
  tft.add_round(10, 5, 300, 22, 4, 0x2352);
  pn_txt_c(160, 5, PN_TXT, GET_TEXT(MSG_KNE_QUICK_LEVEL));

  // The existing font is 20 px high: the 12 px footer uses focus dots, not clipped text.
  pn_band(228, 12);
  for (uint8_t j = 0; j < 6; j++)
    tft.add_round(119 + j * 14, 4, 10, 4, 2, pantalla_inicio_foco == j ? PN_ACC : PN_TRACK);
}

// ------------------------------ IMPRESION ----------------------------------
static void pn_cabecera(bool pausa, uint8_t pag) {
  pn_band(0, 26);
  tft.add_round(14, 8, 10, 10, 5, pausa ? PN_WARN : PN_ACC);
  pn_txt(32, 3, PN_TXT, pausa ? GET_TEXT(MSG_KNE_PAUSED) : GET_TEXT(MSG_KNE_PRINTING));
  for (uint8_t i = 0; i < 3; i++) tft.add_round(268 + 16 * i, 9, 8, 8, 4, i == pag ? PN_ACC : PN_TRACK);
}

static void pn_pie(const char *msg) {
  pn_band(216, 24);
  pn_txt_c(160, 2, msg[0] ? PN_TXT : PN_DIM,
    pn_fit(msg[0] ? msg : GET_TEXT(MSG_KNE_HINT_PAGES), 304));
}

static void pn_miniatura(uint8_t pct) {
  uint16_t w = 72, h = 72;
  bool real = false;
  #if ENABLED(MINIATURA_GCODE)
    if (GcodeThumb::ready() && GcodeThumb::pixels() && GcodeThumb::width() && GcodeThumb::height()
        && GcodeThumb::width() <= 104 && GcodeThumb::height() <= 104) {
      w = GcodeThumb::width(); h = GcodeThumb::height(); real = true;
    }
  #endif
  const uint16_t tx = 8 + (112 - w) / 2, ty = 2 + (112 - h) / 2,
                 lit = (h * pct) / 100, dim = h - lit;   // filas apagadas (arriba) / encendidas (abajo)
  #if ENABLED(MINIATURA_GCODE)
    if (real) tft.add_pixels(tx, ty, w, h, GcodeThumb::pixels(), dim);
  #endif
  if (!real) {                                      // marcador de posicion: caja y relleno desde abajo
    tft.add_bar(tx, ty, w, h, PN_TRACK);
    if (lit) tft.add_bar(tx, ty + dim, w, lit, PN_ACC);
  }
  if (pct > 0 && pct < 100) tft.add_bar(tx, ty + dim, w, 1, PN_WARN);   // borde del avance
}

static void pn_pag0(uint8_t pct, bool pausa) {
  char b[24];
  const float cn = thermalManager.degHotend(0), cb = float(thermalManager.wholeDegBed());
  const int16_t tn = int16_t(thermalManager.degTargetHotend(0)), tb = int16_t(thermalManager.degTargetBed());
  const duration_t el = print_job_timer.duration();

  pn_cabecera(pausa, 0);

  pn_band(26, 116);                                 // miniatura + datos
  tft.add_round(8, 2, 112, 112, 10, PN_CARD);
  pn_miniatura(pct);
  *pn_s(pn_u(b, pct), "%") = 0;
  tft_string.set(b);
  tft.add_text_big(132, 0, PN_ACC, tft_string);
  pn_txt(132, 46, PN_DIM, GET_TEXT(MSG_KNE_REMAINING));
  { const uint32_t r = pn_restante(pct); if (r) *pn_time(b, r, false) = 0; else pn_s(b, "--")[0] = 0; }
  pn_txt_r(312, 46, PN_TXT, b);
  pn_txt(132, 70, PN_DIM, GET_TEXT(MSG_KNE_ELAPSED));
  *pn_time(b, el.value, false) = 0;
  pn_txt_r(312, 70, PN_TXT, b);
  pn_txt(132, 94, PN_DIM, GET_TEXT(MSG_KNE_Z_HEIGHT));
  if (axis_should_home(Z_AXIS)) pn_s(b, "--")[0] = 0;
  else *pn_s(pn_dec(b, LOGICAL_Z_POSITION(current_position.z), 1), " mm") = 0;
  pn_txt_r(312, 94, PN_TXT, b);

  pn_band(142, 34);                                 // temperaturas compactas
  tft.add_round(8, 2, 304, 30, 8, PN_CARD);
  { char *p = pn_u(pn_s(b, GET_TEXT(MSG_KNE_NOZZLE_ABBR)), pn_r(cn)); p = pn_u(pn_s(p, "/"), tn); *p = 0; }
  pn_txt(20, 5, pn_tcol(cn, tn), b);
  { char *p = pn_u(pn_s(b, GET_TEXT(MSG_KNE_BED_ABBR)), pn_r(cb)); p = pn_u(pn_s(p, "/"), tb); *p = 0; }
  pn_txt(170, 5, pn_tcol(cb, tb), b);

  pn_band(176, 40);                                 // barra de avance y mensaje
  pn_bar(8, 10, 304, 12, pct / 100.0f, PN_ACC);
}

static void pn_fila(uint16_t y, const char *etq, const char *val, uint16_t vcol, float f, uint16_t bcol) {
  pn_band(y, 38);
  pn_txt(12, 0, PN_DIM, etq);
  pn_txt_r(308, 0, vcol, val);
  if (f >= 0) pn_bar(12, 24, 296, 8, f, bcol);
}

static void pn_pag1(bool pausa) {                   // temperaturas, velocidad, flujo, ventilador
  char b[24], *p;
  const float cn = thermalManager.degHotend(0), cb = float(thermalManager.wholeDegBed());
  const int16_t tn = int16_t(thermalManager.degTargetHotend(0)), tb = int16_t(thermalManager.degTargetBed());
  const uint8_t fan = uint8_t((thermalManager.fan_speed[0] * 100 + 127) / 255);
  pn_cabecera(pausa, 1);
  p = pn_u(b, pn_r(cn)); p = pn_s(p, " / "); p = pn_u(p, tn); pn_s(p, " C")[0] = 0;
  pn_fila(26, GET_TEXT(MSG_KNE_NOZZLE), b, pn_tcol(cn, tn), tn > 0 ? cn / tn : cn / 250, pn_tcol(cn, tn));
  p = pn_u(b, pn_r(cb)); p = pn_s(p, " / "); p = pn_u(p, tb); pn_s(p, " C")[0] = 0;
  pn_fila(64, GET_TEXT(MSG_KNE_BED), b, pn_tcol(cb, tb), tb > 0 ? cb / tb : cb / 100, pn_tcol(cb, tb));
  *pn_s(pn_u(b, feedrate_percentage), " %") = 0;
  pn_fila(102, GET_TEXT(MSG_KNE_SPEED), b, PN_TXT, feedrate_percentage / 200.0f, PN_ACC);
  *pn_s(pn_u(b, planner.flow_percentage[0]), " %") = 0;
  pn_fila(140, GET_TEXT(MSG_KNE_FLOW), b, PN_TXT, planner.flow_percentage[0] / 200.0f, PN_ACC);
  *pn_s(pn_u(b, fan), " %") = 0;
  pn_fila(178, GET_TEXT(MSG_KNE_FAN), b, PN_TXT, fan / 100.0f, PN_COLD);
}

static void pn_pag2(uint8_t pct, bool pausa) {      // estadisticas
  char b[24];
  const duration_t el = print_job_timer.duration();
  pn_cabecera(pausa, 2);
  pn_band(26, 38);
  pn_txt(12, 0, PN_DIM, GET_TEXT(MSG_KNE_FILE));
  #if ENABLED(SDSUPPORT)
    pn_txt(112, 0, PN_TXT, pn_fit(card.longest_filename(), 196), 196);
  #else
    pn_txt(112, 0, PN_TXT, "--");
  #endif
  *pn_time(b, el.value, true) = 0;
  pn_fila(64, GET_TEXT(MSG_KNE_ELAPSED), b, PN_TXT, -1, 0);
  { const uint32_t r = pn_restante(pct); if (r) *pn_time(b, r, true) = 0; else pn_s(b, "--")[0] = 0; }
  pn_fila(102, GET_TEXT(MSG_KNE_REMAINING), b, PN_TXT, -1, 0);
  *pn_s(pn_u(b, pct), " %") = 0;
  pn_fila(140, GET_TEXT(MSG_KNE_PROGRESS), b, PN_ACC, pct / 100.0f, PN_ACC);
  if (axis_should_home(Z_AXIS)) pn_s(b, "--")[0] = 0;
  else *pn_s(pn_dec(b, LOGICAL_Z_POSITION(current_position.z), 1), " mm") = 0;
  pn_fila(178, GET_TEXT(MSG_KNE_Z_HEIGHT), b, PN_TXT, -1, 0);
}

static void pn_dibujar(const char *msg, bool busy, bool paused) {
  TERN_(TOUCH_SCREEN, touch.clear());
  if (!busy && !paused) { pn_inicio(msg); return; }
  const uint8_t pct = ui.get_progress_percent() > 100 ? 100 : ui.get_progress_percent();
  switch (pantalla_pagina) {
    default: pn_pag0(pct, paused); break;
    case 1:  pn_pag1(paused);           break;
    case 2:  pn_pag2(pct, paused);      break;
  }
  pn_pie(msg);
}

#endif // PANTALLA_NUEVA

void MarlinUI::draw_status_screen(char seclect) {
#if ENABLED(PANTALLA_NUEVA)
  pn_dibujar(status_message, printingIsActive(), printingIsPaused());
  return;
#endif
  const bool blink = get_blink();
  const bool busy = printingIsActive();
  const bool Paused= printingIsPaused();
  uint16_t color;
  TERN_(TOUCH_SCREEN, touch.clear());

  // heaters and fan
  uint16_t i, x, y = TFT_STATUS_TOP_Y;

  for (i = 0 ; i < ITEMS_COUNT; i++) {
    x = (320 / ITEMS_COUNT - 64) / 2  + (320 * i / ITEMS_COUNT);
    switch (i) {
      #ifdef ITEM_E0
        case ITEM_E0:
          if(seclect==1)
          {
            /*
            if(blink) // Blink selection box
            {
               draw_heater_status(x, y, H_E0, 1);
            }
            else
            {
                //tft.canvas(x, y, 64, 100);
                //tft.set_background(COLOR_BACKGROUND);
                draw_heater_status(x, y, H_E0, 0);
            }
            */
            draw_heater_status(x, y, H_E0,1);
          }
          else
          {
            draw_heater_status(x, y, H_E0,0);
          }
          break;
      #endif
      #ifdef ITEM_E1
        case ITEM_E1:draw_heater_status(x, y, H_E1); break;
      #endif
      #ifdef ITEM_E2
        case ITEM_E2: draw_heater_status(x, y, H_E2); break;
      #endif
      #ifdef ITEM_BED
        case ITEM_BED:
          if(seclect==2)
          {
            /*
            if(blink) // Blink selection box
            {
                 draw_heater_status(x, y, H_BED, 1); 
            }
            else
            {
                //tft.canvas(x, y, 64, 100);
                //tft.set_background(COLOR_BACKGROUND);
                draw_heater_status(x, y, H_BED,0);
            }
            */
            draw_heater_status(x, y, H_BED,1); 
          }
          else
          {
              draw_heater_status(x, y, H_BED,0); 
          }
       
        break;
      #endif
      #ifdef ITEM_CHAMBER
        case ITEM_CHAMBER: draw_heater_status(x, y, H_CHAMBER); break;
      #endif
      #ifdef ITEM_COOLER
        case ITEM_COOLER: draw_heater_status(x, y, H_COOLER); break;
      #endif
      #ifdef ITEM_FAN
        case ITEM_FAN:
           if(seclect==3)
          {
            /*
            if(blink) // Blink selection box
            {
                draw_fan_status(x, y, blink, 1);
            }
            else
            {
                draw_fan_status(x, y, blink, 0);
                //tft.canvas(x, y, 64,100);
                //tft.set_background(COLOR_BACKGROUND);
            }
            */
            draw_fan_status(x, y, blink,1);
          }
          else
          {
             draw_fan_status(x, y, blink,0);
          }
       
        
        break;
      #endif
    }
  } 
  #if 1  // Keep the matching legacy #else / #endif below.
  //***********z.offset**************************
//    if((seclect==4)&&( blink))
//    {
//      tft.canvas(4, 103, 125, 24);
//      tft.set_background(COLOR_BACKGROUND);    
//    }
//    else
//    {
      /* // Do not blink z-offset
      if( blink)
      {
        color=COLOR_VIVID_GREEN;
      }
      else
      {
        color=COLOR_GREY;
      }
      */
      color=COLOR_VIVID_GREEN;
      tft.canvas(4, 103, 125, 24);
      tft.set_background(COLOR_BACKGROUND);
      if(seclect==4)
      {
        tft.add_rectangle(0, 0, 125, 24, COLOR_AXIS_HOMED);
      }
      tft.add_text( 5, 3, color , GET_TEXT(MSG_Z_OFFSET));
      tft.add_text( 75, 3, color ,ftostr42_52( probe.offset.z));      
//    }

 

  // feed rate

//   if((seclect==5)&&( blink))
//   {
//        tft.canvas(140, 100, 80, 32);
//      tft.set_background(COLOR_BACKGROUND);

//   }
//   else
//   {
//      tft.canvas(140, 100, 80, 32);
//      tft.set_background(COLOR_BACKGROUND);
//      color = feedrate_percentage == 100 ? COLOR_RATE_100 : COLOR_RATE_ALTERED;
//      tft.add_image(0, 0, imgFeedRate, color);
//      tft_string.set(i16tostr3rj(feedrate_percentage));
//      tft_string.add('%');
//      tft.add_text(32, 6, color , tft_string);

//   }
      tft.canvas(140, 100, 82, 34);
      tft.set_background(COLOR_BACKGROUND);
      if(seclect==5)
      {
        tft.add_rectangle(0, 0, 82, 34, COLOR_AXIS_HOMED);
      }
      color = feedrate_percentage == 100 ? COLOR_RATE_100 : COLOR_RATE_ALTERED;
      tft.add_image(1, 1, imgFeedRate, color);
      tft_string.set(i16tostr3rj(feedrate_percentage));
      tft_string.add('%');
      tft.add_text(32, 6, color , tft_string);
  // flow rate
//     if((seclect==6)&&( blink))
//     {
//         tft.canvas(235, 100, 80, 32);
//        tft.set_background(COLOR_BACKGROUND);     
//     }
//     else
//     {
//        tft.canvas(235, 100, 80, 32);
//        tft.set_background(COLOR_BACKGROUND);
//        color = planner.flow_percentage[0] == 100 ? COLOR_RATE_100 : COLOR_RATE_ALTERED;
//        tft.add_image(0, 0, imgFlowRate, color);
//        tft_string.set(i16tostr3rj(planner.flow_percentage[active_extruder]));
//        tft_string.add('%');
//        tft.add_text(32, 6, color , tft_string);
// 
//     }
     
 
//        tft.canvas(235, 100, 82, 34);
//        tft.set_background(COLOR_BACKGROUND);
//        if(seclect==6)
//        {
//          tft.add_rectangle(0, 0, 82, 34, COLOR_AXIS_HOMED);
//        }
//        color = planner.flow_percentage[0] == 100 ? COLOR_RATE_100 : COLOR_RATE_ALTERED;
//        tft.add_image(1, 1, imgFlowRate, color);
//        tft_string.set(i16tostr3rj(planner.flow_percentage[active_extruder]));
//        tft_string.add('%');
//        tft.add_text(32, 6, color , tft_string); 
        tft.canvas(228, 100, 82, 34);
        tft.set_background(COLOR_BACKGROUND); //COLOR_BACKGROUND
        if(seclect==6)
        {
          tft.add_rectangle(0, 0, 82, 34, COLOR_AXIS_HOMED);
        }
		
        tft.add_image(1, 1, imgSettings, COLOR_VIVID_GREEN);
  
  if (busy)
  {
    tft.add_text( 34, 8, COLOR_VIVID_GREEN , GET_TEXT(MSG_TUNE));  
  } else {
    tft.add_text( 34, 8, COLOR_VIVID_GREEN , GET_TEXT(MSG_MAIN_SHORT));  
  }
     
     //print sd card  
  tft.canvas(115, 140, 48, 48);
  tft.set_background(COLOR_BACKGROUND);
  bool  card_detected = card.isMounted();
  if(card_detected)
  tft.add_image(0, 0, imgSD, COLOR_AQUA);
  else
  tft.add_image(0, 0, imgSD, COLOR_GREY);



//*********************Pause*****************

//    if((seclect==7)&&( blink))
//    {
//      tft.canvas(185, 140, 48, 48);
//      tft.set_background(COLOR_BACKGROUND);      
//    }
//    else
//    {
//       if(busy)    //���ڴ�ӡ��
//       {
//         color=COLOR_ORANGE;
//       }
//       else if(Paused)      //��ͣ��
//       {
//         color=COLOR_GREEN; 
//       }
//       else          //ѹ����û�д�ӡ
//       {
//         color=COLOR_GREY;
//       }
//       tft.canvas(185, 140, 48, 48);
//      tft.set_background(COLOR_BACKGROUND);
//      tft.add_image(0, 0, imgPause, color);
//    }

       if(busy)    //���ڴ�ӡ��
       {
         color=COLOR_ORANGE;
       }
       else if(Paused)      //��ͣ��
       {
         color=COLOR_GREEN; 
       }
       else          //ѹ����û�д�ӡ
       {
#ifdef ACTION_ON_START
         color=COLOR_GREEN;
#else
         color=COLOR_GREY; 
#endif
       }
       tft.canvas(185, 140, 50, 50);
      tft.set_background(COLOR_BACKGROUND);
        if(seclect==7)
        {
          tft.add_rectangle(0, 0, 50, 50, COLOR_AXIS_HOMED);
        }
      tft.add_image(1, 1, imgPause, color); 
  
  
////*********************Cancel*************** 

//    if((seclect==8)&&( blink))
//    {
//      tft.canvas(255, 140, 48, 48);
//      tft.set_background(COLOR_BACKGROUND);     
//    }
//    else
//    {
//       if(busy)    //���ڴ�ӡ��
//       {
//         color=COLOR_RED;
//       }
//       else if(Paused)      //��ͣ��
//       {
//         color=COLOR_RED; 
//       }
//       else          //ѹ����û�д�ӡ
//       {
//         color=COLOR_GREY;
//       }
//       tft.canvas(255, 140, 48, 48);
//      tft.set_background(COLOR_BACKGROUND);
//      tft.add_image(0, 0, imgStop, color);     
//    }

       if(busy)    //���ڴ�ӡ��
       {
         color=COLOR_RED;
       }
       else if(Paused)      //��ͣ��
       {
         color=COLOR_RED; 
       }
       else          //ѹ����û�д�ӡ
       {
         color=COLOR_GREY;
       }
       tft.canvas(255, 140, 50, 50);
      tft.set_background(COLOR_BACKGROUND);
       if(seclect==8)
        {
          tft.add_rectangle(0, 0, 50, 50, COLOR_AXIS_HOMED);
        }
      tft.add_image(1, 1, imgStop, color);       
  
  
  // print duration
  char buffer[14];
  if(blink && !busy)
  {
    color=COLOR_GREY;
  }
  else
  {
    color=COLOR_VIVID_GREEN;
  }
  duration_t elapsed = print_job_timer.duration();
  elapsed.toDigital(buffer);
  
    tft.canvas(15, 145, 80, 20);
    tft.set_background(COLOR_BACKGROUND);
    tft.add_text(0, 0, color, GET_TEXT(MSG_PRINT_TIME));

  tft.canvas(10, 170, 100, 20);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(buffer);
  tft.add_text(20, 0, color, tft_string);  
  
  #else

  // coordinates
  tft.canvas(4, 103, 312, 24);
  tft.set_background(COLOR_BACKGROUND);
  tft.add_rectangle(0, 0, 312, 24, COLOR_AXIS_HOMED);

  tft.add_text( 10, 3, COLOR_AXIS_HOMED , "X");
  tft.add_text(127, 3, COLOR_AXIS_HOMED , "Y");
  tft.add_text(219, 3, COLOR_AXIS_HOMED , "Z");

  bool not_homed = axis_should_home(X_AXIS);
  tft_string.set(blink && not_homed ? "?" : ftostr4sign(LOGICAL_X_POSITION(current_position.x)));
  tft.add_text( 68 - tft_string.width(), 3, not_homed ? COLOR_AXIS_NOT_HOMED : COLOR_AXIS_HOMED, tft_string);

  not_homed = axis_should_home(Y_AXIS);
  tft_string.set(blink && not_homed ? "?" : ftostr4sign(LOGICAL_Y_POSITION(current_position.y)));
  tft.add_text(185 - tft_string.width(), 3, not_homed ? COLOR_AXIS_NOT_HOMED : COLOR_AXIS_HOMED, tft_string);

  not_homed = axis_should_home(Z_AXIS);
  uint16_t offset = 25;
  if (blink && not_homed)
    tft_string.set("?");
  else {
    const float z = LOGICAL_Z_POSITION(current_position.z);
    tft_string.set(ftostr52sp((int16_t)z));
    tft_string.rtrim();
    offset += tft_string.width();

    tft_string.set(ftostr52sp(z));
    offset -= tft_string.width();
  }
  tft.add_text(301 - tft_string.width() - offset, 3, not_homed ? COLOR_AXIS_NOT_HOMED : COLOR_AXIS_HOMED, tft_string);
  TERN_(TOUCH_SCREEN, touch.add_control(MOVE_AXIS, 0, 103, 312, 24));

  // feed rate

   if((seclect==4)&&( blink))
   {
        tft.canvas(80, 136, 80, 32);
      tft.set_background(COLOR_BACKGROUND);

   }
   else
   {
      tft.canvas(80, 136, 80, 32);
      tft.set_background(COLOR_BACKGROUND);
      color = feedrate_percentage == 100 ? COLOR_RATE_100 : COLOR_RATE_ALTERED;
      tft.add_image(0, 0, imgFeedRate, color);
      tft_string.set(i16tostr3rj(feedrate_percentage));
      tft_string.add('%');
      tft.add_text(32, 6, color , tft_string);
      TERN_(TOUCH_SCREEN, touch.add_control(FEEDRATE, 80, 136, 80, 32));
   }

  // flow rate
     if((seclect==5)&&( blink))
     {
         tft.canvas(180, 136, 80, 32);
        tft.set_background(COLOR_BACKGROUND);     
     }
     else
     {
        tft.canvas(180, 136, 80, 32);
        tft.set_background(COLOR_BACKGROUND);
        color = planner.flow_percentage[0] == 100 ? COLOR_RATE_100 : COLOR_RATE_ALTERED;
        tft.add_image(0, 0, imgFlowRate, color);
        tft_string.set(i16tostr3rj(planner.flow_percentage[active_extruder]));
        tft_string.add('%');
        tft.add_text(32, 6, color , tft_string);
        TERN_(TOUCH_SCREEN, touch.add_control(FLOWRATE, 180, 136, 80, 32, active_extruder));  
     }
  //print sd card
  tft.canvas(10, 130, 48, 48);
  tft.set_background(COLOR_BACKGROUND);
  bool  card_detected = card.isMounted();
  if(card_detected)
  tft.add_image(0, 0, imgSD, COLOR_YELLOW);
  else
  tft.add_image(0, 0, imgSD, COLOR_GREY);




  // print duration
  char buffer[14];
  duration_t elapsed = print_job_timer.duration();
  elapsed.toDigital(buffer);

  tft.canvas(96, 176, 128, 20);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(buffer);
  tft.add_text(tft_string.center(128), 0, COLOR_PRINT_TIME, tft_string);
  #endif
  // progress bar
  const uint8_t progress = ui.get_progress_percent();
  tft.canvas(4, 198, 312, 9);
  tft.set_background(COLOR_PROGRESS_BG);
  tft.add_rectangle(0, 0, 312, 9, COLOR_PROGRESS_FRAME);
  if (progress)
    tft.add_bar(1, 1, (310 * progress) / 100, 7, COLOR_PROGRESS_BAR);

  // status message
  tft.canvas(0, 216, 320, 22);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(status_message);
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 1, COLOR_STATUS_MESSAGE, tft_string);

  #if ENABLED(TOUCH_SCREEN)
    add_control(256, 130, menu_main, imgSettings);
    TERN_(SDSUPPORT, add_control(0, 130, menu_media, imgSD, !printingIsActive(), COLOR_CONTROL_ENABLED, card.isMounted() && printingIsActive() ? COLOR_BUSY : COLOR_CONTROL_DISABLED));
  #endif
}

// Low-level draw_edit_screen can be used to draw an edit screen from anyplace
void MenuEditItemBase::draw_edit_screen(PGM_P const pstr, const char * const value/*=nullptr*/) {
  ui.encoder_direction_normal();
  TERN_(TOUCH_SCREEN, touch.clear());

  uint16_t line = 1;
	extern screenFunc_t _manual_move_func_ptr;
	//if(ui.currentScreen == lcd_babystep_zoffset || ui.currentScreen == lcd_move_x || ui.currentScreen == lcd_move_y || ui.currentScreen == lcd_move_z )
	//if( (ui.currentScreen != _manual_move_func_ptr && !ui.external_control&& !ui.AC_bar_control) )
	if( !ui.external_control&& !ui.AC_bar_control )
	{
	
	
    #if ENABLED(PANTALLA_NUEVA)
      tft.canvas(0, 74, TFT_WIDTH, 42);
      tft.set_background(COLOR_BACKGROUND);
      tft.add_round(8, 2, 304, 38, 6, KNE_UI_CARD);
      tft.add_bar(18, 2, 284, 2, KNE_UI_ACC);
    #else
		tft.canvas(0, 90, 320, 25);
		tft.set_background(COLOR_BACKGROUND);
    #endif
		tft_string.set(pstr, itemIndex, itemString);
		tft_string.trim();
    tft.add_text(tft_string.width() > 284 ? 18 : tft_string.center(TFT_WIDTH), 5, COLOR_MENU_TEXT, tft_string, 284);

		TERN_(AUTO_BED_LEVELING_UBL, if (ui.external_control) line++);  // ftostr52() will overwrite *value so *value has to be displayed first


    #if ENABLED(PANTALLA_NUEVA)
      tft.canvas(0, 120, TFT_WIDTH, 54);
      tft.set_background(COLOR_BACKGROUND);
      tft.add_round(8, 2, 304, 50, 6, KNE_UI_SELECT);
      tft.add_bar(8, 12, 3, 30, KNE_UI_ACC);
    #else
		tft.canvas(0, 125, 320, 25);
		tft.set_background(COLOR_BACKGROUND);
    #endif
		tft_string.set(value);
		tft_string.trim();
    tft.add_text(tft_string.width() > 284 ? 18 : tft_string.center(TFT_WIDTH),
      TERN(PANTALLA_NUEVA, 16, 5), COLOR_MENU_VALUE, tft_string, 284);
	}
	else
	{
		menu_line(line++);
		tft_string.set(pstr, itemIndex, itemString);
		tft_string.trim();
		tft.add_text(tft_string.center(TFT_WIDTH), MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);

		TERN_(AUTO_BED_LEVELING_UBL, if (ui.external_control) line++);  // ftostr52() will overwrite *value so *value has to be displayed first

		menu_line(line);
		tft_string.set(value);
		tft_string.trim();
		tft.add_text(tft_string.center(TFT_WIDTH), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

	}



  
  #if ENABLED(AUTO_BED_LEVELING_UBL)
    if (ui.external_control) {
      menu_line(line - 1);

      tft_string.set(X_LBL);
      tft.add_text(TFT_WIDTH / 2 - 120, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
      tft_string.set(ftostr52(LOGICAL_X_POSITION(current_position.x)));
      tft_string.trim();
      tft.add_text(TFT_WIDTH / 2 - 16 - tft_string.width(), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

      tft_string.set(Y_LBL);
      tft.add_text(TFT_WIDTH / 2 + 16, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
      tft_string.set(ftostr52(LOGICAL_X_POSITION(current_position.y)));
      tft_string.trim();
      tft.add_text(TFT_WIDTH / 2 + 120 - tft_string.width(), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);
    }
  #endif

  
  if (ui.currentScreen != _manual_move_func_ptr && !ui.external_control&& !ui.AC_bar_control) {

    #define SLIDER_LENGTH 224
    #define SLIDER_Y_POSITION 140

//    tft.canvas((TFT_WIDTH - SLIDER_LENGTH) / 2, SLIDER_Y_POSITION, SLIDER_LENGTH, 16);
//    tft.set_background(COLOR_BACKGROUND);

//    int16_t position = (SLIDER_LENGTH - 2) * ui.encoderPosition / maxEditValue;
//    tft.add_bar(0, 7, 1, 2, ui.encoderPosition == 0 ? COLOR_SLIDER_INACTIVE : COLOR_SLIDER);
//    tft.add_bar(1, 6, position, 4, COLOR_SLIDER);
//    tft.add_bar(position + 1, 6, SLIDER_LENGTH - 2 - position, 4, COLOR_SLIDER_INACTIVE);
//    tft.add_bar(SLIDER_LENGTH - 1, 7, 1, 2, int32_t(ui.encoderPosition) == maxEditValue ? COLOR_SLIDER : COLOR_SLIDER_INACTIVE);

    #if ENABLED(TOUCH_SCREEN)
      tft.add_image((SLIDER_LENGTH - 8) * ui.encoderPosition / maxEditValue, 0, imgSlider, COLOR_SLIDER);
      touch.add_control(SLIDER, (TFT_WIDTH - SLIDER_LENGTH) / 2, SLIDER_Y_POSITION - 8, SLIDER_LENGTH, 32, maxEditValue);
    #endif
  }

  tft.draw_edit_screen_buttons();
}

void TFT::draw_edit_screen_buttons() {
  #if ENABLED(TOUCH_SCREEN)
    add_control(32, TFT_HEIGHT - 64, DECREASE, imgDecrease);
    add_control(224, TFT_HEIGHT - 64, INCREASE, imgIncrease);
    add_control(128, TFT_HEIGHT - 64, CLICK, imgConfirm);
  #endif
}

// The Select Screen presents a prompt and two "buttons"
void MenuItem_confirm::draw_select_screen(PGM_P const yes, PGM_P const no, const bool yesno, PGM_P const pref, const char * const string/*=nullptr*/, PGM_P const suff/*=nullptr*/) {
  #if ENABLED(PANTALLA_NUEVA) && DISABLED(TOUCH_SCREEN)
    tft.canvas(0, 22, TFT_WIDTH, 56);
    tft.set_background(COLOR_BACKGROUND);
    tft.add_round(8, 2, 304, 52, 6, KNE_UI_CARD);
    tft.add_bar(18, 2, 284, 2, KNE_UI_WARN);
    tft_string.set(pref); tft_string.trim();
    tft.add_text(tft_string.width() > 280 ? 20 : tft_string.center(TFT_WIDTH), 18, COLOR_MENU_TEXT, tft_string, 280);

    tft.canvas(0, 78, TFT_WIDTH, 58);
    tft.set_background(COLOR_BACKGROUND);
    tft.add_round(8, 0, 304, 56, 6, KNE_UI_CARD);
    if (string) {
      tft_string.set(string); tft_string.trim();
      tft.add_text(tft_string.width() > 280 ? 20 : tft_string.center(TFT_WIDTH), 3, COLOR_MENU_TEXT, tft_string, 280);
    }
    if (suff) {
      tft_string.set(suff); tft_string.trim();
      tft.add_text(tft_string.width() > 280 ? 20 : tft_string.center(TFT_WIDTH), 29, COLOR_MENU_TEXT, tft_string, 280);
    }
    tft.canvas(0, 154, TFT_WIDTH, 52);
    tft.set_background(COLOR_BACKGROUND);
    for (uint8_t b = 0; b < 2; b++) {
      const uint16_t x = b ? 170 : 8;
      const bool selected = b ? yesno : !yesno;
      tft.add_round(x, 2, 142, 46, 6, selected ? KNE_UI_WARN : KNE_UI_CARD);
      tft.add_round(x + 1, 3, 140, 44, 5, selected ? KNE_UI_SELECT : KNE_UI_CARD);
      tft_string.set(b ? yes : no); tft_string.trim();
      const uint16_t w = tft_string.width();
      tft.add_text(w > 126 ? x + 8 : x + (142 - w) / 2, 15,
        selected ? KNE_UI_TEXT : KNE_UI_DIM, tft_string, 126);
    }
    return;
  #endif
  uint16_t line = 1;

  if (!string) line++;

  menu_line(line++);
  tft_string.set(pref);
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);

  if (string) {
    menu_line(line++);
    tft_string.set(string);
    tft_string.trim();
    tft.add_text(tft_string.center(TFT_WIDTH), MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
  }

  if (suff) {
    menu_line(line++);
    tft_string.set(suff);
    tft_string.trim();
    tft.add_text(tft_string.center(TFT_WIDTH), MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
  }
//  if((no)&&(yes))
//  {
//    menu_line(line);
//    tft_string.set(no);
//    tft_string.trim();
//    if(yesno)
//    {

//      tft.add_text(TFT_WIDTH/4, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);       
//      
//    }
//    else
//    {
//       tft.add_text(TFT_WIDTH/4, MENU_TEXT_Y_OFFSET, COLOR_RED, tft_string); 
//    }
//    

//    tft_string.set(yes);
//    tft_string.trim();
//    if(yesno)
//    {
//        tft.add_text(TFT_WIDTH*3/5, MENU_TEXT_Y_OFFSET, COLOR_RED, tft_string);  
//    }
//    else
//    {
//        tft.add_text(TFT_WIDTH*3/5, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);   
//    }
//  }
        //print sd card

  
      if(yesno)
      {
        tft.canvas(60, 140, 64, 64);
        tft.set_background(COLOR_BACKGROUND);
        tft.add_image(0, 0, imgCancel, COLOR_GREY);
        
        tft.canvas(180, 140, 64, 64);
        tft.set_background(COLOR_BACKGROUND);
        tft.add_image(0, 0, imgConfirm, COLOR_YELLOW);
      
      }
      else
     {
        tft.canvas(60, 140, 64, 64);
        tft.set_background(COLOR_BACKGROUND);
        tft.add_image(0, 0, imgCancel, COLOR_YELLOW);
        
        tft.canvas(180, 140, 64, 64);
        tft.set_background(COLOR_BACKGROUND);
        tft.add_image(0, 0, imgConfirm, COLOR_GREY);
     }

  #if ENABLED(TOUCH_SCREEN)
    add_control(48, TFT_HEIGHT - 64, CANCEL, imgCancel, true, yesno ? HALF(COLOR_CONTROL_CANCEL) : COLOR_CONTROL_CANCEL);
    add_control(208, TFT_HEIGHT - 64, CONFIRM, imgConfirm, true, yesno ? COLOR_CONTROL_CONFIRM : HALF(COLOR_CONTROL_CONFIRM));
  #endif
}

#if ENABLED(ADVANCED_PAUSE_FEATURE)

  void MarlinUI::draw_hotend_status(const uint8_t row, const uint8_t extruder) {
    #if ENABLED(TOUCH_SCREEN)
      touch.clear();
      draw_menu_navigation = false;
      touch.add_control(RESUME_CONTINUE , 0, 0, TFT_WIDTH, TFT_HEIGHT);
    #endif

    menu_line(row);
    tft_string.set(GET_TEXT(MSG_FILAMENT_CHANGE_NOZZLE));
//    tft_string.add('E');
//    tft_string.add((char)('1' + extruder));
    tft_string.add(' ');
    tft_string.add(i16tostr3rj(thermalManager.degHotend(extruder)));
    //tft_string.add(LCD_STR_DEGREE);
    tft_string.add(" / ");
    tft_string.add(i16tostr3rj(thermalManager.degTargetHotend(extruder)));
    //tft_string.add(LCD_STR_DEGREE);
    tft_string.trim();
    tft.add_text(tft_string.center(TFT_WIDTH), MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
  }

#endif // ADVANCED_PAUSE_FEATURE

#if ENABLED(AUTO_BED_LEVELING_UBL)
  #define GRID_OFFSET_X   8
  #define GRID_OFFSET_Y   8
  #define GRID_WIDTH      144
  #define GRID_HEIGHT     144
  #define CONTROL_OFFSET  8

  void MarlinUI::ubl_plot(const uint8_t x_plot, const uint8_t y_plot) {

    tft.canvas(GRID_OFFSET_X, GRID_OFFSET_Y, GRID_WIDTH, GRID_HEIGHT);
    tft.set_background(COLOR_BACKGROUND);
    tft.add_rectangle(0, 0, GRID_WIDTH, GRID_HEIGHT, COLOR_WHITE);

    for (uint16_t x = 0; x < (GRID_MAX_POINTS_X); x++)
      for (uint16_t y = 0; y < (GRID_MAX_POINTS_Y); y++)
        if (position_is_reachable({ ubl.mesh_index_to_xpos(x), ubl.mesh_index_to_ypos(y) }))
          tft.add_bar(1 + (x * 2 + 1) * (GRID_WIDTH - 4) / (GRID_MAX_POINTS_X) / 2, GRID_HEIGHT - 3 - ((y * 2 + 1) * (GRID_HEIGHT - 4) / (GRID_MAX_POINTS_Y) / 2), 2, 2, COLOR_UBL);

    tft.add_rectangle((x_plot * 2 + 1) * (GRID_WIDTH - 4) / (GRID_MAX_POINTS_X) / 2 - 1, GRID_HEIGHT - 5 - ((y_plot * 2 + 1) * (GRID_HEIGHT - 4) / (GRID_MAX_POINTS_Y) / 2), 6, 6, COLOR_UBL);

    const xy_pos_t pos = { ubl.mesh_index_to_xpos(x_plot), ubl.mesh_index_to_ypos(y_plot) },
                   lpos = pos.asLogical();

    tft.canvas(216, GRID_OFFSET_Y + (GRID_HEIGHT - MENU_ITEM_HEIGHT) / 2 - MENU_ITEM_HEIGHT, 96, MENU_ITEM_HEIGHT);
    tft.set_background(COLOR_BACKGROUND);
    tft_string.set(X_LBL);
    tft.add_text(0, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
    tft_string.set(ftostr52(lpos.x));
    tft_string.trim();
    tft.add_text(96 - tft_string.width(), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

    tft.canvas(216, GRID_OFFSET_Y + (GRID_HEIGHT - MENU_ITEM_HEIGHT) / 2, 96, MENU_ITEM_HEIGHT);
    tft.set_background(COLOR_BACKGROUND);
    tft_string.set(Y_LBL);
    tft.add_text(0, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
    tft_string.set(ftostr52(lpos.y));
    tft_string.trim();
    tft.add_text(96 - tft_string.width(), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

    tft.canvas(216, GRID_OFFSET_Y + (GRID_HEIGHT - MENU_ITEM_HEIGHT) / 2 + MENU_ITEM_HEIGHT, 96, MENU_ITEM_HEIGHT);
    tft.set_background(COLOR_BACKGROUND);
    tft_string.set(Z_LBL);
    tft.add_text(0, MENU_TEXT_Y_OFFSET, COLOR_MENU_TEXT, tft_string);
    tft_string.set(isnan(ubl.z_values[x_plot][y_plot]) ? "-----" : ftostr43sign(ubl.z_values[x_plot][y_plot]));
    tft_string.trim();
    tft.add_text(96 - tft_string.width(), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

    constexpr uint8_t w = (TFT_WIDTH) / 10;
    tft.canvas(GRID_OFFSET_X + (GRID_WIDTH - w) / 2, GRID_OFFSET_Y + GRID_HEIGHT + CONTROL_OFFSET - 1, w, MENU_ITEM_HEIGHT);
    tft.set_background(COLOR_BACKGROUND);
    tft_string.set(ui8tostr3rj(x_plot));
    tft_string.trim();
    tft.add_text(tft_string.center(w), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

    tft.canvas(GRID_OFFSET_X + GRID_WIDTH + CONTROL_OFFSET, GRID_OFFSET_Y + (GRID_HEIGHT - 27) / 2, w, MENU_ITEM_HEIGHT);
    tft.set_background(COLOR_BACKGROUND);
    tft_string.set(ui8tostr3rj(y_plot));
    tft_string.trim();
    tft.add_text(tft_string.center(w), MENU_TEXT_Y_OFFSET, COLOR_MENU_VALUE, tft_string);

    #if ENABLED(TOUCH_SCREEN)
      touch.clear();
      draw_menu_navigation = false;
      add_control(GRID_OFFSET_X + GRID_WIDTH + CONTROL_OFFSET,      GRID_OFFSET_Y + CONTROL_OFFSET,                    UBL,  (ENCODER_STEPS_PER_MENU_ITEM) * (GRID_MAX_POINTS_X), imgUp);
      add_control(GRID_OFFSET_X + GRID_WIDTH + CONTROL_OFFSET,      GRID_OFFSET_Y + GRID_HEIGHT - CONTROL_OFFSET - 32, UBL, -(ENCODER_STEPS_PER_MENU_ITEM) * (GRID_MAX_POINTS_X), imgDown);
      add_control(GRID_OFFSET_X + CONTROL_OFFSET,                   GRID_OFFSET_Y + GRID_HEIGHT + CONTROL_OFFSET,      UBL, -(ENCODER_STEPS_PER_MENU_ITEM), imgLeft);
      add_control(GRID_OFFSET_X + GRID_WIDTH - CONTROL_OFFSET - 32, GRID_OFFSET_Y + GRID_HEIGHT + CONTROL_OFFSET,      UBL,   ENCODER_STEPS_PER_MENU_ITEM, imgRight);
      add_control(224, GRID_OFFSET_Y + GRID_HEIGHT + CONTROL_OFFSET, CLICK, imgLeveling);
      add_control(144, 206, BACK, imgBack);
    #endif
  }
#endif // AUTO_BED_LEVELING_UBL

#if ENABLED(BABYSTEP_ZPROBE_OFFSET)
  #include "../../feature/babystep.h"
#endif

#if HAS_BED_PROBE
  #include "../../module/probe.h"
#endif

#define Z_SELECTION_Z 1
#define Z_SELECTION_Z_PROBE -1

struct MotionAxisState {
  xy_int_t xValuePos, yValuePos, zValuePos, eValuePos, stepValuePos, zTypePos, eNamePos;
  float currentStepSize = 10.0;
  int z_selection = Z_SELECTION_Z;
  uint8_t e_selection = 0;
  bool blocked = false;
  char message[32];
};

MotionAxisState motionAxisState;

#define E_BTN_COLOR COLOR_YELLOW
#define X_BTN_COLOR COLOR_CORAL_RED
#define Y_BTN_COLOR COLOR_VIVID_GREEN
#define Z_BTN_COLOR COLOR_LIGHT_BLUE

#define BTN_WIDTH 48
#define BTN_HEIGHT 39
#define X_MARGIN 15
#define Y_MARGIN 11

static void quick_feedback() {
  #if HAS_CHIRP
    ui.chirp(); // Buzz and wait. Is the delay needed for buttons to settle?
    #if BOTH(HAS_LCD_MENU, USE_BEEPER)
      for (int8_t i = 10; i--;) { buzzer.tick(); delay(2); }
    #elif HAS_LCD_MENU
      delay(10);
    #endif
  #endif
}

#define CUR_STEP_VALUE_WIDTH 38
static void drawCurStepValue() {
  tft_string.set((uint8_t *)ftostr52sp(motionAxisState.currentStepSize));
  tft.canvas(motionAxisState.stepValuePos.x, motionAxisState.stepValuePos.y, CUR_STEP_VALUE_WIDTH, 20);
  tft.set_background(COLOR_BACKGROUND);
  tft.add_text(CUR_STEP_VALUE_WIDTH - tft_string.width(), 0, COLOR_AXIS_HOMED, tft_string);
  tft.queue.sync();
  tft_string.set("mm");
  tft.canvas(motionAxisState.stepValuePos.x, motionAxisState.stepValuePos.y + 20, CUR_STEP_VALUE_WIDTH, 20);
  tft.set_background(COLOR_BACKGROUND);
  tft.add_text(CUR_STEP_VALUE_WIDTH - tft_string.width(), 0, COLOR_AXIS_HOMED, tft_string);
}

static void drawCurZSelection() {
  tft_string.set("Z");
  tft.canvas(motionAxisState.zTypePos.x, motionAxisState.zTypePos.y, tft_string.width(), 20);
  tft.set_background(COLOR_BACKGROUND);
  tft.add_text(0, 0, Z_BTN_COLOR, tft_string);
  tft.queue.sync();
  tft_string.set("Offset");
  tft.canvas(motionAxisState.zTypePos.x, motionAxisState.zTypePos.y + 34, tft_string.width(), 20);
  tft.set_background(COLOR_BACKGROUND);
  if (motionAxisState.z_selection == Z_SELECTION_Z_PROBE) {
    tft.add_text(0, 0, Z_BTN_COLOR, tft_string);
  }
}

static void drawCurESelection() {
  tft.canvas(motionAxisState.eNamePos.x, motionAxisState.eNamePos.y, BTN_WIDTH, BTN_HEIGHT);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set("E");
  tft.add_text(0, 0, E_BTN_COLOR , tft_string);
  tft.add_text(tft_string.width(), 0, E_BTN_COLOR, ui8tostr3rj(motionAxisState.e_selection));
}

static void drawMessage(const char *msg) {
  tft.canvas(X_MARGIN, TFT_HEIGHT - Y_MARGIN - 29, (TFT_WIDTH / 2) - (BTN_WIDTH / 2) - X_MARGIN, 20);
  tft.set_background(COLOR_BACKGROUND);
  tft.add_text(0, 0, COLOR_YELLOW, msg);
}

static void drawAxisValue(const AxisEnum axis) {
  const float value = (
    TERN_(HAS_BED_PROBE, axis == Z_AXIS && motionAxisState.z_selection == Z_SELECTION_Z_PROBE ? probe.offset.z :)
    ui.manual_move.axis_value(axis)
  );
  xy_int_t pos;
  uint16_t color;
  switch (axis) {
    case X_AXIS: pos = motionAxisState.xValuePos; color = X_BTN_COLOR; break;
    case Y_AXIS: pos = motionAxisState.yValuePos; color = Y_BTN_COLOR; break;
    case Z_AXIS: pos = motionAxisState.zValuePos; color = Z_BTN_COLOR; break;
    case E_AXIS: pos = motionAxisState.eValuePos; color = E_BTN_COLOR; break;
    default: return;
  }
  tft.canvas(pos.x, pos.y, BTN_WIDTH + X_MARGIN, 20);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(ftostr52sp(value));
  tft.add_text(0, 0, color, tft_string);
}

static void moveAxis(const AxisEnum axis, const int8_t direction) {
  quick_feedback();

  #if ENABLED(PREVENT_COLD_EXTRUSION)
    if (axis == E_AXIS && thermalManager.tooColdToExtrude(motionAxisState.e_selection)) {
      drawMessage("Too cold");
      return;
    }
  #endif

  const float diff = motionAxisState.currentStepSize * direction;

  if (axis == Z_AXIS && motionAxisState.z_selection == Z_SELECTION_Z_PROBE) {
    #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
      const int16_t babystep_increment = direction * BABYSTEP_SIZE_Z;
      const bool do_probe = DISABLED(BABYSTEP_HOTEND_Z_OFFSET) || active_extruder == 0;
      const float bsDiff = planner.steps_to_mm[Z_AXIS] * babystep_increment,
                  new_probe_offset = probe.offset.z + bsDiff,
                  new_offs = TERN(BABYSTEP_HOTEND_Z_OFFSET
                    , do_probe ? new_probe_offset : hotend_offset[active_extruder].z - bsDiff
                    , new_probe_offset
                  );
      if (isfinite(new_offs) && WITHIN(new_offs, Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)
          && babystep.add_steps(Z_AXIS, babystep_increment)) {
        if (do_probe)
          probe.offset.z = new_offs;
        else
          TERN(BABYSTEP_HOTEND_Z_OFFSET, hotend_offset[active_extruder].z = new_offs, NOOP);
        drawMessage(""); // clear the error
        drawAxisValue(axis);
      }
      else {
        drawMessage(GET_TEXT(MSG_LCD_SOFT_ENDSTOPS));
      }
    #elif HAS_BED_PROBE
      // only change probe.offset.z
      probe.offset.z += diff;
      if (direction < 0 && current_position[axis] < Z_PROBE_OFFSET_RANGE_MIN) {
        current_position[axis] = Z_PROBE_OFFSET_RANGE_MIN;
        drawMessage(GET_TEXT(MSG_LCD_SOFT_ENDSTOPS));
      }
      else if (direction > 0 && current_position[axis] > Z_PROBE_OFFSET_RANGE_MAX) {
        current_position[axis] = Z_PROBE_OFFSET_RANGE_MAX;
        drawMessage(GET_TEXT(MSG_LCD_SOFT_ENDSTOPS));
      }
      else {
        drawMessage(""); // clear the error
      }
      drawAxisValue(axis);
    #endif
    return;
  }

  if (!ui.manual_move.processing) {
    // Get motion limit from software endstops, if any
    float min, max;
    soft_endstop.get_manual_axis_limits(axis, min, max);

    // Delta limits XY based on the current offset from center
    // This assumes the center is 0,0
    #if ENABLED(DELTA)
      if (axis != Z_AXIS && axis != E_AXIS) {
        max = SQRT(sq((float)(DELTA_PRINTABLE_RADIUS)) - sq(current_position[Y_AXIS - axis])); // (Y_AXIS - axis) == the other axis
        min = -max;
      }
    #endif

    // Get the new position
    const bool limited = ui.manual_move.apply_diff(axis, diff, min, max);
    #if IS_KINEMATIC
      UNUSED(limited);
    #else
      PGM_P const msg = limited ? GET_TEXT(MSG_LCD_SOFT_ENDSTOPS) : NUL_STR;
      drawMessage(msg);
    #endif

    ui.manual_move.soon(axis OPTARG(MULTI_E_MANUAL, motionAxisState.e_selection));
  }

  drawAxisValue(axis);
}

static void e_plus()  { moveAxis(E_AXIS, 1);  }
static void e_minus() { moveAxis(E_AXIS, -1); }
static void x_minus() { moveAxis(X_AXIS, -1); }
static void x_plus()  { moveAxis(X_AXIS, 1);  }
static void y_plus()  { moveAxis(Y_AXIS, 1);  }
static void y_minus() { moveAxis(Y_AXIS, -1); }
static void z_plus()  { moveAxis(Z_AXIS, 1);  }
static void z_minus() { moveAxis(Z_AXIS, -1); }

#if ENABLED(TOUCH_SCREEN)
  static void e_select() {
    motionAxisState.e_selection++;
    if (motionAxisState.e_selection >= EXTRUDERS) {
      motionAxisState.e_selection = 0;
    }

    quick_feedback();
    drawCurESelection();
    drawAxisValue(E_AXIS);
  }

  static void do_home() {
    quick_feedback();
    drawMessage(GET_TEXT(MSG_LEVEL_BED_HOMING));
    queue.inject_P(G28_STR);
    // Disable touch until home is done
    TERN_(HAS_TFT_XPT2046, touch.disable());
    drawAxisValue(E_AXIS);
    drawAxisValue(X_AXIS);
    drawAxisValue(Y_AXIS);
    drawAxisValue(Z_AXIS);
  }

  static void step_size() {
    motionAxisState.currentStepSize = motionAxisState.currentStepSize / 10.0;
    if (motionAxisState.currentStepSize < 0.0015) motionAxisState.currentStepSize = 10.0;
    quick_feedback();
    drawCurStepValue();
  }
#endif

#if HAS_BED_PROBE
  static void z_select() {
    motionAxisState.z_selection *= -1;
    quick_feedback();
    drawCurZSelection();
    drawAxisValue(Z_AXIS);
  }
#endif

static void disable_steppers() {
  quick_feedback();
  queue.inject_P(PSTR("M84"));
}

static void drawBtn(int x, int y, const char *label, intptr_t data, MarlinImage img, uint16_t bgColor, bool enabled = true) {
  uint16_t width = Images[imgBtn39Rounded].width;
  uint16_t height = Images[imgBtn39Rounded].height;

  if (!enabled) bgColor = COLOR_CONTROL_DISABLED;

  tft.canvas(x, y, width, height);
  tft.set_background(COLOR_BACKGROUND);
  tft.add_image(0, 0, imgBtn39Rounded, bgColor, COLOR_BACKGROUND, COLOR_DARKGREY);

  // TODO: Make an add_text() taking a font arg
  if (label) {
    tft_string.set(label);
    tft_string.trim();
    tft.add_text(tft_string.center(width), height / 2 - tft_string.font_height() / 2, bgColor, tft_string);
  }
  else {
    tft.add_image(0, 0, img, bgColor, COLOR_BACKGROUND, COLOR_DARKGREY);
  }

  TERN_(HAS_TFT_XPT2046, if (enabled) touch.add_control(BUTTON, x, y, width, height, data));
}
void MarlinUI::move_axis_screen() {
  // Reset
  defer_status_screen(true);
  motionAxisState.blocked = false;
  TERN_(HAS_TFT_XPT2046, touch.enable());

  ui.clear_lcd();

  TERN_(TOUCH_SCREEN, touch.clear());

  const bool busy = printingIsActive();

  // Babysteps during printing? Select babystep for Z probe offset
  if (busy && ENABLED(BABYSTEP_ZPROBE_OFFSET))
    motionAxisState.z_selection = Z_SELECTION_Z_PROBE;

  // ROW 1 -> E- Y- CurY Z+
  int x = X_MARGIN, y = Y_MARGIN, spacing = 0;

  drawBtn(x, y, "E+", (intptr_t)e_plus, imgUp, E_BTN_COLOR, !busy);

  spacing = (TFT_WIDTH - X_MARGIN * 2 - 3 * BTN_WIDTH) / 2;
  x += BTN_WIDTH + spacing;
  uint16_t yplus_x = x;
  drawBtn(x, y, "Y+", (intptr_t)y_plus, imgUp, Y_BTN_COLOR, !busy);

  // Cur Y
  x += BTN_WIDTH;
  motionAxisState.yValuePos.x = x + 2;
  motionAxisState.yValuePos.y = y;
  drawAxisValue(Y_AXIS);

  x += spacing;
  drawBtn(x, y, "Z+", (intptr_t)z_plus, imgUp, Z_BTN_COLOR, !busy || ENABLED(BABYSTEP_ZPROBE_OFFSET)); //only enabled when not busy or have baby step

  // ROW 2 -> "Ex"  X-  HOME X+  "Z"
  y += BTN_HEIGHT + (TFT_HEIGHT - Y_MARGIN * 2 - 4 * BTN_HEIGHT) / 3;
  x = X_MARGIN;
  spacing = (TFT_WIDTH - X_MARGIN * 2 - 5 * BTN_WIDTH) / 4;

  motionAxisState.eNamePos.x = x;
  motionAxisState.eNamePos.y = y;
  drawCurESelection();
  TERN_(HAS_TFT_XPT2046, if (!busy) touch.add_control(BUTTON, x, y, BTN_WIDTH, BTN_HEIGHT, (intptr_t)e_select));

  x += BTN_WIDTH + spacing;
  drawBtn(x, y, "X-", (intptr_t)x_minus, imgLeft, X_BTN_COLOR, !busy);

  x += BTN_WIDTH + spacing; //imgHome is 64x64
  TERN_(HAS_TFT_XPT2046, add_control(TFT_WIDTH / 2 - Images[imgHome].width / 2, y - (Images[imgHome].width - BTN_HEIGHT) / 2, BUTTON, (intptr_t)do_home, imgHome, !busy));

  x += BTN_WIDTH + spacing;
  uint16_t xplus_x = x;
  drawBtn(x, y, "X+", (intptr_t)x_plus, imgRight, X_BTN_COLOR, !busy);

  x += BTN_WIDTH + spacing;
  motionAxisState.zTypePos.x = x;
  motionAxisState.zTypePos.y = y;
  drawCurZSelection();
  #if BOTH(HAS_BED_PROBE, TOUCH_SCREEN)
    if (!busy) touch.add_control(BUTTON, x, y, BTN_WIDTH, 34 * 2, (intptr_t)z_select);
  #endif

  // ROW 3 -> E- CurX Y-  Z-
  y += BTN_HEIGHT + (TFT_HEIGHT - Y_MARGIN * 2 - 4 * BTN_HEIGHT) / 3;
  x = X_MARGIN;
  spacing = (TFT_WIDTH - X_MARGIN * 2 - 3 * BTN_WIDTH) / 2;

  drawBtn(x, y, "E-", (intptr_t)e_minus, imgDown, E_BTN_COLOR, !busy);

  // Cur E
  motionAxisState.eValuePos.x = x;
  motionAxisState.eValuePos.y = y + BTN_HEIGHT + 2;
  drawAxisValue(E_AXIS);

  // Cur X
  motionAxisState.xValuePos.x = BTN_WIDTH + (TFT_WIDTH - X_MARGIN * 2 - 5 * BTN_WIDTH) / 4; //X- pos
  motionAxisState.xValuePos.y = y - 10;
  drawAxisValue(X_AXIS);

  x += BTN_WIDTH + spacing;
  drawBtn(x, y, "Y-", (intptr_t)y_minus, imgDown, Y_BTN_COLOR, !busy);

  x += BTN_WIDTH + spacing;
  drawBtn(x, y, "Z-", (intptr_t)z_minus, imgDown, Z_BTN_COLOR, !busy || ENABLED(BABYSTEP_ZPROBE_OFFSET)); //only enabled when not busy or have baby step

  // Cur Z
  motionAxisState.zValuePos.x = x;
  motionAxisState.zValuePos.y = y + BTN_HEIGHT + 2;
  drawAxisValue(Z_AXIS);

  // ROW 4 -> step_size  disable steppers back
  y = TFT_HEIGHT - Y_MARGIN - BTN_HEIGHT; //
  x = xplus_x - CUR_STEP_VALUE_WIDTH - 10;
  motionAxisState.stepValuePos.x = yplus_x + BTN_WIDTH - CUR_STEP_VALUE_WIDTH;
  motionAxisState.stepValuePos.y = TFT_HEIGHT - Y_MARGIN - BTN_HEIGHT;
  if (!busy) {
    drawCurStepValue();
    TERN_(HAS_TFT_XPT2046, touch.add_control(BUTTON, motionAxisState.stepValuePos.x, motionAxisState.stepValuePos.y, CUR_STEP_VALUE_WIDTH, BTN_HEIGHT, (intptr_t)step_size));
  }

  // aligned with x+
  drawBtn(xplus_x, y, "off", (intptr_t)disable_steppers, imgCancel, COLOR_WHITE, !busy);

  TERN_(HAS_TFT_XPT2046, add_control(TFT_WIDTH - X_MARGIN - BTN_WIDTH, y, BACK, imgBack));
}

#endif // HAS_UI_320x240
