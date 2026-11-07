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
// Motion Menu
//

#include "../../inc/MarlinConfigPre.h"

#include "tft.h"
#include "gcode.h"

#if HAS_LCD_MENU

#define LARGE_AREA_TEST ((X_BED_SIZE) >= 1000 || (Y_BED_SIZE) >= 1000 || (Z_MAX_POS) >= 1000)

#include "menu_item.h"
#include "menu_addon.h"

#include "../../module/motion.h"
#include "../../gcode/parser.h" // for inch support

  #if ENABLED(BLTOUCH)
    #include "../../feature/bltouch.h"
  #endif

#if ENABLED(DELTA)
  #include "../../module/delta.h"
#endif

#include "../../module/temperature.h"
#include "../../MarlinCore.h"
#include "../../gcode/queue.h"

#if HAS_LEVELING
  #include "../../module/planner.h"
  #include "../../feature/bedlevel/bedlevel.h"
#endif

#if ENABLED(MANUAL_E_MOVES_RELATIVE)
  float manual_move_e_origin = 0;
#endif
void menu_temperature();
//
// "Motion" > "Move Axis" submenu
//
#define MAX_move_xyz 20.0

 void _lcd_move_xyz(PGM_P const name, const AxisEnum axis) {
  float pos;
  if (ui.use_click())
  { 
      ui.enable_encoder_multiplier(false);
    return  ui.goto_previous_screen_no_defer();
  }
  if (ui.encoderPosition && !ui.manual_move.processing) {
    // Get motion limit from software endstops, if any
    float min, max;
    soft_endstop.get_manual_axis_limits(axis, min, max);
	min =  min < 0? 0:min;
	
	// Preserve the physical reference when moving a single axis.
   
    // Delta limits XY based on the current offset from center
    // This assumes the center is 0,0
    #if ENABLED(DELTA)
      if (axis != Z_AXIS) {
        max = SQRT(sq((float)(DELTA_PRINTABLE_RADIUS)) - sq(current_position[Y_AXIS - axis])); // (Y_AXIS - axis) == the other axis
        min = -max;
      }
    #endif

    // Get the new position
     float diff = float(int32_t(ui.encoderPosition)) * ui.manual_move.menu_scale;
    if(diff>(MAX_move_xyz))
    {
      diff=(float)(MAX_move_xyz);
    }
    else if(diff<(-MAX_move_xyz))
    {
      diff=(float)(-MAX_move_xyz);
    }
    (void)ui.manual_move.apply_diff(axis, diff, min, max);
    ui.manual_move.soon(axis);
    ui.refresh(LCDVIEW_REDRAW_NOW);
  }
  ui.encoderPosition = 0;
  if (ui.should_draw()) {
 //   const float pos;
    pos= ui.manual_move.axis_value(axis);
 	pos = pos <0? 0:pos;
    if (parser.using_inch_units()) {
      const float imp_pos = LINEAR_UNIT(pos);
      MenuEditItemBase::draw_edit_screen(name, ftostr63(imp_pos));
    }
    else
      MenuEditItemBase::draw_edit_screen(name, ui.manual_move.menu_scale >= 0.1f ? (LARGE_AREA_TEST ? ftostr51sign(pos) : ftostr41sign(pos)) : ftostr63(pos));
  }
}
void lcd_move_x() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_X), X_AXIS); }
#if HAS_Y_AXIS
  void lcd_move_y() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_Y), Y_AXIS); }
#endif
#if HAS_Z_AXIS
  void lcd_move_z() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_Z), Z_AXIS); }
#endif
#if LINEAR_AXES >= 4
  void lcd_move_i() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_I), I_AXIS); }
#endif
#if LINEAR_AXES >= 5
  void lcd_move_j() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_J), J_AXIS); }
#endif
#if LINEAR_AXES >= 6
  void lcd_move_k() { _lcd_move_xyz(GET_TEXT(MSG_MOVE_K), K_AXIS); }
#endif

