#pragma once
#include "bindings.h"

void settings_load_dips(int game);
// A saved DIP change requests a hardware reset. The caller resumes the clock.
bool settings_edit(int game, Bindings *bindings);
