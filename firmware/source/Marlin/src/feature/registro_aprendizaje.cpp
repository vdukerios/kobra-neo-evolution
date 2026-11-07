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
static uint32_t registro_numero = 0;
static uint32_t csv_inicio = 0;   // durable marker also records the append boundary
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
static bool historial_sucio = false;
static uint32_t historial_generacion = 0;
static int8_t historial_copia = -1;
struct HistorialCopia {
  uint32_t magic, generation, count;
  uint16_t values[APRENDIZAJE_MAX_DATOS];
  uint32_t crc;
};
static uint32_t historial_checksum(const HistorialCopia &v) {
  const uint8_t *p = (const uint8_t*)&v;
  uint32_t crc = 2166136261UL;
  for (size_t i = 0; i < offsetof(HistorialCopia,crc); ++i) crc = (crc ^ p[i]) * 16777619UL;
  return crc;
}
static const char* historial_nombre(const int8_t copia) { return copia ? "FACTOR1.DAT" : "FACTOR0.DAT"; }
static bool leer_copia(const int8_t copia, HistorialCopia &v) {
  SdFile f;
  if (!abrir(f, historial_nombre(copia), O_READ)) return false;
  const bool ok = f.fileSize() == sizeof(v) && f.read(&v, sizeof(v)) == sizeof(v);
  const bool cerrado = f.close();
  return ok && cerrado && v.magic == 0x4B4E4546UL && v.count <= APRENDIZAJE_MAX_DATOS && v.crc == historial_checksum(v);
}
static bool guardar_historial();

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
  historial_sucio = true;
}

// Lee FACTOR.TXT (un numero por linea). Sin SD conserva lo que hubiera en memoria.
static void cargar_historial() {
  if (!card.isMounted()) return;
  if (historial_sucio) { guardar_historial(); return; }
  HistorialCopia a, b;
  const bool va = leer_copia(0, a), vb = leer_copia(1, b);
  if (va || vb) {
    historial_copia = vb && (!va || (int32_t)(b.generation - a.generation) > 0) ? 1 : 0;
    const HistorialCopia &v = historial_copia ? b : a;
    n_hist = (uint8_t)v.count;
    memcpy(hist, v.values, sizeof(hist));
    historial_generacion = v.generation;
    calcular_factor(); return;
  }
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
  historial_sucio = false;
  calcular_factor();
}

static bool guardar_historial() {
  if (!historial_sucio) return true;
  HistorialCopia v = { 0x4B4E4546UL, historial_generacion + 1, n_hist, {0}, 0 };
  memcpy(v.values, hist, sizeof(hist)); v.crc = historial_checksum(v);
  const int8_t siguiente = historial_copia == 0 ? 1 : 0;
  SdFile f;
  // The previous verified copy (or FACTOR.TXT during migration) is untouched.
  if (!abrir(f, historial_nombre(siguiente), O_CREAT | O_WRITE | O_TRUNC)) return false;
  const bool escrito = f.write((const uint8_t*)&v, sizeof(v)) == sizeof(v) && !f.writeError;
  const bool cerrado = f.close();
  HistorialCopia verificado;
  if (!escrito || !cerrado || !leer_copia(siguiente, verificado) || memcmp(&v, &verificado, sizeof(v))) return false;
  historial_copia = siguiente; historial_generacion = v.generation;
  historial_sucio = false; return true;
}

#endif // APRENDIZAJE_TIEMPOS

//
// ---------------- Registro ----------------
//
#ifdef REGISTRO_IMPRESIONES

// Only one pending print exists. Its key is the unchanged "n,archivo," prefix.
// A retry inspects complete tail rows and removes an incomplete final row first.
static bool anexar_csv(const char * const linea, const uint16_t len, const uint32_t desde) {
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_ARCHIVO), O_CREAT | O_READ | O_WRITE)) return false;
  const uint32_t tam = f.fileSize();
  if (desde > tam) { f.close(); return false; }
  const uint16_t cuanto = tam > 512 ? 512 : (uint16_t)tam;
  char cola[513];
  if (!f.seekSet(tam - cuanto) || f.read(cola, cuanto) != cuanto) { f.close(); return false; }
  cola[cuanto] = '\0';
  size_t clave = 0; uint8_t comas = 0;
  while (clave < len && comas < 2) if (linea[clave++] == ',') ++comas;
  if (comas != 2) { f.close(); return false; }
  bool encontrado = false;
  uint16_t inicio = 0, completo = 0;
  if (tam > cuanto) { while (inicio < cuanto && cola[inicio] != '\n') ++inicio; if (inicio < cuanto) ++inicio; }
  for (uint16_t i = inicio; i < cuanto; ++i) if (cola[i] == '\n') {
    if (tam - cuanto + inicio >= desde && i - inicio >= clave && !memcmp(cola + inicio, linea, clave)) encontrado = true;
    completo = i + 1; inicio = i + 1;
  }
  if (cuanto && cola[cuanto - 1] != '\n') {
    // No known complete boundary: do not truncate an unbounded/corrupt row.
    if (tam > cuanto && !completo) { f.close(); return false; }
    if (!f.truncate(tam - cuanto + completo)) { f.close(); return false; }
  }
  if (encontrado) return f.close();
  if (!f.seekEnd()) { f.close(); return false; }
  bool ok = true;
  if (f.fileSize() == 0) {
    static const char cab[] = "n,archivo,estimado_s,real_s,resultado\n";
    ok = f.write(cab, sizeof(cab) - 1) == sizeof(cab) - 1;
  }
  if (ok) ok = f.write(linea, len) == len;
  ok = ok && !f.writeError;
  const bool cerrado = f.close();
  return ok && cerrado;
}

