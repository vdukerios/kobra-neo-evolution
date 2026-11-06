/**
 * feature/registro_aprendizaje.cpp
 * Registro de impresiones en la SD y aprendizaje del tiempo restante (proyecto Valentin).
 *
 * Solo se escribe en la SD al INICIAR y al TERMINAR/DETENER una impresion, nunca durante ella.
 * Al iniciar se deja una marca (REGACTIV.TXT). Al terminar se agrega la linea al CSV y se vacia la marca.
 * Si al arrancar la marca sigue con contenido, la impresion anterior se interrumpio sin poder
 * anotarse (corte de luz o falla grave con kill); se registra entonces como interrumpida.
 * No se escribe en la SD dentro de kill(): puede ocurrir dentro de una interrupcion mientras la
 * SD esta en uso (limitacion conocida: el motivo exacto de un kill no queda registrado).
 */

#include "../inc/MarlinConfig.h"

#ifdef REGISTRO_APRENDIZAJE_ACTIVO

#include "registro_aprendizaje.h"
#include "../sd/cardreader.h"
#include "../module/printcounter.h"
#include "../lcd/marlinui.h"

enum : uint8_t { REG_CANCELADA = 0, REG_FINALIZADA, REG_FILAMENTO, REG_CORTE, REG_INTERRUMPIDA };

static bool activo = false,         // hay una impresion SD registrada en curso
            finalizada = false,     // M1001 confirmo el final del archivo
            filamento_visto = false;
static uint32_t estimado_s = 0;     // primer M73 R (en segundos)
static bool prep_hecha = false;     // M1100 termino bien: el calentamiento ya esta descontado
static uint32_t prep_s = 0;         // segundos de preparacion (M1100: calentar, esperar, malla) antes de imprimir
static char nombre[28];             // nombre del archivo, sin comas

// Abre un archivo de la raiz de la SD (nombre 8.3). false si no hay SD o falla.
static bool abrir(SdFile &f, const char * const nom, const uint8_t flags) {
  if (!card.isMounted()) return false;
  SdFile raiz = card.getroot();
  return f.open(&raiz, nom, flags);
}

static void motivo_a_texto(char * const d, const uint8_t m) {
  switch (m) {
    case REG_FINALIZADA:   strcpy_P(d, PSTR("done")); break;
    case REG_FILAMENTO:    strcpy_P(d, PSTR("filament")); break;
    case REG_CORTE:        strcpy_P(d, PSTR("power")); break;
    case REG_INTERRUMPIDA: strcpy_P(d, PSTR("interrupted")); break;
    default:               strcpy_P(d, PSTR("cancelled")); break;
  }
}

//
// ---------------- Aprendizaje ----------------
//
#ifdef APRENDIZAJE_TIEMPOS

static uint16_t hist[APRENDIZAJE_MAX_DATOS]; // cocientes real/estimado en milesimas
static uint8_t n_hist = 0;
static float factor = 1.0f;

static void calcular_factor() {
  if (n_hist < APRENDIZAJE_MIN_DATOS) { factor = 1.0f; return; }
  // Mediana (robusta ante cocientes atipicos), no media simple
  uint16_t orden[APRENDIZAJE_MAX_DATOS];
  for (uint8_t i = 0; i < n_hist; i++) orden[i] = hist[i];
  for (uint8_t i = 1; i < n_hist; i++) {            // ordenacion por insercion
    const uint16_t v = orden[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && orden[j] > v) { orden[j + 1] = orden[j]; j--; }
    orden[j + 1] = v;
  }
  const uint32_t med = (n_hist & 1) ? orden[n_hist / 2]
                                    : ((uint32_t)orden[n_hist / 2 - 1] + orden[n_hist / 2]) / 2;
  float f = (float)med / 1000.0f;
  if (f < APRENDIZAJE_FACTOR_MIN) f = APRENDIZAJE_FACTOR_MIN;
  if (f > APRENDIZAJE_FACTOR_MAX) f = APRENDIZAJE_FACTOR_MAX;
  factor = f;
}

float tiempo_factor() { return factor; }
uint8_t tiempo_n_datos() { return n_hist; }

static void agregar_dato(const uint16_t v) {
  if (n_hist >= APRENDIZAJE_MAX_DATOS) {
    for (uint8_t i = 1; i < n_hist; i++) hist[i - 1] = hist[i];
    n_hist = APRENDIZAJE_MAX_DATOS - 1;
  }
  hist[n_hist++] = v;
}