#if E_MANUAL

  static void lcd_move_e(TERN_(MULTI_E_MANUAL, const int8_t eindex=active_extruder)) {
    if (ui.use_click()) 
    {
      //ui.enable_encoder_multiplier(false);
      return ui.goto_previous_screen_no_defer();
    }
    if (ui.encoderPosition) {
      if (!ui.manual_move.processing) {
         float diff = float(int32_t(ui.encoderPosition)) * ui.manual_move.menu_scale;
       if(diff>(MAX_move_xyz))
        {
          diff=(float)(MAX_move_xyz);
        }
        else if(diff<(-MAX_move_xyz))
        {
          diff=(float)(-MAX_move_xyz);
        }   
        
        TERN(IS_KINEMATIC, ui.manual_move.offset, current_position.e) += diff;
        ui.manual_move.soon(E_AXIS OPTARG(MULTI_E_MANUAL, eindex));
        ui.refresh(LCDVIEW_REDRAW_NOW);
      }
      ui.encoderPosition = 0;
    }
    if (ui.should_draw()) {
      TERN_(MULTI_E_MANUAL, MenuItemBase::init(eindex));
      MenuEditItemBase::draw_edit_screen(
        GET_TEXT(TERN(MULTI_E_MANUAL, MSG_MOVE_EN, MSG_MOVE_E)),
        ftostr41sign(current_position.e
          PLUS_TERN0(IS_KINEMATIC, ui.manual_move.offset)
          MINUS_TERN0(MANUAL_E_MOVES_RELATIVE, manual_move_e_origin)
        )
      );
    } // should_draw
  }

#endif // E_MANUAL

//
// "Motion" > "Move Xmm" > "Move XYZ" submenu
//

#ifndef FINE_MANUAL_MOVE
  #define FINE_MANUAL_MOVE 0.025
#endif

screenFunc_t _manual_move_func_ptr;

void _goto_manual_move(const_float_t scale) {
  ui.defer_status_screen();
  ui.manual_move.menu_scale = scale;
  ui.enable_encoder_multiplier(true);
  ui.goto_screen(_manual_move_func_ptr);
}

void _menu_move_distance(const AxisEnum axis, const screenFunc_t func, const int8_t eindex=active_extruder) {
  _manual_move_func_ptr = func;

  START_MENU();
  if (LCD_HEIGHT >= 4) {
    switch (axis) {
      case X_AXIS: STATIC_ITEM(MSG_MOVE_X, SS_DEFAULT|SS_INVERT); break;
      case Y_AXIS: STATIC_ITEM(MSG_MOVE_Y, SS_DEFAULT|SS_INVERT); break;
      case Z_AXIS: STATIC_ITEM(MSG_MOVE_Z, SS_DEFAULT|SS_INVERT); break;
      default:
        TERN_(MANUAL_E_MOVES_RELATIVE, manual_move_e_origin = current_position.e);
        STATIC_ITEM(MSG_MOVE_E, SS_DEFAULT|SS_INVERT);
        break;
    }
  }

  BACK_ITEM(MSG_MOVE_AXIS);
  if (parser.using_inch_units()) {
    if (LARGE_AREA_TEST) SUBMENU(MSG_MOVE_1IN, []{ _goto_manual_move(IN_TO_MM(1.000f)); });
    SUBMENU(MSG_MOVE_01IN,   []{ _goto_manual_move(IN_TO_MM(0.100f)); });
    SUBMENU(MSG_MOVE_001IN,  []{ _goto_manual_move(IN_TO_MM(0.010f)); });
    SUBMENU(MSG_MOVE_0001IN, []{ _goto_manual_move(IN_TO_MM(0.001f)); });
  }
  else {
    if (LARGE_AREA_TEST) SUBMENU(MSG_MOVE_100MM, []{ _goto_manual_move(100); });
    SUBMENU(MSG_MOVE_10MM, []{ _goto_manual_move(10);    });
    SUBMENU(MSG_MOVE_1MM,  []{ _goto_manual_move( 1);    });
    SUBMENU(MSG_MOVE_01MM, []{ _goto_manual_move( 0.1f); });
    if (axis == Z_AXIS && (FINE_MANUAL_MOVE) > 0.0f && (FINE_MANUAL_MOVE) < 0.1f) {
      // Determine digits needed right of decimal
      constexpr uint8_t digs = !UNEAR_ZERO((FINE_MANUAL_MOVE) * 1000 - int((FINE_MANUAL_MOVE) * 1000)) ? 4 :
                               !UNEAR_ZERO((FINE_MANUAL_MOVE) *  100 - int((FINE_MANUAL_MOVE) *  100)) ? 3 : 2;
      PGM_P const label = GET_TEXT(MSG_MOVE_N_MM);
      char tmp[strlen_P(label) + 10 + 1], numstr[10];
      sprintf_P(tmp, label, dtostrf(FINE_MANUAL_MOVE, 1, digs, numstr));
      #if DISABLED(HAS_GRAPHICAL_TFT)
        SUBMENU_P(NUL_STR, []{ _goto_manual_move(float(FINE_MANUAL_MOVE)); });
        MENU_ITEM_ADDON_START(0 + ENABLED(HAS_MARLINUI_HD44780));
        lcd_put_u8str(tmp);
        MENU_ITEM_ADDON_END();
      #else
        SUBMENU_P(tmp, []{ _goto_manual_move(float(FINE_MANUAL_MOVE)); });
      #endif
    }
  }
  END_MENU();
}

