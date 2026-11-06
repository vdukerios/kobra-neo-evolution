#pragma once
/**
 * Textos de la funcion "guia_ayuda" (guia para principiantes y ayuda por opcion).
 * Dos idiomas: ingles (indice 0) y espanol (indice 1). guia_idioma() devuelve el indice
 * del idioma activo de la interfaz; cualquier otro idioma usa ingles.
 * Solo ASCII (sin tildes ni enies) para no depender de la fuente de la pantalla.
 * Maximo 33 caracteres por linea y 5 lineas por pagina. Incluir SOLO desde menu_guia.cpp.
 * Todo son cadenas literales constantes: viven en flash, no en RAM.
 */

#if ENABLED(GUIA_AYUDA)

#define GUIA_EN 0
#define GUIA_ES 1

//
// Idioma activo -> GUIA_EN o GUIA_ES. Se calcula en compilacion a que idioma
// corresponde cada hueco de LCD_LANGUAGE..LCD_LANGUAGE_5 (da igual el orden en
// Configuration.h) y en ejecucion se consulta ui.language. Si no es en ni es -> ingles.
// Los nombres 'en' y 'es' se definen solo aqui (language_xx.h hace lo mismo) y se
// retiran antes de usar GET_TEXT, que pega el nombre del idioma.
//
#define en 1
#define es 2
#if LCD_LANGUAGE == es
  #define GUIA_ID0 GUIA_ES
#else
  #define GUIA_ID0 GUIA_EN
#endif
#if LCD_LANGUAGE_2 == es
  #define GUIA_ID1 GUIA_ES
#else
  #define GUIA_ID1 GUIA_EN
#endif
#if LCD_LANGUAGE_3 == es
  #define GUIA_ID2 GUIA_ES
#else
  #define GUIA_ID2 GUIA_EN
#endif
#if LCD_LANGUAGE_4 == es
  #define GUIA_ID3 GUIA_ES
#else
  #define GUIA_ID3 GUIA_EN
#endif
#if LCD_LANGUAGE_5 == es
  #define GUIA_ID4 GUIA_ES
#else
  #define GUIA_ID4 GUIA_EN
#endif
#undef en
#undef es

static uint8_t guia_idioma() {
  #if ENABLED(HAS_MULTI_LANGUAGE)
    static const uint8_t mapa[] PROGMEM = { GUIA_ID0, GUIA_ID1, GUIA_ID2, GUIA_ID3, GUIA_ID4 };
    return (ui.language < sizeof(mapa)) ? mapa[ui.language] : uint8_t(GUIA_EN);
  #else
    return GUIA_ID0;
  #endif
}

struct GuiaTema {
  const char *titulo[2];
  const char * const *paginas[2];
  uint8_t n;
};

// ---- 1. Primera impresion / First print ----
static const char * const guia_t0_en[] = {
  "Insert the SD card with your\n.gcode file into the slot,\ncontacts facing down.",
  "Load the filament:\nPrepare > Load Filament.\nWait until plastic comes out\nof the nozzle.",
  "Level the bed and set the\nnozzle height with a sheet of\npaper. See the topic: Level\nand height (paper).",
  "Choose SD Card Print, pick the\nfile and confirm. The printer\nheats up and measures by itself.",
  "Watch the first layer: it must\nstick well and look smooth. If\nnot, stop the print and adjust\nthe Z height.",
  "When done, wait for the bed to\ncool below 50 degrees, remove\nthe part and switch the printer\noff."
};
static const char * const guia_t0_es[] = {
  "Pon la tarjeta SD con tu archivo\n.gcode en la ranura, con los\ncontactos hacia abajo.",
  "Carga el filamento:\nPreparar > Cargar filamento.\nEspera a que salga plastico\npor la boquilla.",
  "Nivela la cama y ajusta la\naltura con una hoja de papel.\nMira el tema: Nivelar y altura\ncon papel.",
  "Elige Imprimir desde SD, escoge\nel archivo y confirma. La\nimpresora calienta y mide sola.",
  "Vigila la primera capa: debe\nquedar pegada y lisa. Si no,\ndeten la impresion y ajusta\nla altura Z.",
  "Al terminar, espera a que enfrie\nbajo 50 grados, despega la pieza\ny apaga la impresora."
};

