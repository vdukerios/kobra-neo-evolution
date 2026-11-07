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

//
// SD Card Menu
//

#include "../../inc/MarlinConfigPre.h"
#include "powerloss.h"
#include "eeprom_api.h"
#include "planner.h"
#if BOTH(HAS_LCD_MENU, SDSUPPORT)

#include "menu_item.h"
#include "../../sd/cardreader.h"
#if ENABLED(MODOS_IMPRESION)
  #include "../../feature/modos_impresion.h"
  #include "../fontutils.h"
  #include "../tft/tft.h"   // tft_string: medir y recortar el nombre del archivo
  #include "../../feature/gcode_thumb.h"
#endif

void lcd_sd_updir() {
  ui.encoderPosition = card.cdup() ? ENCODER_STEPS_PER_MENU_ITEM : 0;
  encoderTopLine = 0;
  ui.screen_changed = true;
  ui.refresh();
}

#if ENABLED(SD_REPRINT_LAST_SELECTED_FILE)

  uint32_t sd_encoder_position = UINT32_MAX;
  int32_t sd_top_line, sd_items;

  void MarlinUI::reselect_last_file() {
    if (sd_encoder_position == UINT32_MAX) return;
    goto_screen(menu_media, sd_encoder_position, sd_top_line, sd_items);
    sd_encoder_position = UINT32_MAX;
    defer_status_screen();
    TERN_(HAS_TOUCH_SLEEP, ui.wakeup_screen());
  }

#endif

inline void sdcard_start_selected_file() {

  feedrate_percentage = 100;
  recovery.info.sdpos = 0;//Clean recovery.info.sdpos when start printing
  // Current settings were loaded and validated as one record at startup.
  // Do not restore acceleration from unrelated raw EEPROM byte offsets.
  #if ENABLED(MODOS_IMPRESION)
    modos_impresion_iniciando = true;   // el archivo se abre por cola: no tratar la salida de Confirmar como cancelacion
  #endif
  card.openAndPrintFile(card.filename);
  ui.return_to_status();
  ui.reset_status();
}

#if ENABLED(MODOS_IMPRESION)

// Recorta el nombre del archivo con "..." para que etiqueta + nombre quepan en 292 px.
// Devuelve s tal cual si cabe; si no, un buffer propio (valido hasta la siguiente llamada).
static const char* kne_nombre_ajustado(const char *s) {
  static char out[MAX_STRING_LENGTH + 1];
  tft_string.set(GET_TEXT(MSG_KNE_FILE_COLON));
  const uint16_t wlabel = tft_string.width();
  const uint16_t maxw = 292 > wlabel ? 292 - wlabel : 0;
  const uint8_t label_length = strlen((const char *)tft_string.string());
  uint32_t total = 0, characters = 0;
  uint8_t *p = (uint8_t *)s;
  while (*p) {
    wchar_t wc;
    p = get_utf8_value_cb(p, read_byte_ram, &wc);
    total += tft_string.glyph(uint8_t(wc & 0xFF))->DWidth;
    ++characters;
  }
  if (total <= maxw && characters + label_length <= MAX_STRING_LENGTH) return s;
  const uint16_t dots = 3 * tft_string.glyph(uint8_t('.'))->DWidth;
  uint16_t width = 0, cut = 0;
  characters = 0;
  p = (uint8_t *)s;
  while (*p) {
    wchar_t wc;
    uint8_t *next = get_utf8_value_cb(p, read_byte_ram, &wc);
    const uint16_t advance = tft_string.glyph(uint8_t(wc & 0xFF))->DWidth;
    if (width + advance + dots > maxw || next - (uint8_t *)s > MAX_STRING_LENGTH - 3
        || label_length + characters + 4 > MAX_STRING_LENGTH) break;
    cut = next - (uint8_t *)s;
    width += advance;
    ++characters;
    p = next;
  }
  memcpy(out, s, cut);
  out[cut] = '.'; out[cut + 1] = '.'; out[cut + 2] = '.'; out[cut + 3] = 0;
  return out;
}