#if E_MANUAL

  static int16_t kne_filament_temperature = PREHEAT_1_TEMP_HOTEND;
  static uint8_t kne_filament_material_index = 0;
  extern uint8_t filament_cmd;

  static bool kne_filament_blocked() {
    if (!(printer_busy() || printingIsPaused() || queue.has_commands_queued())) return false;
    ui.set_status_P(GET_TEXT(MSG_BNC_OCUPADO));
    ui.return_to_status();
    return true;
  }

  void _menu_move_distance_e_maybe() {
    if (kne_filament_blocked()) return;
    #if ENABLED(PREVENT_COLD_EXTRUSION)
      if (kne_filament_temperature < thermalManager.extrude_min_temp) {
        ui.set_status_P(GET_TEXT(MSG_HOTEND_TOO_COLD));
        return;
      }
    #endif
    thermalManager.setTargetHotend(_MIN(kne_filament_temperature, thermalManager.hotend_max_target(0)), 0);
    ui.goto_screen(preheat_to_move_E);
  }

  // Called only when the user finishes manual loading after the hot extrusion step.
  bool kne_filament_prepare_print() {
    if (printer_busy() || printingIsPaused()
        || thermalManager.degTargetHotend(0) < EXTRUDE_MINTEMP
        || thermalManager.tooColdToExtrude(0)) return false;
    thermalManager.setTargetHotend(_MIN(kne_filament_temperature, thermalManager.hotend_max_target(0)), 0);
    #if PREHEAT_COUNT
      const uint8_t material = _MIN(kne_filament_material_index, PREHEAT_COUNT - 1);
      #if HAS_HEATED_BED
        thermalManager.setTargetBed(_MIN(ui.material_preset[material].bed_temp, BED_MAX_TARGET));
      #endif
      #if HAS_FAN
        thermalManager.set_fan_speed(0, ui.material_preset[material].fan_speed);
      #endif
    #endif
    ui.set_status_P(GET_TEXT(MSG_KNE_LOAD_COMPLETE));
    ui.return_to_status();
    return true;
  }

  static void menu_kne_filament_temperature() {
    if (kne_filament_blocked()) return;
    START_MENU();
    BACK_ITEM(MSG_BACK);
    #if ENABLED(PREVENT_COLD_EXTRUSION)
      const int16_t minimum = thermalManager.extrude_min_temp;
    #else
      const int16_t minimum = EXTRUDE_MINTEMP;
    #endif
    if (filament_cmd == FILA_IN) STATIC_ITEM(MSG_KNE_AFTER_LOAD_HEAT);
    EDIT_ITEM(int3, MSG_KNE_FILAMENT_TEMP, &kne_filament_temperature, minimum, thermalManager.hotend_max_target(0));
    ACTION_ITEM(MSG_BUTTON_PROCEED, _menu_move_distance_e_maybe);
    END_MENU();
  }

  void menu_kne_filament_material() {
    if (kne_filament_blocked()) return;
    START_MENU();
    BACK_ITEM(MSG_BACK);
    #if PREHEAT_COUNT
      LOOP_L_N(m, PREHEAT_COUNT)
        SUBMENU_N_S(m, ui.get_preheat_label(m), MSG_KNE_MATERIAL_CHOICE, []{
          kne_filament_material_index = MenuItemBase::itemIndex;
          kne_filament_temperature = ui.material_preset[kne_filament_material_index].hotend_temp;
          ui.goto_screen(menu_kne_filament_temperature);
        });
    #else
      SUBMENU(MSG_KNE_FILAMENT_TEMP, menu_kne_filament_temperature);
    #endif
    END_MENU();
  }