// ---- 2. Cargar filamento / Load filament ----
static const char * const guia_t1_en[] = {
  "Cut the filament tip at an\nangle with pliers.\nIt feeds in more easily.",
  "Go to Prepare > Load Filament.\nThe nozzle heats up by itself.\nWait until it finishes.",
  "When it is hot, push the\nfilament in by hand until the\nmotor grabs it.",
  "Wait until clean plastic of the\nright color comes out. Remove\nthe excess strand."
};
static const char * const guia_t1_es[] = {
  "Corta la punta del filamento en\ndiagonal con una pinza.\nAsi entra mas facil.",
  "Ve a Preparar > Cargar filamento.\nLa boquilla calienta sola.\nEspera a que termine.",
  "Cuando este caliente, empuja el\nfilamento a mano hasta que el\nmotor lo jale.",
  "Espera a que salga plastico\nlimpio y del color correcto.\nRetira el hilo sobrante."
};

// ---- 3. Nivelar y ajustar altura con papel / Level and height ----
static const char * const guia_t2_en[] = {
  "The nozzle must sit at the\nright distance from the bed.\nToo far: it will not stick.\nToo close: it scrapes.",
  "Go to Level and center. The\nprinter probes the bed and\nmoves to the center.",
  "Place a normal sheet of paper\nbetween the nozzle and the bed.",
  "Turn the wheel to raise or\nlower in 0.01 mm steps until\nyou feel a slight drag when\nmoving the paper.",
  "Click to save. Print a test\nfirst layer to confirm it\nlooks right."
};
static const char * const guia_t2_es[] = {
  "La boquilla debe quedar a la\ndistancia justa de la cama.\nMuy lejos: no pega.\nMuy cerca: raspa.",
  "Ve a Nivelar y centrar. La\nimpresora mide la cama y se\ncoloca al centro.",
  "Pon una hoja de papel comun\nentre la boquilla y la cama.",
  "Gira la rueda para subir o bajar\nde a 0,01 mm hasta sentir un\nroce leve al mover el papel.",
  "Clic para guardar. Haz una\nprimera capa de prueba para\nconfirmar que queda bien."
};

// ---- 4. Si se corta la luz / If power is cut ----
static const char * const guia_t3_en[] = {
  "If the power fails during a\nprint, the printer saves where\nit was, if Power Outage is on\nin Configuration.",
  "When power returns, an outage\nnotice appears. Do not move\nthe part or the bed.",
  "Choose to continue: it reheats\nand resumes from the last saved\nlayer.",
  "If the part came loose or cooled\ndown too much, discard it and\nstart over."
};
static const char * const guia_t3_es[] = {
  "Si se va la luz imprimiendo, la\nimpresora guarda donde iba, si\nla recuperacion tras corte esta\nactivada en Configuracion.",
  "Al volver la luz aparece un\naviso de corte. No muevas la\npieza ni la cama.",
  "Elige continuar: recalienta y\nretoma desde la ultima capa\nguardada.",
  "Si la pieza se despego o se\nenfrio mucho, descarta y\nempieza de nuevo."
};

// ---- 5. Si se despega la pieza / If the part lifts off ----
static const char * const guia_t4_en[] = {
  "Clean the bed with isopropyl\nalcohol. Fingers leave grease\nthat stops it from sticking.",
  "Check the Z height with paper:\nthe first layer should be\nslightly squished.",
  "Raise the bed temperature by 5\ndegrees, or apply glue stick\nto the bed.",
  "Avoid drafts and keep the fan\noff during the first layers."
};
static const char * const guia_t4_es[] = {
  "Limpia la cama con alcohol\nisopropilico. Los dedos dejan\ngrasa que impide pegar.",
  "Revisa la altura Z con el papel:\nla primera capa debe quedar\nligeramente aplastada.",
  "Sube la temperatura de la cama\n5 grados, o pasa pegamento en\nbarra sobre la cama.",
  "Evita corrientes de aire y no\npongas el ventilador en las\nprimeras capas."
};

