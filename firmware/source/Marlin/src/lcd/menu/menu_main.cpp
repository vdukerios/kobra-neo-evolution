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
// Main Menu
//

#include "../../inc/MarlinConfigPre.h"
#include "tft.h"

#if HAS_LCD_MENU

#if HAS_LEVELING
  #include "../../module/planner.h"
  #include "../../feature/bedlevel/bedlevel.h"
#endif
#include "menu_item.h"
#include "../../module/temperature.h"
#include "../../gcode/queue.h"
#include "../../module/printcounter.h"
#include "../../module/stepper.h"
#include "../../sd/cardreader.h"

#if ENABLED(PSU_CONTROL)
  #include "../../feature/power.h"
#endif

#if HAS_GAMES && DISABLED(LCD_INFO_MENU)
  #include "game/game.h"
#endif

#if EITHER(SDSUPPORT, HOST_PROMPT_SUPPORT) || defined(ACTION_ON_CANCEL)
  #define MACHINE_CAN_STOP 1
#endif
#if ANY(SDSUPPORT, HOST_PROMPT_SUPPORT, PARK_HEAD_ON_PAUSE) || defined(ACTION_ON_PAUSE)
  #define MACHINE_CAN_PAUSE 1
#endif

#if ENABLED(MMU2_MENUS)
  #include "menu_mmu2.h"
#endif

#if ENABLED(PASSWORD_FEATURE)
  #include "../../feature/password/password.h"
#endif

#if ENABLED(HOST_START_MENU_ITEM) && defined(ACTION_ON_START)
  #include "../../feature/host_actions.h"
#endif

#if ENABLED(GCODE_REPEAT_MARKERS)
  #include "../../feature/repeat.h"
#endif
void menu_bed_leveling();
void add_bed_leveling();
void menu_preheat_m();
void menu_ready();
void menu_tune();
void menu_cancelobject();
void menu_motion();
//void menu_temperature();
void menu_configuration();
#if ENABLED(GUIA_AYUDA)
  void menu_guia();
#endif

#if HAS_POWER_MONITOR
  void menu_power_monitor();
#endif

#if ENABLED(MIXING_EXTRUDER)
  void menu_mixer();
#endif

#if ENABLED(ADVANCED_PAUSE_FEATURE)
  void menu_change_filament();
#endif

#if ENABLED(LCD_INFO_MENU)
  void menu_info();
#endif

#if EITHER(LED_CONTROL_MENU, CASE_LIGHT_MENU)
  void menu_led();
#endif

#if HAS_CUTTER
  void menu_spindle_laser();
#endif

#if ENABLED(PREHEAT_SHORTCUT_MENU_ITEM)
  void menu_preheat_only();
#endif

#if HAS_MULTI_LANGUAGE
  void menu_language();
#endif