#endif // E_MANUAL
extern uint32_t ac_touch_ms;
extern bool ac_touch_flag;
void menu_move() {
	 START_MENU();
   BACK_ITEM(MSG_BACK);
   
//#if ENABLED(BLTOUCH)
//	if(ELAPSED(millis(), ac_touch_ms + 1000))
//	{
//		if(ac_touch_flag ){
//			ac_touch_flag =false;
//			bltouch._reset();
//			bltouch._stow();	
//		}

//	}
//    
//#endif

  #if BOTH(HAS_SOFTWARE_ENDSTOPS, SOFT_ENDSTOPS_MENU_ITEM)
    EDIT_ITEM(bool, MSG_LCD_SOFT_ENDSTOPS, &soft_endstop._enabled);
  #endif

  if (NONE(IS_KINEMATIC, NO_MOTION_BEFORE_HOMING) || all_axes_homed()) {
    if (TERN1(DELTA, current_position.z <= delta_clip_start_height)) {
      SUBMENU(MSG_MOVE_X,  []{  LCD_goto_manual_move(0.1,X_AXIS);    });
     // SUBMENU(MSG_MOVE_X, []{ _menu_move_distance(X_AXIS, lcd_move_x); });
      #if HAS_Y_AXIS
    //    SUBMENU(MSG_MOVE_Y, []{ _menu_move_distance(Y_AXIS, lcd_move_y); });
         SUBMENU(MSG_MOVE_Y,  []{  LCD_goto_manual_move(0.1,Y_AXIS);    });
      #endif
    }
    #if ENABLED(DELTA)
      else
        ACTION_ITEM(MSG_FREE_XY, []{ line_to_z(delta_clip_start_height); ui.synchronize(); });
    #endif

    #if HAS_Z_AXIS
     // SUBMENU(MSG_MOVE_Z, []{ _menu_move_distance(Z_AXIS, lcd_move_z); });
           SUBMENU(MSG_MOVE_Z,  []{  LCD_goto_manual_move(0.1,Z_AXIS);    });
    #endif
    #if LINEAR_AXES >= 4
      SUBMENU(MSG_MOVE_I, []{ _menu_move_distance(I_AXIS, lcd_move_i); });
    #endif
    #if LINEAR_AXES >= 5
      SUBMENU(MSG_MOVE_J, []{ _menu_move_distance(J_AXIS, lcd_move_j); });
    #endif
    #if LINEAR_AXES >= 6
      SUBMENU(MSG_MOVE_K, []{ _menu_move_distance(K_AXIS, lcd_move_k); });
    #endif
  }
  else
    GCODES_ITEM(MSG_AUTO_HOME, G28_STR);

  #if ANY(SWITCHING_EXTRUDER, SWITCHING_NOZZLE, MAGNETIC_SWITCHING_TOOLHEAD)

    #if EXTRUDERS >= 4
      switch (active_extruder) {
        case 0: GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1")); break;
        case 1: GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0")); break;
        case 2: GCODES_ITEM_N(3, MSG_SELECT_E, PSTR("T3")); break;
        case 3: GCODES_ITEM_N(2, MSG_SELECT_E, PSTR("T2")); break;
        #if EXTRUDERS == 6
          case 4: GCODES_ITEM_N(5, MSG_SELECT_E, PSTR("T5")); break;
          case 5: GCODES_ITEM_N(4, MSG_SELECT_E, PSTR("T4")); break;
        #endif
      }
    #elif EXTRUDERS == 3
      if (active_extruder < 2) {
        if (active_extruder)
          GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0"));
        else
          GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1"));
      }
    #else
      if (active_extruder)
        GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0"));
      else
        GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1"));
    #endif

  #elif ENABLED(DUAL_X_CARRIAGE)

    if (active_extruder)
      GCODES_ITEM_N(0, MSG_SELECT_E, PSTR("T0"));
    else
      GCODES_ITEM_N(1, MSG_SELECT_E, PSTR("T1"));

  #endif

  #if E_MANUAL

    // The current extruder
    //SUBMENU(MSG_MOVE_E, []{ _menu_move_distance_e_maybe(); });

    #define SUBMENU_MOVE_E(N) SUBMENU_N(N, MSG_MOVE_EN, []{ _menu_move_distance(E_AXIS, []{ lcd_move_e(MenuItemBase::itemIndex); }, MenuItemBase::itemIndex); });

    #if EITHER(SWITCHING_EXTRUDER, SWITCHING_NOZZLE)

      // ...and the non-switching
      #if E_MANUAL == 7 || E_MANUAL == 5 || E_MANUAL == 3
        SUBMENU_MOVE_E(E_MANUAL - 1);
      #endif

    #elif MULTI_E_MANUAL

      // Independent extruders with one E-stepper per hotend
      LOOP_L_N(n, E_MANUAL) SUBMENU_MOVE_E(n);

    #endif

  #endif // E_MANUAL
  SUBMENU(Home_motor, menu_xyz_home);
  GCODES_ITEM(MSG_DISABLE_STEPPERS, PSTR("M84"));
  END_MENU();
}