static bool escribir_marca(const uint32_t n) {
  SdFile csv;
  if (!abrir(csv, PSTR(REGISTRO_ARCHIVO), O_CREAT | O_READ | O_WRITE)) return false;
  csv_inicio = csv.fileSize();
  if (!csv.close()) return false;
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_PENDIENTE), O_CREAT | O_WRITE | O_TRUNC)) return false;
  char buf[64];
  const int c = sprintf_P(buf, PSTR("%lu,%s,%lu\n"), (unsigned long)n, nombre, (unsigned long)csv_inicio);
  const bool ok = c > 0 && c < (int)sizeof(buf) && f.write(buf, (uint16_t)c) == c && !f.writeError;
  const bool cerrado = f.close();
  return ok && cerrado;
}
static bool vaciar_marca() {
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_PENDIENTE), O_WRITE)) return false;
  const bool ok = f.truncate(0);
  const bool cerrado = f.close();
  return ok && cerrado;
}
static bool revisar_pendiente(const bool corte_luz) {
  if (activo || !card.isMounted()) return false;
  SdFile f;
  if (!abrir(f, PSTR(REGISTRO_PENDIENTE), O_READ)) {
    // Exclusive create distinguishes missing from unreadable; never overwrite a failed read.
    if (!abrir(f, PSTR(REGISTRO_PENDIENTE), O_CREAT | O_EXCL | O_WRITE)) return false;
    return f.close();
  }
  char buf[64];
  const uint32_t tam = f.fileSize();
  const int16_t len = f.read(buf, sizeof(buf) - 1);
  const bool cerrado = f.close();
  if (!cerrado || len < 0) return false;
  if (!len) return true;
  if ((uint32_t)len != tam || buf[len - 1] != '\n') return false;
  int16_t l = len;
  while (l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) --l;
  if (l <= 0) return vaciar_marca();
  buf[l] = '\0';
  // A torn marker must remain available for inspection, never become a CSV row.
  if (buf[0] < '0' || buf[0] > '9' || !strchr(buf, ',')) return false;
  uint32_t desde = 0;
  char *separador = strchr(strchr(buf, ',') + 1, ',');
  if (separador) {
    *separador++ = '\0';
    if (!*separador) return false;
    for (const char *p = separador; *p; ++p) {
      if (*p < '0' || *p > '9' || desde > (0xFFFFFFFFUL - (*p - '0')) / 10UL) return false;
      desde = desde * 10UL + (*p - '0');
    }
  } // Old two-field markers remain readable (boundary zero).
  char linea[96], mot[40];
  motivo_a_texto(mot, corte_luz ? REG_CORTE : REG_INTERRUMPIDA);
  const int c = sprintf_P(linea, PSTR("%s,0,0,%s\n"), buf, mot);
  return c > 0 && c < (int)sizeof(linea) && anexar_csv(linea, (uint16_t)c, desde) && vaciar_marca();
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
    if (completa && p[0] >= '0' && p[0] <= '9') {
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
    registro_numero = print_job_timer.getStats().totalPrints;
    if (!revisar_pendiente(false) || !escribir_marca(registro_numero)) { SERIAL_ERROR_MSG("Print log marker failed"); return; }
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
      (unsigned long)registro_numero, nombre,
      (unsigned long)estimado_s, (unsigned long)real_s, mot);
    if (c > 0 && c < (int)sizeof(linea) && anexar_csv(linea, (uint16_t)c, csv_inicio)) {
      if (!vaciar_marca()) SERIAL_ERROR_MSG("Print log pending cleanup failed");
    }
    else SERIAL_ERROR_MSG("Print log append failed; marker retained");
  #endif

  #ifdef APRENDIZAJE_TIEMPOS
    // El estimado del laminador NO incluye la preparacion (espera termica, malla): se descuenta.
    const uint32_t util_s = (real_s > prep_s) ? (real_s - prep_s) : real_s;
    // Solo se aprende si M1100 descuenta la preparacion (si no, el calentamiento infla el cociente)
    // y se descartan los cocientes absurdos (fuera de 0.4 - 3.0).
    if (m == REG_FINALIZADA && prep_hecha && estimado_s > 0 && util_s > 0) {
      const uint64_t q = ((uint64_t)util_s * 1000UL) / estimado_s;
      if (q >= 400UL && q <= 3000UL) {
        agregar_dato((uint16_t)q);
        calcular_factor();
        if (!guardar_historial()) SERIAL_ERROR_MSG("Time factor storage failed");
      }
    }
  #endif
}

void registro_prep_fin() { if (activo) { prep_s = print_job_timer.duration(); prep_hecha = true; } }
void registro_motivo_finalizada() { if (activo) finalizada = true; }
void registro_marca_filamento()   { if (activo) filamento_visto = true; }

void registro_estimado_m73(const uint32_t minutos) {
  if (activo && estimado_s == 0 && minutos > 0 && minutos <= 0xFFFFFFFFUL / 60UL) estimado_s = minutos * 60UL;
}

void registro_arranque(const bool corte_luz) {
  #ifdef REGISTRO_IMPRESIONES
    revisar_pendiente(corte_luz);
  #else
    UNUSED(corte_luz);
  #endif
}

#endif // REGISTRO_APRENDIZAJE_ACTIVO
