#pragma once

#include "quantum.h"

/* True while Left Alt is held on the vim nav layer (local HL-mode only;
 * Alt is not sent to the host). Used by layer overlay rendering. */
bool vim_nav_lalt_held(void);
