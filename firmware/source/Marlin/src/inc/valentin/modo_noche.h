#pragma once
// Banderas y ajustes de la funcion "modo_noche". Solo este archivo se edita para esta funcion.
// Cada funcion debe poder apagarse comentando su #define principal.
//
// MODO NOCHE: interruptor "Modo noche" (menu principal y Configuracion). NO es persistente:
// se apaga al reiniciar (no toca la EEPROM). Mientras esta activo:
//  - el zumbador queda en silencio (M300, pitidos de la UI, etc.),
//  - la pantalla TFT se pinta de negro y deja de redibujarse (la rueda sigue atendida),
//  - el primer giro o clic de la rueda solo despierta la pantalla (no ejecuta la accion);
//    tras NOCHE_TIEMPO_ACTIVIDAD_S segundos sin uso vuelve a oscurecerse,
//  - despierta sola (NOCHE_TIEMPO_ALERTA_S segundos) ante: mensajes de alerta de la UI
//    (ui.set_status con nivel > 0 / status_printf), fin de filamento, pausa y fin de impresion
//    (print_job_timer pause/stop). Ante un kill_screen el modo se DESACTIVA por completo.
#define MODO_NOCHE

#ifdef MODO_NOCHE
  #define NOCHE_TIEMPO_ACTIVIDAD_S  30   // Segundos de pantalla encendida tras tocar la rueda
  #define NOCHE_TIEMPO_ALERTA_S    120   // Segundos de pantalla encendida tras una alerta/evento
  #define NOCHE_RETARDO_ENTRADA_S    3   // Segundos entre activar el modo y quedar en negro (0 = al instante)

  // PERFIL SUAVE (opcional, APAGADO por defecto): mientras el modo noche esta activo, baja el
  // porcentaje de velocidad (feedrate_percentage) a NOCHE_PERFIL_PORCENTAJE y al salir del modo
  // restaura el valor previo. Si durante el modo cambias la velocidad a mano, al salir se
  // restaura igualmente el valor guardado al entrar. Descomenta para activar:
  //#define NOCHE_PERFIL_SUAVE
  #define NOCHE_PERFIL_PORCENTAJE   80

  // LUZ DE FONDO (APAGADA por defecto, ver informe): apaga de verdad el pin de luz de fondo
  // (BL_port/BL_pin = PC0 en board/bsp_spi_tft.h) mientras la pantalla esta en negro.
  // Solo implementado para la interfaz 320x240. Sin esta bandera la pantalla queda negra
  // pero con la luz encendida (comportamiento base). Descomenta para activar:
  //#define NOCHE_LUZ_FONDO
#endif