// Pantalla "Confirmar impresion": archivo, modo y opciones. Es un menu normal sin dibujar la
// miniatura aqui (seria muy invasivo en este puerto); la miniatura se carga al entrar y se
// conserva para la pantalla de impresion (se descarga al cancelar, terminar o abortar).
void menu_confirmar_impresion() {   // no static: menu.cpp la usa para detectar la salida de la pantalla
  START_MENU();
  STATIC_ITEM(MSG_KNE_CONFIRM_PRINT, SS_CENTER | SS_INVERT);
  STATIC_ITEM(MSG_KNE_FILE_COLON, SS_DEFAULT, kne_nombre_ajustado(card.longest_filename()));
  ACTION_ITEM_P(print_mode == MODO_EXPRESS ? GET_TEXT(MSG_KNE_MODE_EXPRESS) : GET_TEXT(MSG_KNE_MODE_FULL), []{
    print_mode = (print_mode == MODO_EXPRESS) ? MODO_COMPLETO : MODO_EXPRESS;
    ui.refresh();
  });
  ACTION_ITEM(MSG_KNE_PRINT, sdcard_start_selected_file);
  ACTION_ITEM(MSG_KNE_CANCEL, []{
    ui.goto_previous_screen();   // modos_impresion_fin() lo llama goto_screen al salir de esta pantalla (cualquier salida)
  });
  END_MENU();
}

static void abrir_confirmar_impresion() {
  print_mode = MODO_COMPLETO;
  modos_impresion_iniciando = false;
  GcodeThumb::unload();
  GcodeThumb::load(card.filename);   // si falla, la pantalla de impresion usa el marcador de posicion
  ui.push_current_screen();   // N-07: Cancelar vuelve a la lista de archivos (antes caia en el menu principal)
  ui.goto_screen(menu_confirmar_impresion);
}

#endif // MODOS_IMPRESION

class MenuItem_sdfile : public MenuItem_sdbase {
  public:
    static inline void draw(const bool sel, const uint8_t row, PGM_P const pstr, CardReader &theCard) {
      MenuItem_sdbase::draw(sel, row, pstr, theCard, false);
    }
    static void action(PGM_P const pstr, CardReader &) {
      #if ENABLED(SD_REPRINT_LAST_SELECTED_FILE)
        // Save menu state for the selected file
        sd_encoder_position = ui.encoderPosition;
        sd_top_line = encoderTopLine;
        sd_items = screen_items;
      #endif
      #if ENABLED(MODOS_IMPRESION)
        abrir_confirmar_impresion();
        UNUSED(pstr);
      #elif ENABLED(SD_MENU_CONFIRM_START)
        MenuItem_submenu::action(pstr, []{
          char * const longest = card.longest_filename();
          char buffer[strlen(longest) + 2];
          buffer[0] = ' ';
          strcpy(buffer + 1, longest);
          MenuItem_confirm::select_screen(
            GET_TEXT(MSG_BUTTON_PRINT), GET_TEXT(MSG_BUTTON_CANCEL),
            sdcard_start_selected_file, ui.goto_previous_screen,
            GET_TEXT(MSG_START_PRINT), buffer, PSTR("?")
          );
        });
      #else
        sdcard_start_selected_file();
        UNUSED(pstr);
      #endif
    }
};

class MenuItem_sdfolder : public MenuItem_sdbase {
  public:
    static inline void draw(const bool sel, const uint8_t row, PGM_P const pstr, CardReader &theCard) {
      MenuItem_sdbase::draw(sel, row, pstr, theCard, true);
    }
    static void action(PGM_P const, CardReader &theCard) {
      card.cd(theCard.filename);
      encoderTopLine = 0;
      ui.encoderPosition = 2 * (ENCODER_STEPS_PER_MENU_ITEM);
      ui.screen_changed = true;
      TERN_(HAS_MARLINUI_U8GLIB, ui.drawing_screen = false);
      ui.refresh();
    }
};