// Lee FACTOR.TXT (un numero por linea). Sin SD conserva lo que hubiera en memoria.
static void cargar_historial() {
  if (!card.isMounted()) return;
  n_hist = 0;
  SdFile f;
  if (abrir(f, PSTR(APRENDIZAJE_ARCHIVO), O_READ)) {
    char buf[96];
    const int16_t len = f.read(buf, sizeof(buf) - 1);
    f.close();
    if (len > 0) {
      uint32_t v = 0;
      bool hay = false;
      for (int16_t i = 0; i <= len; i++) {
        const char c = (i < len) ? buf[i] : '\n';
        if (c >= '0' && c <= '9') {
          if (v < 100000UL) v = v * 10 + (c - '0');
          hay = true;
        }
        else if (c == '\n' || c == '\r') {
          if (hay && v > 0) agregar_dato(v > 65535UL ? 65535 : (uint16_t)v);
          v = 0; hay = false;
        }
      }
    }
  }
  calcular_factor();
}

static void guardar_historial() {
  char buf[APRENDIZAJE_MAX_DATOS * 7 + 1];
  char *p = buf;
  for (uint8_t i = 0; i < n_hist; i++) p += sprintf_P(p, PSTR("%u\n"), (unsigned)hist[i]);
  SdFile f;
  if (abrir(f, PSTR(APRENDIZAJE_ARCHIVO), O_CREAT | O_WRITE | O_TRUNC)) {
    f.write(buf, (uint16_t)(p - buf));
    f.close();
  }
}

#endif // APRENDIZAJE_TIEMPOS

//
// ---------------- Registro ----------------
//
#ifdef REGISTRO_IMPRESIONES

static void anexar_csv(const char * const linea, const uint16_t len) {
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_ARCHIVO), O_CREAT | O_WRITE | O_APPEND)) return;
  if (f.fileSize() == 0) {
    static const char cab[] = "n,archivo,estimado_s,real_s,resultado\n";
    f.write(cab, (uint16_t)(sizeof(cab) - 1));
  }
  f.write(linea, len);
  f.close();
}

static void escribir_marca(const uint32_t n) {
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_PENDIENTE), O_CREAT | O_WRITE | O_TRUNC)) return;
  char buf[48];
  const int c = sprintf_P(buf, PSTR("%lu,%s\n"), (unsigned long)n, nombre);
  if (c > 0) f.write(buf, (uint16_t)c);
  f.close();
}

static void vaciar_marca() {
  SdFile f;
  if (abrir(f, PSTR(REGISTRO_PENDIENTE), O_CREAT | O_WRITE | O_TRUNC)) f.close();
}

// Si la marca tiene contenido, la impresion anterior quedo sin cerrar: se anota como interrumpida.
static void revisar_pendiente(const bool corte_luz) {
  if (activo || !card.isMounted()) return;
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_PENDIENTE), O_READ)) return;
  char buf[48];
  const int16_t len = f.read(buf, 40);
  f.close();
  if (len <= 0) return;
  int16_t l = len;
  while (l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) l--;
  if (l <= 0) { vaciar_marca(); return; }
  buf[l] = '\0';
  char linea[96], mot[40];
  motivo_a_texto(mot, corte_luz ? REG_CORTE : REG_INTERRUMPIDA);
  const int c = sprintf_P(linea, PSTR("%s,0,0,%s\n"), buf, mot);
  if (c > 0) anexar_csv(linea, (uint16_t)c);
  vaciar_marca();
}

// Ultimas lineas para la pantalla
static char resumen[5][28];
static uint8_t n_resumen = 0;

// "n,archivo,est,real,motivo" -> "#n 65m motivo"
// El CSV guarda un codigo fijo (done/filament/power/interrupted/cancelled); aqui se muestra en el idioma activo.
// Lineas antiguas (texto en espanol) se muestran tal cual.
static const char* motivo_traducido(const char *m) {
  if (!strncmp(m, "done", 4))        return GET_TEXT(MSG_KNE_REG_DONE);
  if (!strncmp(m, "filament", 8))    return GET_TEXT(MSG_KNE_REG_FILAMENT);
  if (!strncmp(m, "power", 5))       return GET_TEXT(MSG_KNE_REG_POWER);
  if (!strncmp(m, "interrupted", 11)) return GET_TEXT(MSG_KNE_REG_INTERRUPTED);
  if (!strncmp(m, "cancelled", 9))   return GET_TEXT(MSG_KNE_REG_CANCELLED);
  return m;
}

static void formatear(const char *l, char *d) {
  const char *c[4];
  uint8_t k = 0;
  for (const char *p = l; *p && k < 4; p++) if (*p == ',') c[k++] = p;
  if (k < 4) { strncpy(d, l, 27); d[27] = '\0'; return; }
  const uint16_t n = (uint16_t)atol(l);
  const uint16_t minutos = (uint16_t)((atol(c[2] + 1) + 30) / 60);
  const int w = sprintf_P(d, PSTR("#%u %um "), (unsigned)n, (unsigned)minutos);
  uint8_t i = 0;
  const char *m = motivo_traducido(c[3] + 1);
  while (m[i] && i < 12 && w + i < 27) { d[w + i] = m[i]; i++; }
  d[w + i] = '\0';
}