#if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
  //
  // "Motion" > "Homing" submenu
  //
  void menu_home() {
    START_MENU();
    BACK_ITEM(MSG_MOTION);

    GCODES_ITEM(MSG_AUTO_HOME, G28_STR);
    GCODES_ITEM(MSG_AUTO_HOME_X, PSTR("G28X"));
    #if HAS_Y_AXIS
      GCODES_ITEM(MSG_AUTO_HOME_Y, PSTR("G28Y"));
    #endif
    #if HAS_Z_AXIS
      GCODES_ITEM(MSG_AUTO_HOME_Z, PSTR("G28Z"));
    #endif
    #if LINEAR_AXES >= 4
      GCODES_ITEM(MSG_AUTO_HOME_I, PSTR("G28" AXIS4_STR));
    #endif
    #if LINEAR_AXES >= 5
      GCODES_ITEM(MSG_AUTO_HOME_J, PSTR("G28" AXIS5_STR));
    #endif
    #if LINEAR_AXES >= 6
      GCODES_ITEM(MSG_AUTO_HOME_K, PSTR("G28" AXIS6_STR));
    #endif

    END_MENU();
  }
#endif

#if ENABLED(AUTO_BED_LEVELING_UBL)
  void _lcd_ubl_level_bed();
//#elif ENABLED(LCD_BED_LEVELING)
//  void menu_bed_leveling();
#endif

#if ENABLED(ASSISTED_TRAMMING_WIZARD)
  void goto_tramming_wizard();
