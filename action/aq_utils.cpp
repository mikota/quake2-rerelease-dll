#include "g_local.h"

// Wrapper function for old va() args print methods...
template <typename... Args>
void Debug_Print(edict_t *ent, const char *fmt, Args... args) {
    std::string message = fmt::format(fmt, args...);

    gi.Com_PrintFmt(ent, message.c_str());
}