uint8_t registro_cargar_ultimas() {
  n_resumen = 0;
  #ifdef APRENDIZAJE_TIEMPOS
    cargar_historial();
  #endif
  if (!card.isMounted()) return 0;
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_ARCHIVO), O_READ)) return 0;
  const uint32_t tam = f.fileSize();
  const uint16_t cuanto = tam > 384 ? 384 : (uint16_t)tam;
  char buf[385];
  int16_t len = 0;
  if (f.seekSet(tam - cuanto)) len = f.read(buf, cuanto);
  f.close();
  if (len <= 0) return 0;
  buf[len] = '\0';

  const char *ult[5];
  uint8_t n = 0;
  char *p = buf;
  if (cuanto < tam) { while (*p && *p != '\n') p++; if (*p) p++; } // descarta la primera linea (puede estar cortada)
  while (*p) {
    char *fin = p;
    while (*fin && *fin != '\n') fin++;
    const bool completa = (*fin == '\n');
    if (*fin) *fin = '\0';
    if (p[0] >= '0' && p[0] <= '9') {
      if (n == 5) { for (uint8_t i = 1; i < 5; i++) ult[i - 1] = ult[i]; n = 4; }
      ult[n++] = p;
    }
    p = completa ? fin + 1 : fin;
  }
  for (uint8_t i = 0; i < n; i++) formatear(ult[i], resumen[i]);
  n_resumen = n;
  return n;
}

const char* registro_linea(const uint8_t i) { return i < n_resumen ? resumen[i] : ""; }

#endif // REGISTRO_IMPRESIONES

//
// ---------------- Ganchos ----------------
//
void registro_inicio() {
  if (activo || !card.isFileOpen()) return;  // solo impresiones desde SD

  const char *src = card.longest_filename();
  uint8_t i = 0;
  for (; src[i] && i < sizeof(nombre) - 1; i++)
    nombre[i] = (src[i] == ',' || src[i] == '\n' || src[i] == '\r') ? ';' : src[i];
  nombre[i] = '\0';

  #ifdef APRENDIZAJE_TIEMPOS
    cargar_historial(); // el factor se lee al iniciar, no en cada refresco
  #endif
  #ifdef REGISTRO_IMPRESIONES
    revisar_pendiente(false);  // por si quedo una impresion anterior sin cerrar
    escribir_marca(print_job_timer.getStats().totalPrints);
  #endif

  activo = true;
  finalizada = filamento_visto = false;
  estimado_s = 0;
  prep_s = 0;
  prep_hecha = false;
}

void registro_fin(const uint32_t real_s) {
  if (!activo) return;
  activo = false;
  const uint8_t m = finalizada ? REG_FINALIZADA : (filamento_visto ? REG_FILAMENTO : REG_CANCELADA);

  #ifdef REGISTRO_IMPRESIONES
    char linea[112], mot[40];
    motivo_a_texto(mot, m);
    const int c = sprintf_P(linea, PSTR("%lu,%s,%lu,%lu,%s\n"),
      (unsigned long)print_job_timer.getStats().totalPrints, nombre,
      (unsigned long)estimado_s, (unsigned long)real_s, mot);
    if (c > 0) anexar_csv(linea, (uint16_t)c);
    vaciar_marca();
  #endif

  #ifdef APRENDIZAJE_TIEMPOS
    // El estimado del laminador NO incluye la preparacion (espera termica, malla): se descuenta.
    const uint32_t util_s = (real_s > prep_s) ? (real_s - prep_s) : real_s;
    // Solo se aprende si M1100 descuenta la preparacion (si no, el calentamiento infla el cociente)
    // y se descartan los cocientes absurdos (fuera de 0.4 - 3.0).
    if (m == REG_FINALIZADA && prep_hecha && estimado_s > 0 && util_s > 0) {
      const uint32_t q = (util_s * 1000UL) / estimado_s;
      if (q >= 400UL && q <= 3000UL) {
        agregar_dato((uint16_t)q);
        calcular_factor();
        guardar_historial();
      }
    }
  #endif
}

void registro_prep_fin() { if (activo) { prep_s = print_job_timer.duration(); prep_hecha = true; } }
void registro_motivo_finalizada() { if (activo) finalizada = true; }
void registro_marca_filamento()   { if (activo) filamento_visto = true; }

void registro_estimado_m73(const uint32_t minutos) {
  if (activo && estimado_s == 0 && minutos > 0) estimado_s = minutos * 60UL;
}

void registro_arranque(const bool corte_luz) {
  #ifdef REGISTRO_IMPRESIONES
    revisar_pendiente(corte_luz);
  #else
    UNUSED(corte_luz);
  #endif
}

#endif // REGISTRO_APRENDIZAJE_ACTIVO