#endif
void menu_xyz_home()
{
    START_MENU();
    BACK_ITEM(MSG_BACK);
    #if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
    SUBMENU(MSG_HOMING, menu_home);
  #else
    #if ENABLED(INDIVIDUAL_AXIS_HOMING_MENU)  
      GCODES_ITEM(MSG_AUTO_HOME_X, PSTR("G28X"));
      #if HAS_Y_AXIS
        GCODES_ITEM(MSG_AUTO_HOME_Y, PSTR("G28Y"));
      #endif
      #if HAS_Z_AXIS
        GCODES_ITEM(MSG_AUTO_HOME_Z, PSTR("G28ZR10"));
      #endif
      #if LINEAR_AXES >= 4
        GCODES_ITEM(MSG_AUTO_HOME_I, PSTR("G28" AXIS4_STR));
      #endif
      #if LINEAR_AXES >= 5
        GCODES_ITEM(MSG_AUTO_HOME_J, PSTR("G28" AXIS5_STR));
      #endif
      #if LINEAR_AXES >= 6
        GCODES_ITEM(MSG_AUTO_HOME_K, PSTR("G28" AXIS6_STR));
      #endif
    #endif
  #endif
    END_MENU();
}
void menu_motion() {
  START_MENU();

  //
  // ^ Main
  //
  BACK_ITEM(MSG_BACK);

  //
  // Move Axis
  //
 // if (TERN1(DELTA, all_axes_homed()))
  SUBMENU(MSG_MOVE_AXIS, []{ ui.goto_screen(menu_move);});
  SUBMENU(Home_motor, menu_xyz_home);
  //
  // Auto Home
  //
//  #if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
//    SUBMENU(MSG_HOMING, menu_home);
//  #else
//    GCODES_ITEM(MSG_AUTO_HOME, G28_STR);
//    #if ENABLED(INDIVIDUAL_AXIS_HOMING_MENU)  
//      GCODES_ITEM(MSG_AUTO_HOME_X, PSTR("G28X"));
//      #if HAS_Y_AXIS
//        GCODES_ITEM(MSG_AUTO_HOME_Y, PSTR("G28Y"));
//      #endif
//      #if HAS_Z_AXIS
//        GCODES_ITEM(MSG_AUTO_HOME_Z, PSTR("G28Z"));
//      #endif
//      #if LINEAR_AXES >= 4
//        GCODES_ITEM(MSG_AUTO_HOME_I, PSTR("G28" AXIS4_STR));
//      #endif
//      #if LINEAR_AXES >= 5
//        GCODES_ITEM(MSG_AUTO_HOME_J, PSTR("G28" AXIS5_STR));
//      #endif
//      #if LINEAR_AXES >= 6
//        GCODES_ITEM(MSG_AUTO_HOME_K, PSTR("G28" AXIS6_STR));
//      #endif
//    #endif
//  #endif

  //
  // Auto-calibration
  //
  #if ENABLED(CALIBRATION_GCODE)
    GCODES_ITEM(MSG_AUTO_CALIBRATE, PSTR("G425"));
  #endif

  //
  // Auto Z-Align
  //
  #if EITHER(Z_STEPPER_AUTO_ALIGN, MECHANICAL_GANTRY_CALIBRATION)
    GCODES_ITEM(MSG_AUTO_Z_ALIGN, PSTR("G34"));
  #endif

  //
  // Assisted Bed Tramming
  //
  #if ENABLED(ASSISTED_TRAMMING_WIZARD)
    SUBMENU(MSG_TRAMMING_WIZARD, goto_tramming_wizard);
  #endif

  //
  // Level Bed
  //
//  #if ENABLED(AUTO_BED_LEVELING_UBL)

//    SUBMENU(MSG_UBL_LEVEL_BED, _lcd_ubl_level_bed);

//  #elif ENABLED(LCD_BED_LEVELING)

//    if (!g29_in_progress)
//      SUBMENU(MSG_BED_LEVELING, menu_bed_leveling);

//  #elif HAS_LEVELING && DISABLED(SLIM_LCD_MENUS)

//    #if DISABLED(PROBE_MANUALLY)
//      GCODES_ITEM(MSG_LEVEL_BED, PSTR("G29N"));
//    #endif

//    if (all_axes_homed() && leveling_is_valid()) {
//      bool show_state = planner.leveling_active;
//      EDIT_ITEM(bool, MSG_BED_LEVELING, &show_state, _lcd_toggle_bed_leveling);
//    }

//    #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
//      editable.decimal = planner.z_fade_height;
//      EDIT_ITEM_FAST(float3, MSG_Z_FADE_HEIGHT, &editable.decimal, 0, 100, []{ set_z_fade_height(editable.decimal); });
//    #endif

//  #endif

//  #if ENABLED(LEVEL_BED_CORNERS) && DISABLED(LCD_BED_LEVELING)
//    SUBMENU(MSG_BED_TRAMMING, _lcd_level_bed_corners);
//  #endif

//  #if ENABLED(Z_MIN_PROBE_REPEATABILITY_TEST)
//    GCODES_ITEM(MSG_M48_TEST, PSTR("G28O\nM48 P10"));
//  #endif

  //
  // Disable Steppers
  //
 
  #if HAS_TEMPERATURE
    SUBMENU(MSG_TEMPERATURE, menu_temperature);   //�¶ȷŵ�����
  #endif
  
 

  END_MENU();
}

