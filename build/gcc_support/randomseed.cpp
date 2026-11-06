// Only used when G26_MESH_VALIDATION is enabled: the project declares randomSeed() (wirish_math.h) but never defines it.
// random(min,max) in framework/cores/Arduino.cpp is based on rand(), so seed rand().
#include <stdlib.h>
void randomSeed(unsigned int seed) { srand(seed); }