#if ENABLED(CUSTOM_MENU_MAIN)

  void _lcd_custom_menu_main_gcode(PGM_P const cmd) {
    queue.inject_P(cmd);
    TERN_(CUSTOM_MENU_MAIN_SCRIPT_AUDIBLE_FEEDBACK, ui.completion_feedback());
    TERN_(CUSTOM_MENU_MAIN_SCRIPT_RETURN, ui.return_to_status());
  }

  void custom_menus_main() {
    START_MENU();
    BACK_ITEM(MSG_MAIN);

    #define HAS_CUSTOM_ITEM_MAIN(N) (defined(MAIN_MENU_ITEM_##N##_DESC) && defined(MAIN_MENU_ITEM_##N##_GCODE))

    #ifdef CUSTOM_MENU_MAIN_SCRIPT_DONE
      #define _DONE_SCRIPT "\n" CUSTOM_MENU_MAIN_SCRIPT_DONE
    #else
      #define _DONE_SCRIPT ""
    #endif
    #define GCODE_LAMBDA_MAIN(N) []{ _lcd_custom_menu_main_gcode(PSTR(MAIN_MENU_ITEM_##N##_GCODE _DONE_SCRIPT)); }
    #define _CUSTOM_ITEM_MAIN(N) ACTION_ITEM_P(PSTR(MAIN_MENU_ITEM_##N##_DESC), GCODE_LAMBDA_MAIN(N));
    #define _CUSTOM_ITEM_MAIN_CONFIRM(N)             \
      SUBMENU_P(PSTR(MAIN_MENU_ITEM_##N##_DESC), []{ \
          MenuItem_confirm::confirm_screen(          \
            GCODE_LAMBDA_MAIN(N),                    \
            ui.goto_previous_screen,                 \
            PSTR(MAIN_MENU_ITEM_##N##_DESC "?")      \
          );                                         \
        })

    #define CUSTOM_ITEM_MAIN(N) do{ \
      constexpr char c = MAIN_MENU_ITEM_##N##_GCODE[strlen(MAIN_MENU_ITEM_##N##_GCODE) - 1]; \
      static_assert(c != '\n' && c != '\r', "MAIN_MENU_ITEM_" STRINGIFY(N) "_GCODE cannot have a newline at the end. Please remove it."); \
      if (ENABLED(MAIN_MENU_ITEM_##N##_CONFIRM)) \
        _CUSTOM_ITEM_MAIN_CONFIRM(N); \
      else \
        _CUSTOM_ITEM_MAIN(N); \
    }while(0)

    #if HAS_CUSTOM_ITEM_MAIN(1)
      CUSTOM_ITEM_MAIN(1);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(2)
      CUSTOM_ITEM_MAIN(2);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(3)
      CUSTOM_ITEM_MAIN(3);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(4)
      CUSTOM_ITEM_MAIN(4);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(5)
      CUSTOM_ITEM_MAIN(5);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(6)
      CUSTOM_ITEM_MAIN(6);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(7)
      CUSTOM_ITEM_MAIN(7);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(8)
      CUSTOM_ITEM_MAIN(8);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(9)
      CUSTOM_ITEM_MAIN(9);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(10)
      CUSTOM_ITEM_MAIN(10);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(11)
      CUSTOM_ITEM_MAIN(11);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(12)
      CUSTOM_ITEM_MAIN(12);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(13)
      CUSTOM_ITEM_MAIN(13);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(14)
      CUSTOM_ITEM_MAIN(14);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(15)
      CUSTOM_ITEM_MAIN(15);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(16)
      CUSTOM_ITEM_MAIN(16);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(17)
      CUSTOM_ITEM_MAIN(17);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(18)
      CUSTOM_ITEM_MAIN(18);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(19)
      CUSTOM_ITEM_MAIN(19);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(20)
      CUSTOM_ITEM_MAIN(20);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(21)
      CUSTOM_ITEM_MAIN(21);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(22)
      CUSTOM_ITEM_MAIN(22);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(23)
      CUSTOM_ITEM_MAIN(23);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(24)
      CUSTOM_ITEM_MAIN(24);
    #endif
    #if HAS_CUSTOM_ITEM_MAIN(25)
      CUSTOM_ITEM_MAIN(25);
    #endif
    END_MENU();
  }

#endif // CUSTOM_MENU_MAIN

void version_message(){
	
	ui.defer_status_screen();
	//tft.fill(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BACKGROUND);
	if (ui.should_draw()){
		
			tft.canvas(0, 40, TFT_WIDTH, 30);		
			tft.set_background(COLOR_BACKGROUND);
			tft_string.set(DEVICE_NAME);
			tft_string.trim();
			tft.add_text(tft_string.center(TFT_WIDTH),5,COLOR_WHITE,tft_string);

			tft.canvas(0, 80, TFT_WIDTH, 30);
			tft.set_background(COLOR_BACKGROUND);
			tft_string.set(FIRMWARE_VER);
			tft_string.trim();
			tft.add_text(tft_string.center(TFT_WIDTH),5,COLOR_WHITE,tft_string);
			
			tft.canvas(0, 120, TFT_WIDTH, 30);
			tft.set_background(COLOR_BACKGROUND);
			tft_string.set(BUILD_VOLUME);
			tft_string.trim();
			tft.add_text(tft_string.center(TFT_WIDTH),5,COLOR_WHITE,tft_string);

			tft.canvas(0, 160, TFT_WIDTH, 30);
			tft.set_background(COLOR_BACKGROUND);
			tft_string.set(TECH_SUPPORT);
			tft_string.trim();
			tft.add_text(tft_string.center(TFT_WIDTH),5,COLOR_WHITE,tft_string);
			//ui.refresh(LCDVIEW_CALL_REDRAW_NEXT);
	}
	if(ui.use_click()) {ui.goto_previous_screen();}   // N-13: volver al menu principal, no a estado

}


#if ENABLED(BOTON_NIVELAR_CENTRAR)

//
// Boton "Nivelar y centrar" (bandera BOTON_NIVELAR_CENTRAR en inc/valentin/boton_nivelar.h)
//
// Maquina de estados movida desde MarlinUI::update() (boton_nivelar_tick), porque G29 termina con
// un mensaje de alerta que devuelve la pantalla al estado y la pantalla propia no seguiria llamandose.
// Se encola un solo tramo cada vez (maximo 4 lineas: BUFSIZE = 4) y se avanza cuando la cola y el
// planificador estan vacios. Solo se mueve al centro si G29 termino bien y no hubo cancelacion.
//
#include "../../module/motion.h"
#include "../../module/probe.h"

#if DISABLED(BABYSTEP_ZPROBE_OFFSET)
  #error "BOTON_NIVELAR_CENTRAR requiere BABYSTEP_ZPROBE_OFFSET (pantalla de ajuste de Z offset)."
#endif

extern float Beforeprobe_offset; // definida en menu.cpp: lcd_babystep_zoffset guarda (M500) si cambia

enum : uint8_t {
  BNC_IDLE = 0,
  BNC_INICIO,     // pedido recibido, aun no se encolo nada
  BNC_ORIGEN,     // G28 en curso
  BNC_MALLA,      // G29 en curso
  BNC_CENTRAR,    // subiendo a Z seguro y yendo al centro
  BNC_BAJAR,      // bajando a Z de trabajo
  BNC_CANCELANDO, // cancelacion pedida, esperando a que pare lo que esta en curso
  BNC_AJUSTE      // pantalla de ajuste de Z offset abierta
};

bool boton_nivelar_cancelado = false; // leida por G29 para abortar el sondeo
extern bool g29_fallo;   // definida en G29.cpp: la ultima medicion de la malla fallo
static uint8_t bnc_fase = BNC_IDLE;
static bool bnc_restaurar = false;     // restaurar el offset previo si se cancela antes de terminar la malla
static float bnc_offset_previo = 0;

void boton_nivelar_pantalla();

static bool bnc_ocupado() { return queue.has_commands_queued() || planner.has_blocks_queued(); }

// Copia de la malla de nivelacion: G29 la borra (y "G29 J" tras un fallo o cancelacion). Si se cancela
// antes de terminar la malla nueva se devuelve la anterior a la RAM.
#if ENABLED(AUTO_BED_LEVELING_BILINEAR)
  static bed_mesh_t bnc_malla_previa;
  static xy_pos_t bnc_inicio_previo, bnc_espaciado_previo;
  static bool bnc_malla_guardada = false, bnc_nivel_previo = false;
#endif

static void bnc_malla_guardar() {
  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    memcpy(bnc_malla_previa, z_values, sizeof(bnc_malla_previa));
    bnc_inicio_previo = bilinear_start;
    bnc_espaciado_previo = bilinear_grid_spacing;
    bnc_nivel_previo = planner.leveling_active;
    bnc_malla_guardada = true;
  #endif
}

static void bnc_malla_restaurar() {
  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    if (!bnc_malla_guardada) return;
    bnc_malla_guardada = false;
    memcpy(z_values, bnc_malla_previa, sizeof(z_values));
    bilinear_start = bnc_inicio_previo;
    bilinear_grid_spacing = bnc_espaciado_previo;
    if (bilinear_grid_spacing.x) {          // solo si habia una malla valida antes
      refresh_bed_level();
      set_bed_leveling_enabled(bnc_nivel_previo);
    }
  #endif
}

static void bnc_fin(const bool cancelado) {
  if (bnc_restaurar) {
    probe.offset.z = bnc_offset_previo;
    bnc_malla_restaurar();                       // y devolver la malla anterior a la RAM
  }
  bnc_restaurar = false;
  boton_nivelar_cancelado = false;
  bnc_fase = BNC_IDLE;
  if (cancelado) ui.set_status_P(GET_TEXT(MSG_BNC_CANCELADO), -1);
  ui.return_to_status();
}

static void bnc_linea(const uint16_t y, const char * const texto) {
  tft.canvas(0, y, TFT_WIDTH, 30);
  tft.set_background(COLOR_BACKGROUND);
  tft_string.set(texto);
  tft_string.trim();
  tft.add_text(tft_string.center(TFT_WIDTH), 5, COLOR_WHITE, tft_string);
}

// Pantalla de estado: cualquier pulsacion cancela
void boton_nivelar_pantalla() {
  ui.defer_status_screen();

  if (bnc_fase == BNC_IDLE || bnc_fase == BNC_AJUSTE) {
    // Se vuelve aqui al salir de la pantalla de ajuste (ya guardado): regresar al menu anterior.
    ui.goto_previous_screen_no_defer();
    return;
  }

  if (ui.use_click()) {
    if (bnc_fase != BNC_CANCELANDO) {
      bnc_restaurar = (bnc_fase <= BNC_MALLA);      // si la malla no termino, devolver el offset anterior
      bnc_fase = BNC_CANCELANDO;
      boton_nivelar_cancelado = true;               // G29 aborta en el siguiente punto
      wait_for_heatup = false;                      // corta la espera de calentamiento
      queue.clear();                                // descarta lo que aun no empezo
    }
    return;
  }

  if (ui.should_draw()) {
    const char *t;
    switch (bnc_fase) {
      case BNC_MALLA:
        t = GET_TEXT(MSG_BNC_MALLA);
        #if ENABLED(PREHEAT_BEFORE_LEVELING)
          if (ui.preheating_start) t = GET_TEXT(MSG_WAIT_LEVELING_HEAT);
        #endif
        break;
      case BNC_CENTRAR:    t = GET_TEXT(MSG_BNC_CENTRANDO); break;
      case BNC_BAJAR:      t = GET_TEXT(MSG_BNC_BAJANDO);   break;
      case BNC_CANCELANDO: t = GET_TEXT(MSG_BNC_CANCELANDO); break;
      default:             t = GET_TEXT(MSG_BNC_ORIGEN);    break;
    }
    bnc_linea(80, t);
    bnc_linea(140, GET_TEXT(MSG_BNC_CANCELAR));
  }
  ui.refresh(LCDVIEW_CALL_REDRAW_NEXT);
}

// Llamada desde MarlinUI::update()
void boton_nivelar_tick() {
  if (bnc_fase == BNC_IDLE) return;

  if (bnc_fase == BNC_AJUSTE) {                      // el usuario salio de la pantalla de ajuste
    if (ui.currentScreen != lcd_babystep_zoffset) bnc_fase = BNC_IDLE;
    return;
  }

  // G29 devuelve la pantalla al estado al terminar: volver a la pantalla propia
  if (ui.currentScreen != boton_nivelar_pantalla) ui.goto_screen(boton_nivelar_pantalla);

  if (bnc_ocupado()) return;                         // algo en curso (G28, G29 o movimientos)

  switch (bnc_fase) {
    case BNC_INICIO:
      // Se CONSERVA el offset actual durante todo el proceso: asi nunca queda un offset 0 guardado en la EEPROM
      // (G29 termina con M500) y la boquilla no puede quedar hundida en la cama si el usuario sale sin ajustar.
      if (all_axes_trusted()) {
        queue.enqueue_now_P(PSTR("G29"));
        bnc_fase = BNC_MALLA;
      }
      else {
        queue.enqueue_now_P(PSTR("G28"));
        bnc_fase = BNC_ORIGEN;
      }
      break;

    case BNC_ORIGEN:
      queue.enqueue_now_P(PSTR("G29"));              // G29 termina con M500 y G28 (Z_PROBE_END_SCRIPT)
      bnc_fase = BNC_MALLA;
      break;

    case BNC_MALLA:
      if (probe.status == -1 || g29_fallo || boton_nivelar_cancelado) {
        bnc_fin(boton_nivelar_cancelado);            // fallo de G29: no se mueve nada
      }
      else {
        bnc_restaurar = false;                       // la malla ya quedo medida y guardada con offset 0
        queue.enqueue_now_P(PSTR("M420 S1\nG90\nG1 Z" BNC_Z_SEGURO " F" BNC_F_Z_SUBIR "\nG1 X" BNC_CENTRO_X " Y" BNC_CENTRO_Y " F" BNC_F_XY));
        bnc_fase = BNC_CENTRAR;
      }
      break;

    case BNC_CENTRAR:
      queue.enqueue_now_P(PSTR("G1 Z" BNC_Z_INTERMEDIA " F" BNC_F_Z_BAJAR "\nG1 Z" BNC_Z_TRABAJO " F" BNC_F_Z_FINAL));
      bnc_fase = BNC_BAJAR;
      break;

    case BNC_BAJAR:
      Beforeprobe_offset = probe.offset.z;           // asi lcd_babystep_zoffset guarda (M500) al confirmar si cambio
      bnc_fase = BNC_AJUSTE;
      ui.goto_screen(lcd_babystep_zoffset);
      break;

    case BNC_CANCELANDO:
      bnc_fin(true);
      break;

    default: break;
  }
}

// Accion del menu principal
void boton_nivelar_inicio() {
  if (printingIsActive() || printingIsPaused() || bnc_ocupado()) {
    ui.set_status_P(GET_TEXT(MSG_BNC_OCUPADO), -1);
    ui.return_to_status();
    return;
  }
  bnc_offset_previo = probe.offset.z;
  bnc_malla_guardar();
  bnc_restaurar = true;
  boton_nivelar_cancelado = false;
  bnc_fase = BNC_INICIO;
  ui.defer_status_screen();
  ui.goto_screen(boton_nivelar_pantalla);
}

#endif // BOTON_NIVELAR_CENTRAR

uint32_t ac_touch_ms;
bool ac_touch_flag = false;

#define MEDIA_MENU_AT_TOP
#if HAS_SERVICE_INTERVALS
// Submenu "Mantenimiento": recordatorios de servicio (reiniciar el contador tras hacerlo)
void menu_mantenimiento() {
  static auto _service_reset = [](const int index) {
    print_job_timer.resetServiceInterval(index);
    ui.completion_feedback();
    ui.reset_status();
    ui.return_to_status();
  };
  START_MENU();
  BACK_ITEM(MSG_BACK);
  #if SERVICE_INTERVAL_1 > 0
    CONFIRM_ITEM(MSG_KNE_SERVICE_1,
      MSG_BUTTON_RESET, MSG_BUTTON_CANCEL,
      []{ _service_reset(1); }, ui.goto_previous_screen,
      GET_TEXT(MSG_SERVICE_RESET), GET_TEXT(MSG_KNE_SERVICE_1), PSTR("?")
    );
  #endif
  #if SERVICE_INTERVAL_2 > 0
    CONFIRM_ITEM(MSG_KNE_SERVICE_2,
      MSG_BUTTON_RESET, MSG_BUTTON_CANCEL,
      []{ _service_reset(2); }, ui.goto_previous_screen,
      GET_TEXT(MSG_SERVICE_RESET), GET_TEXT(MSG_KNE_SERVICE_2), PSTR("?")
    );
  #endif
  END_MENU();
}
#endif // HAS_SERVICE_INTERVALS

void menu_main() {
  const bool busy = (printingIsActive() || printingIsPaused())  // N-03: en pausa tambien se ofrece el menu de impresion (Reanudar/Detener/Ajustar)
    #if ENABLED(SDSUPPORT)
      , card_detected = card.isMounted()
      , card_open = card_detected && card.isFileOpen()
    #endif
  ;

  START_MENU();
  BACK_ITEM(MSG_INFO_SCREEN);   //�ŵ������ڶ�  

  if (busy) {
    #if MACHINE_CAN_PAUSE
      if (printingIsPaused()) ACTION_ITEM(MSG_RESUME_PRINT, ui.resume_print);   // N-03: en pausa se ofrece Reanudar
      else ACTION_ITEM(MSG_PAUSE_PRINT, ui.pause_print);
    #endif
    #if MACHINE_CAN_STOP
      SUBMENU(MSG_STOP_PRINT, []{
        MenuItem_confirm::select_screen(
          GET_TEXT(MSG_BUTTON_STOP), GET_TEXT(MSG_BACK),
          ui.abort_print, ui.goto_previous_screen,
          GET_TEXT(MSG_STOP_PRINT), (const char *)nullptr, PSTR("?")
        );
      });
    #endif

    #if ENABLED(GCODE_REPEAT_MARKERS)
      if (repeat.is_active())
        ACTION_ITEM(MSG_END_LOOPS, repeat.cancel);
    #endif

    SUBMENU(MSG_TUNE, menu_tune);
    // N-11: modo noche y guia tambien disponibles imprimiendo o en pausa
    #if HAS_MODO_NOCHE
      EDIT_ITEM(bool, MSG_KNE_NIGHT_MODE, &ui.noche_activo, []{ ui.noche_aplicar(); });
    #endif
    #if ENABLED(GUIA_AYUDA)
      SUBMENU(MSG_KNE_GUIDE_HELP, menu_guia);
    #endif

    #if ENABLED(CANCEL_OBJECTS) && DISABLED(SLIM_LCD_MENUS)
      SUBMENU(MSG_CANCEL_OBJECT, []{ editable.int8 = -1; ui.goto_screen(menu_cancelobject); });
    #endif
	 //BACK_ITEM(MSG_INFO_SCREEN);    //��Ϣ����
  }
  else {
  	
        SUBMENU(MSG_MEDIA_MENU, MEDIA_MENU_GATEWAY);
		SUBMENU(MSG_MOVE_AXIS, menu_move);
		#if ENABLED(BOTON_NIVELAR_CENTRAR)
			ACTION_ITEM(MSG_NIVELAR_CENTRAR, boton_nivelar_inicio);
		#endif
//      if (card_detected) {
//        if (!card_open) {

//          SUBMENU(MSG_MEDIA_MENU, MEDIA_MENU_GATEWAY);        // Media Menu (or Password First)
//        }
//      }
//      else {

//          GCODES_ITEM(MSG_ATTACH_MEDIA, PSTR("M21"));  
//      }

    if (TERN0(MACHINE_CAN_PAUSE, printingIsPaused()))
      ACTION_ITEM(MSG_RESUME_PRINT, ui.resume_print);

    #if ENABLED(HOST_START_MENU_ITEM) && defined(ACTION_ON_START)
      ACTION_ITEM(MSG_HOST_START_PRINT, host_action_start);
    #endif

    #if ENABLED(PREHEAT_SHORTCUT_MENU_ITEM)
      SUBMENU(MSG_PREHEAT_CUSTOM, menu_preheat_only);
    #endif
    
  //  SUBMENU(MSG_TUNE, menu_tune);
    SUBMENU(MSG_PREPARE, menu_ready);
    //SUBMENU(Operate_MSG, menu_motion);
    SUBMENU(MSG_CONFIGURATION, menu_configuration);
    #if HAS_MODO_NOCHE
      EDIT_ITEM(bool, MSG_KNE_NIGHT_MODE, &ui.noche_activo, []{ ui.noche_aplicar(); });
    #endif
    #if ENABLED(GUIA_AYUDA)
      SUBMENU(MSG_KNE_GUIDE_HELP, menu_guia);
    #endif
	
		
  //BACK_ITEM(MSG_INFO_SCREEN);    //��Ϣ����
 #if HAS_SERVICE_INTERVALS
   SUBMENU(MSG_KNE_MAINTENANCE, menu_mantenimiento);   // agrupa Lubricar ejes / Revisar correas
 #endif
  END_MENU();
  }

//  #if HAS_CUTTER
//    SUBMENU(MSG_CUTTER(MENU), STICKY_SCREEN(menu_spindle_laser));
//  #endif

//  #if HAS_TEMPERATURE
//    SUBMENU(MSG_TEMPERATURE, menu_temperature);   //�¶ȷŵ�����
//  #endif

//  #if HAS_POWER_MONITOR
//    SUBMENU(MSG_POWER_MONITOR, menu_power_monitor);
//  #endif

  #if ENABLED(MIXING_EXTRUDER)
    SUBMENU(MSG_MIXER, menu_mixer);
  #endif

  #if ENABLED(MMU2_MENUS)
    if (!busy) SUBMENU(MSG_MMU2_MENU, menu_mmu2);
  #endif



//  #if ENABLED(CUSTOM_MENU_MAIN)
//    if (TERN1(CUSTOM_MENU_MAIN_ONLY_IDLE, !busy)) {
//      #ifdef CUSTOM_MENU_MAIN_TITLE
//        SUBMENU_P(PSTR(CUSTOM_MENU_MAIN_TITLE), custom_menus_main);
//      #else
//        SUBMENU(MSG_CUSTOM_COMMANDS, custom_menus_main);
//      #endif
//    }
//  #endif

//  #if ENABLED(ADVANCED_PAUSE_FEATURE)                //�����Ĳĸĵ��Ǹ�׼������
//    #if E_STEPPERS == 1 && DISABLED(FILAMENT_LOAD_UNLOAD_GCODES)
//      YESNO_ITEM(MSG_FILAMENTCHANGE,
//        menu_change_filament, ui.goto_previous_screen,
//        GET_TEXT(MSG_FILAMENTCHANGE), (const char *)nullptr, PSTR("?")
//      );
//    #else
//      SUBMENU(MSG_FILAMENTCHANGE, menu_change_filament);
//    #endif
//  #endif

//  #if ENABLED(LCD_INFO_MENU)
//    SUBMENU(MSG_INFO_MENU, menu_info);
//  #endif

  #if EITHER(LED_CONTROL_MENU, CASE_LIGHT_MENU)
    SUBMENU(MSG_LEDS, menu_led);
  #endif

  //
  // Switch power on/off
  //
  #if ENABLED(PSU_CONTROL)
    if (powerManager.psu_on)
      #if ENABLED(PS_OFF_CONFIRM)
        CONFIRM_ITEM(MSG_SWITCH_PS_OFF,
          MSG_YES, MSG_NO,
          ui.poweroff, ui.goto_previous_screen,
          GET_TEXT(MSG_SWITCH_PS_OFF), (const char *)nullptr, PSTR("?")
        );
      #else
        GCODES_ITEM(MSG_SWITCH_PS_OFF, PSTR("M81"));
      #endif
    else
      GCODES_ITEM(MSG_SWITCH_PS_ON, PSTR("M80"));
  #endif

//  #if ENABLED(SDSUPPORT) && DISABLED(MEDIA_MENU_AT_TOP)
//    sdcard_menu_items();
//  #endif


 #endif

  #if HAS_GAMES && DISABLED(LCD_INFO_MENU)
   #if ENABLED(GAMES_EASTER_EGG)
     SKIP_ITEM();
     SKIP_ITEM();
     SKIP_ITEM();
   #endif
   // Game sub-menu or the individual game
   {
     SUBMENU(
       #if HAS_GAME_MENU
         MSG_GAMES, menu_game
       #elif ENABLED(MARLIN_BRICKOUT)
         MSG_BRICKOUT, brickout.enter_game
       #elif ENABLED(MARLIN_INVADERS)
         MSG_INVADERS, invaders.enter_game
       #elif ENABLED(MARLIN_SNAKE)
         MSG_SNAKE, snake.enter_game
       #elif ENABLED(MARLIN_MAZE)
         MSG_MAZE, maze.enter_game
       #endif
     );
   }
 #endif
	

}

void preheat_pla()
{
	 queue.inject_P(PSTR("M140 S60\nM104 S190"));
   ui.return_to_status();
}

void preheat_ABS()
{
		queue.inject_P(PSTR("M140 S80\nM104 S240"));
   	ui.return_to_status();
}

//xtern _filament_cmd_t filament_cmd;
 extern uint8_t filament_cmd;

 void menu_ready()
 {
  START_MENU();
  BACK_ITEM(MSG_BACK);
   
//    if (!g29_in_progress)
//      SUBMENU(MSG_BED_LEVELING, menu_bed_leveling);
//*********************Ԥ��***************************************
  
	SUBMENU(MSG_PREHEAT_PLA,preheat_pla);
	SUBMENU(MSG_PREHEAT_ABS,preheat_ABS);
	
//  #if PREHEAT_COUNT
//    //
//    // Preheat for all Materials
//    //
//    LOOP_L_N(m, PREHEAT_COUNT) {
//      editable.int8 = m;
//      #if HOTENDS > 1 || HAS_HEATED_BED
//        SUBMENU_S(ui.get_preheat_label(m), MSG_PREHEAT_M, menu_preheat_m);
//      #elif HAS_HOTEND
//        ACTION_ITEM_S(ui.get_preheat_label(m), MSG_PREHEAT_M, do_preheat_end_m);
//      #endif
//    }
//  #endif
 //************************��˿****************************************** 
//   #if ENABLED(ADVANCED_PAUSE_FEATURE)                //��˿
//    #if E_STEPPERS == 1 && DISABLED(FILAMENT_LOAD_UNLOAD_GCODES)
//      YESNO_ITEM(MSG_FILAMENTCHANGE,
//        menu_change_filament, ui.goto_previous_screen,
//        GET_TEXT(MSG_FILAMENTCHANGE), (const char *)nullptr, PSTR("?")
//      );
//    #else
//      SUBMENU(MSG_FILAMENTCHANGE, menu_change_filament);
//    #endif
//  #endif
	
  SUBMENU(MSG_MOVE_E_IN, []{filament_cmd = FILA_IN; _menu_move_distance_e_maybe(); }); 
  SUBMENU(MSG_MOVE_E_OUT, []{filament_cmd = FILA_OUT; _menu_move_distance_e_maybe();});
  END_MENU();
 }
 

#endif // HAS_LCD_MENU
