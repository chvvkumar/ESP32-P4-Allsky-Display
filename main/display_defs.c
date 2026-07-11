/*
 * Runtime panel profile table. Both round DSI panels are described here; the active one is
 * selected at boot from persisted config (see display_defs.h). Values match the authoritative
 * hardware table in .planning/specs/display-driver.md section 2.
 */
#include <stddef.h>
#include "display_defs.h"

static const allsky_panel_profile_t s_profiles[] = {
    { .type_id = 1, .name = "3.4INCH-DSI", .width = 800, .height = 800,
      .panel_init_byte = 0x00, .rotation = ALLSKY_PANEL_ROTATION },
    { .type_id = 2, .name = "4INCH-DSI",   .width = 720, .height = 720,
      .panel_init_byte = 0x04, .rotation = ALLSKY_PANEL_ROTATION },
};

#define ALLSKY_PROFILE_COUNT (sizeof(s_profiles) / sizeof(s_profiles[0]))

/* Active profile index; defaults to the type-1 (3.4") profile until set. */
static int s_active_index = 0;

static int index_for_type(int type_id)
{
    for (size_t i = 0; i < ALLSKY_PROFILE_COUNT; ++i) {
        if (s_profiles[i].type_id == type_id) {
            return (int)i;
        }
    }
    return 0; /* unknown -> type 1 (3.4") */
}

allsky_panel_profile_t allsky_panel_profile_for(int type_id)
{
    return s_profiles[index_for_type(type_id)];
}

void allsky_panel_set_type(int type_id)
{
    s_active_index = index_for_type(type_id);
}

allsky_panel_profile_t allsky_panel_profile(void)
{
    return s_profiles[s_active_index];
}