//********************����ƶ�ר��**********************
void LCD_goto_manual_move(const_float_t scale,const AxisEnum axis) {
  ui.defer_status_screen();
  ui.manual_move.menu_scale = 0.1;
  ui.enable_encoder_multiplier(true);
  if(axis==X_AXIS)
  {
    _manual_move_func_ptr = lcd_move_x;
    ui.goto_screen(_manual_move_func_ptr);
  }
  else if(axis==Y_AXIS)
  {
  _manual_move_func_ptr = lcd_move_y;
    ui.goto_screen(_manual_move_func_ptr);
  }
  else if(axis==Z_AXIS)
  {
   _manual_move_func_ptr = lcd_move_z;
    ui.goto_screen(_manual_move_func_ptr);
  }
  else if(axis==E_AXIS)
  {
		_manual_move_func_ptr = lcd_move_e;
       ui.goto_screen([]{  lcd_move_e(TERN_(MULTI_E_MANUAL, const int8_t eindex=active_extruder)); });
	   
   }
}

#if E_MANUAL
void preheat_to_move_E() {
  ui.defer_status_screen();
  if (ui.use_click()) {
    thermalManager.setTargetHotend(0, 0);
    filament_cmd = FILA_NO_ACT;
    ui.goto_previous_screen_no_defer();
    return;
  }
  if (kne_filament_blocked()) {
    thermalManager.setTargetHotend(0, 0);
    filament_cmd = FILA_NO_ACT;
    return;
  }
  const int16_t currentTemperature = thermalManager.degHotend(0),
                targetTemperature = thermalManager.degTargetHotend(0);
  if (targetTemperature <= 0) {
    filament_cmd = FILA_NO_ACT;
    ui.goto_previous_screen_no_defer();
    return;
  }
  if (currentTemperature >= targetTemperature - 2
    #if ENABLED(PREVENT_COLD_EXTRUSION)
      && !thermalManager.tooColdToExtrude(0)
    #endif
  ) {
    ui.goto_screen(draw_unload_load_filament);
    return;
  }
  if (ui.should_draw()) {
    tft.canvas(30, 80, TFT_WIDTH - 60, 90);
    tft.set_background(COLOR_BACKGROUND);
    tft_string.set(GET_TEXT(MSG_KNE_HEATING));
    tft_string.trim();
    tft.add_text(0, 5, COLOR_WHITE, tft_string);
    tft.add_text(0, 40, COLOR_WHITE, i16tostr3rj(currentTemperature));
    tft.add_text(40, 40, COLOR_WHITE, "/");
    tft.add_text(60, 40, COLOR_WHITE, i16tostr3rj(targetTemperature));
    tft_string.set(GET_TEXT(MSG_KNE_CANCEL));
    tft_string.trim();
    tft.add_text(0, 65, COLOR_WHITE, tft_string);
  }
  ui.refresh(LCDVIEW_CALL_REDRAW_NEXT);
}
#endif // E_MANUAL
#endif // HAS_LCD_MENU