// ---- 6. Mantenimiento basico / Basic maintenance ----
static const char * const guia_t5_en[] = {
  "Clean the bed and the nozzle\n(when cold) every few prints.",
  "If there are strings or blobs,\nunload and load filament while\nhot to clean the nozzle.",
  "Put a drop of machine oil on\nthe smooth rods every 100\nprinting hours.",
  "Check that the belts are tight\nand the screws are firm. Reset\nthe reminders in Maintenance.",
  "Store filament dry and sealed.\nMoisture ruins it."
};
static const char * const guia_t5_es[] = {
  "Limpia la cama y la boquilla\n(en frio) cada pocas piezas.",
  "Si hay hilos o grumos, descarga\ny carga filamento en caliente\npara limpiar la boquilla.",
  "Una gota de aceite de maquina\nen las varillas lisas cada 100\nhoras de impresion.",
  "Revisa que las correas esten\ntensas y los tornillos firmes.\nReinicia los avisos en Mantenim.",
  "Guarda el filamento seco y\ncerrado. La humedad lo estropea."
};

// ---- 7. Que significa cada menu / What each menu does ----
static const char * const guia_t6_en[] = {
  "SD Card Print: lists the files\non the card.\nPrepare: heat, load and unload\nfilament.",
  "Move Axis: moves the head or\nthe bed by hand with the wheel.\nLevel and center: probes the\nbed and sets the height.",
  "Configuration: sound, language,\npower outage recovery, filament\nsensor, About and advanced\nsettings.",
  "Maintenance: lubricate axes and\ncheck belts reminders.",
  "Tip: hold the click for 0.8 s\non an option to see what it\ndoes.",
  "Turning the wheel moves up or\ndown the list. A click enters\nor confirms. The first option\nof every menu always goes back."
};
static const char * const guia_t6_es[] = {
  "Imprim. desde SD: lista de los\narchivos de la tarjeta.\nPreparar: calentar, cargar y\ndescargar filamento.",
  "Mover ejes: mueve la cabeza o la\ncama a mano con la rueda.\nNivelar y centrar: mide la cama\ny ajusta la altura.",
  "Configuracion: sonido, idioma,\nrecuperacion tras corte, sensor\nde filamento, Acerca de y\najustes avanzados.",
  "Mantenimiento: avisos de\nlubricar ejes y revisar correas.",
  "Truco: manten pulsado el clic\n0,8 s sobre una opcion para ver\nque hace.",
  "Girar la rueda sube o baja por\nla lista. Un clic entra o\nconfirma. La primera opcion de\ncada menu siempre vuelve atras."
};

