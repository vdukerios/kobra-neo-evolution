#pragma once
// Restricciones del proyecto para edicion y recuperacion. No son limites fisicos certificados.
#define KNE_CURRENT_MIN 400
#define KNE_CURRENT_MAX 1200
inline bool kne_current_valid(const long value) {
  return value >= KNE_CURRENT_MIN && value <= KNE_CURRENT_MAX;
}
inline uint16_t kne_current_restore(const uint16_t value, const uint16_t factory) {
  return kne_current_valid(value) ? value : factory;
}