void menu_media_filelist() {
  ui.encoder_direction_menus();

  #if HAS_MARLINUI_U8GLIB
    static uint16_t fileCnt;
    if (ui.first_page) fileCnt = card.get_num_Files();
  #else
    const uint16_t fileCnt = card.get_num_Files();
  #endif

  START_MENU();
  #if ENABLED(MULTI_VOLUME)
    ACTION_ITEM(MSG_BACK, []{ ui.goto_screen(menu_media); });
  #else
   // BACK_ITEM_P(TERN1(BROWSE_MEDIA_ON_INSERT, screen_history_depth) ? GET_TEXT(MSG_MAIN) : GET_TEXT(MSG_BACK));
   BACK_ITEM_P(GET_TEXT(MSG_BACK));
  #endif
  if (card.flag.workDirIsRoot) {
    #if !PIN_EXISTS(SD_DETECT)
      ACTION_ITEM(MSG_REFRESH, []{ encoderTopLine = 0; card.mount(); });
    #endif
  }
  else if (card.isMounted())
    ACTION_ITEM_P(PSTR(LCD_STR_FOLDER " .."), lcd_sd_updir);

  const int32_t first_file = _thisItemNr;
  const int32_t visible_file = _menuLineNr - first_file;
  if (WITHIN(visible_file, 0, int32_t(fileCnt) - 1)) {
    _thisItemNr = _menuLineNr;
    card.getfilename_sorted(SD_ORDER(uint16_t(visible_file), fileCnt));
    if (card.flag.filenameIsDir)
      MENU_ITEM(sdfolder, MSG_MEDIA_MENU, card);
    else
      MENU_ITEM(sdfile, MSG_MEDIA_MENU, card);
  }
  _thisItemNr = first_file + int32_t(fileCnt);
  END_MENU();
}

#if ENABLED(MULTI_VOLUME)
  void menu_media_select() {
    START_MENU();
    BACK_ITEM_P(TERN1(BROWSE_MEDIA_ON_INSERT, screen_history_depth) ? GET_TEXT(MSG_MAIN) : GET_TEXT(MSG_BACK));
    #if ENABLED(VOLUME_SD_ONBOARD)
      ACTION_ITEM(MSG_SD_CARD, []{ card.changeMedia(&card.media_driver_sdcard); card.mount(); ui.goto_screen(menu_media_filelist); });
    #endif
    #if ENABLED(VOLUME_USB_FLASH_DRIVE)
      ACTION_ITEM(MSG_USB_DISK, []{ card.changeMedia(&card.media_driver_usbFlash); card.mount(); ui.goto_screen(menu_media_filelist); });
    #endif
    END_MENU();
  }
#endif

void menu_media() {
   #if ENABLED(SDSUPPORT)
  const bool  card_detected = card.isMounted()
      , card_open = card_detected && card.isFileOpen();
  
      if (card_detected) {
        if (!card_open) {

             TERN(MULTI_VOLUME, menu_media_select, menu_media_filelist)();
        }
      else {
        // N-02: con un archivo abierto (impresion o pausa) no hay lista que mostrar: solo ofrecer salir
        START_MENU();
        BACK_ITEM_P(GET_TEXT(MSG_BACK));
        END_MENU();
      }
      }
      else {
				START_MENU();
				BACK_ITEM_P(GET_TEXT(MSG_BACK));
	  			STATIC_ITEM(MSG_ATTACH_MEDIA, SS_CENTER);   // N-13: antes era un segundo "Atras" identico
				//GCODES_ITEM(MSG_ATTACH_MEDIA, PSTR("M21"));  
				END_MENU();
         
      }
  #endif
}

#endif // HAS_LCD_MENU && SDSUPPORT