#define GUIA_NUM_TEMAS 7
#define GUIA_NP(P) uint8_t(sizeof(P) / sizeof(P[0]))
// Ambos idiomas deben tener el mismo numero de paginas
#define GUIA_TEMA(N, T_EN, T_ES) \
  { { T_EN, T_ES }, { guia_t##N##_en, guia_t##N##_es }, GUIA_NP(guia_t##N##_en) }
#define GUIA_IGUALES(N) static_assert(GUIA_NP(guia_t##N##_en) == GUIA_NP(guia_t##N##_es), "guia_t" #N ": paginas distintas")

GUIA_IGUALES(0); GUIA_IGUALES(1); GUIA_IGUALES(2); GUIA_IGUALES(3);
GUIA_IGUALES(4); GUIA_IGUALES(5); GUIA_IGUALES(6);

static const GuiaTema guia_temas[GUIA_NUM_TEMAS] = {
  GUIA_TEMA(0, "First print",            "Primera impresion"),
  GUIA_TEMA(1, "Load filament",          "Cargar filamento"),
  GUIA_TEMA(2, "Level & height (paper)", "Nivelar y altura (papel)"),
  GUIA_TEMA(3, "If power is cut",        "Si se corta la luz"),
  GUIA_TEMA(4, "If the part lifts off",  "Si se despega la pieza"),
  GUIA_TEMA(5, "Basic maintenance",      "Mantenimiento basico"),
  GUIA_TEMA(6, "What each menu does",    "Que significa cada menu")
};

// Pie de las pantallas (los dos primeros %u son pagina actual y total)
static const char * const guia_pie_pagina[2] = { "< %u/%u > Click or turn past end: exit", "< %u/%u > Clic o girar al final: salir" };
static const char * const guia_pie_ayuda[2]  = { "Click: back", "Clic: volver" };

#if ENABLED(GUIA_AYUDA_LARGA)

/**
 * Ayuda por opcion. Se busca por el CONTENIDO del texto de la opcion en el idioma
 * activo (GET_TEXT), asi funciona aunque se cambie de idioma. Si la opcion no tiene
 * ayuda se devuelve nullptr. Maximo 5 lineas de 33 caracteres (se prefieren 2-4).
 * La lista X(MENSAJE, texto en ingles, texto en espanol) genera a la vez la tabla de
 * textos (flash) y la busqueda, asi que ambas no pueden desordenarse.
 * La primera entrada solo vale en el menu principal.
 */
void menu_main();

#define GUIA_AYUDAS(X) \
  X(MSG_MEDIA_MENU,        "Lists the files on the SD card.\nPick one to print it. Filament\nmust be loaded beforehand.", \
                           "Muestra los archivos de la\ntarjeta SD. Elige uno para\nimprimirlo. Antes debes tener\nfilamento cargado.") \
  X(MSG_MOVE_AXIS,         "Moves the head or the bed by\nhand. Pick an axis, turn the\nwheel to move, click to finish.", \
                           "Mueve la cabeza o la cama a\nmano. Elige un eje, gira la\nrueda para avanzar o retroceder\ny clic para terminar.") \
  X(MSG_BED_LEVELING,      "Advanced bed tools: edit the mesh,\nadjust the Z offset and level the\nbed corners (bed tramming).", \
                           "Herramientas de cama: editar la\nmalla, ajustar el desfase Z y\nnivelar las esquinas (recorrido).") \
  X(MSG_KNE_MAINTENANCE,   "Reminders to lubricate the axes\nand check the belts. Reset a\nreminder after doing the task.", \
                           "Avisos para lubricar los ejes\ny revisar las correas. Reinicia\nun aviso al hacer la tarea.") \
  X(MSG_PREPARE,           "Heat, load and unload filament.\nUsed before printing.", \
                           "Calienta, carga y descarga\nfilamento. Se usa antes de\nimprimir.") \
  X(MSG_CONFIGURATION,     "Sound, power outage recovery,\nfilament sensor and more\nprinter settings.", \
                           "Sonido, recuperacion tras corte\nde luz, sensor de filamento y\nmas ajustes de la impresora.") \
  X(MSG_ABOUT,             "Shows the project name, the\nsoftware version, the build\nvolume and the project link.", \
                           "Muestra el nombre del proyecto,\nla version del programa, el\nvolumen de impresion y el enlace\ndel proyecto.") \
  X(MSG_INFO_SCREEN,       "Returns to the main screen with\ntemperatures and print progress.", \
                           "Vuelve a la pantalla principal\ncon las temperaturas y el\navance de la impresion.") \
  X(MSG_BACK,              "Returns to the previous screen\nwithout changing anything.\nIt is always the first option\nof every menu.", \
                           "Regresa a la pantalla anterior\nsin cambiar nada. Es siempre\nla primera opcion de cada menu.") \
  X(MSG_PAUSE_PRINT,       "Stops the print without losing\nprogress. Resume from this same\nmenu.", \
                           "Detiene la impresion sin perder\nel avance. Reanuda desde este\nmismo menu.") \
  X(MSG_RESUME_PRINT,      "Continues the print from where\nit was paused.", \
                           "Continua la impresion desde\ndonde se pauso.") \
  X(MSG_STOP_PRINT,        "Cancels the print. Asks for\nconfirmation. The print cannot\nbe resumed afterwards.", \
                           "Cancela la impresion. Pide\nconfirmacion. La impresion no\nse puede reanudar despues.") \
  X(MSG_TUNE,              "Changes speed, flow or fine Z\nheight live, without stopping\nthe print.", \
                           "Cambia en vivo la velocidad, el\nflujo o la altura Z fina sin\ndetener la impresion.") \
  X(MSG_PREHEAT_PLA,       "Heats nozzle and bed to the\ntypical PLA temperature\n(200 and 60 degrees).", \
                           "Calienta boquilla y cama a la\ntemperatura tipica de PLA\n(200 y 60 grados).") \
  X(MSG_PREHEAT_ABS,       "Heats nozzle and bed to the\ntypical ABS temperature\n(240 and 110 degrees).", \
                           "Calienta boquilla y cama a la\ntemperatura tipica de ABS\n(240 y 110 grados).") \
  X(MSG_MOVE_E_IN,         "Heats the nozzle and lets you\npush the filament in by hand\nwhile the motor pulls it until\nplastic comes out.\nClick to stop.", \
                           "Calienta la boquilla y te deja\nempujar el filamento a mano\nmientras el motor lo jala hasta\nque salga plastico.\nClic para parar.") \
  X(MSG_MOVE_E_OUT,        "Heats the nozzle and retracts\nthe filament until you click.\nThen pull it out by hand.", \
                           "Calienta la boquilla y retrae el\nfilamento hasta que hagas clic.\nLuego sacalo con la mano.") \
  X(MSG_MOVE_X,            "Moves the head sideways. Turn\nthe wheel to move; click ends.", \
                           "Mueve la cabeza en horizontal.\nGira la rueda para avanzar o\nretroceder; clic termina.") \
  X(MSG_MOVE_Y,            "Moves the bed forward or back.\nTurn the wheel; click ends.", \
                           "Mueve la cama hacia adelante o\natras. Gira la rueda; clic\ntermina.") \
  X(MSG_MOVE_Z,            "Raises or lowers the nozzle.\nTurn the wheel; click ends.\nCareful not to hit the bed.", \
                           "Sube o baja la boquilla. Gira\nla rueda; clic termina. Cuidado\nde no chocar con la cama.") \
  X(MSG_MOVE_E,            "Moves the extruder motor to\npush or retract filament. The\nnozzle must be hot.", \
                           "Mueve el motor del extrusor para\nempujar o retraer filamento.\nLa boquilla debe estar caliente.") \
  X(MSG_MOVE_01MM,         "Each wheel turn moves 0.1 mm.\nFor very fine adjustments.", \
                           "Cada giro de la rueda mueve\n0,1 mm. Sirve para ajustes\nmuy finos.") \
  X(MSG_MOVE_1MM,          "Each wheel turn moves 1 mm.\nGeneral use.", \
                           "Cada giro de la rueda mueve\n1 mm. Uso general.") \
  X(MSG_MOVE_10MM,         "Each wheel turn moves 10 mm.\nFor long moves.", \
                           "Cada giro de la rueda mueve\n10 mm. Para recorridos largos.") \
  X(MSG_AUTO_HOME,         "Sends all three axes to their\norigin. Needed before moving\naxes by hand or printing.", \
                           "Lleva los tres ejes a su origen.\nNecesario antes de mover ejes\na mano o de imprimir.") \
  X(MSG_DISABLE_STEPPERS,  "Releases the motors so you can\nmove the head by hand. You must\nhome the axes again afterwards.", \
                           "Libera los motores para mover la\ncabeza con la mano. Despues hay\nque ir a origen otra vez.") \
  X(MSG_LEVEL_BED,         "Homes, probes the bed at several\npoints and moves to the center\nso you can set the height.", \
                           "Va a origen, mide la cama en\nvarios puntos y se coloca al\ncentro para ajustar la altura.") \
  X(MSG_ZPROBE_ZOFFSET,    "Raises or lowers the nozzle in\n0.01 mm steps. Put paper between\nthe nozzle and the bed and look\nfor a slight drag.", \
                           "Sube o baja la boquilla de a\n0,01 mm. Pon un papel entre la\nboquilla y la cama y busca un\nroce leve.") \
  X(MSG_BED_TRAMMING,      "Guides you through the bed\ncorners to level its height\nwith the screws.", \
                           "Te guia por las esquinas de la\ncama para igualar su altura\ncon los tornillos.") \
  X(MSG_BABYSTEP_Z,        "Live fine adjustment of the Z\nheight during the first layer.", \
                           "Ajuste fino de la altura Z en\nvivo, durante la primera capa.") \
  X(MSG_STORE_EEPROM,      "Saves all settings to the\nprinter's memory so they are\nkept after power off.", \
                           "Guarda todos los ajustes en la\nmemoria de la impresora para\nque sigan al apagarla.") \
  X(MSG_LOAD_EEPROM,       "Loads the saved settings from\nmemory, discarding unsaved\nchanges.", \
                           "Carga los ajustes guardados en\nla memoria, descartando los\ncambios no guardados.") \
  X(MSG_RESTORE_DEFAULTS,  "Resets settings to factory\nvalues. Does not delete files\non the SD card.", \
                           "Devuelve los ajustes a los\nvalores de fabrica. No borra\nlos archivos de la SD.") \
  X(MSG_OUTAGE_RECOVERY,   "If on, the printer records its\nprogress and, if power fails,\noffers to continue the part.", \
                           "Si esta activado, la impresora\nanota el avance y, si se va la\nluz, ofrece continuar la pieza.") \
  X(MSG_SOUND,             "Turns the printer beeps on or\noff. A click switches it.", \
                           "Activa o desactiva los pitidos\nde la impresora. Un clic\nalterna entre Si y No.") \
  X(MSG_RUNOUT_SENSOR,     "If on, the printer stops when\nthe filament runs out so you\ncan change it.", \
                           "Si esta activado, la impresora\nse detiene al acabarse el\nfilamento para que lo cambies.") \
  X(MSG_ADVANCED_SETTINGS, "Advanced settings: acceleration,\nspeeds and more. For experienced\nusers only.", \
                           "Ajustes avanzados: aceleracion,\nvelocidades y otros. Solo para\nusuarios con experiencia.") \
  X(MSG_TEMPERATURE,       "Adjusts the nozzle and bed\ntemperatures.", \
                           "Ajusta las temperaturas de la\nboquilla y de la cama.") \
  X(MSG_FLOW,              "Percentage of plastic extruded.\n100% is normal. Increase it if\nthe print lacks material.", \
                           "Porcentaje de plastico que se\nextruye. 100% es lo normal.\nSubir si falta material.") \
  X(MSG_REFRESH,           "Reads the SD card again in case\nyou swapped it or the files do\nnot show up.", \
                           "Vuelve a leer la tarjeta SD por\nsi la cambiaste o no aparecen\nlos archivos.") \
  X(MSG_NIVELAR_CENTRAR,   "Homes, measures the bed mesh\nand moves the nozzle to the\ncenter to set the Z height.\nClick to cancel.", \
                           "Va a origen, mide la malla de la\ncama y lleva la boquilla al\ncentro para ajustar la altura Z.\nClic para cancelar.") \
  X(MSG_KNE_MODE_EXPRESS,  "Express uses the saved bed mesh\nand skips measuring. Full\nmeasures the bed hot before\nprinting (slower, more exact).\nNo saved mesh: runs Full.", \
                           "Express usa la malla guardada y\nno mide. Completo mide la cama\nen caliente antes de imprimir\n(mas lento, mas exacto).\nSin malla guardada, hace Completo") \
  X(MSG_KNE_MODE_FULL,     "Express uses the saved bed mesh\nand skips measuring. Full\nmeasures the bed hot before\nprinting (slower, more exact).\nNo saved mesh: runs Full.", \
                           "Express usa la malla guardada y\nno mide. Completo mide la cama\nen caliente antes de imprimir\n(mas lento, mas exacto).\nSin malla guardada, hace Completo")

struct GuiaAyuda { const char *t[2]; };   // [GUIA_EN], [GUIA_ES]

#define GUIA_X_TABLA(M, EN, ES) { { EN, ES } },
static const GuiaAyuda guia_ayuda_tab[] = { GUIA_AYUDAS(GUIA_X_TABLA) };
#undef GUIA_X_TABLA

static const char* guia_buscar_ayuda(const char *p) {
  uint8_t i = 0;
  const uint8_t idioma = guia_idioma();
  #define GUIA_X_BUSCA(M, EN, ES) \
    if ((i || ui.currentScreen == menu_main) && !strcmp_P(p, GET_TEXT(M))) return guia_ayuda_tab[i].t[idioma]; \
    i++;
  GUIA_AYUDAS(GUIA_X_BUSCA)
  #undef GUIA_X_BUSCA
  return nullptr;
}

#endif // GUIA_AYUDA_LARGA
#endif // GUIA_AYUDA
