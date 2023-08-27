#include "g_local.h"

// Wrapper function for old va() args print methods...
template <typename... Args>
void Print_Fmtargs(edict_t *ent, print_receiver_t receiver, const char *fmt, Args... args) {
    // Format the message using fmt::format
    std::string message = fmt::format(fmt, args...);

    switch (receiver) {
        case PCLIENT:
            // Handle client printing logic
            gi.Client_Print(ent, message.c_str());
            break;
        case PCENTER:
            // Call the original Center_Print function with the formatted message
            gi.Center_Print(ent, message.c_str());
            break;
        case PLOC:
            // Handle location-based printing logic
            gi.LocCenter_Print(ent, message.c_str());
            break;
    }
}

template <typename... Args>
void LocPrint_Fmtargs(edict_t *ent, print_receiver_t receiver, print_type_t printtype, const char *fmt, Args... args) {
    // Format the message using fmt::format
    std::string message = fmt::format(fmt, args...);

    switch (receiver) {
        case PCLIENT:
            // Handle client printing logic
            gi.Client_Print(ent, message.c_str());
            break;
        case PCENTER:
            // Call the original Center_Print function with the formatted message
            gi.Center_Print(ent, message.c_str());
            break;
        case PLOC:
            // Handle location-based printing logic
            gi.LocCenter_Print(ent, message.c_str());
            break;
    }
}