// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See game/language.h.
//
// NFS: Undercover: the game's language table and boot call aren't found yet,
// so the Language row is hidden (user/settings.cpp) and the game picks its
// language from the console's, as on the disc.
#include <stdafx.h>
#include <game/language.h>

namespace game::language
{
    std::string MenuNote(int32_t) { return std::string(); }
    void SetLaunchLanguage(int32_t) {}
}
