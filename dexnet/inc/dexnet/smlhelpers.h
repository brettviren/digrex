#ifndef DEXNET_SMLHELPERS_H_SEEN
#define DEXNET_SMLHELPERS_H_SEEN

#include <typeinfo>
#include <cxxabi.h>

namespace dexnet {

    namespace helpers {
        template<typename T>
        const char* tname(const T& o)
        {
            int status=0;
            return abi::__cxa_demangle(typeid(o).name(), NULL, NULL, &status);
        }


        // The SML policy that logs

        struct Logger {
            template <class SM, class TEvent>
            void log_process_event(const TEvent& ev) {
                zsys_debug("[%s][process_event] %s",
                           tname(SM{}), tname(ev));
            }

            template <class SM, class TGuard, class TEvent>
            void log_guard(const TGuard& g, const TEvent& ev, bool result) {
                zsys_debug("[%s][guard] %s %s %s",
                           tname(SM{}), tname(g), tname(ev),
                           (result ? "[OK]" : "[Reject]"));
            }

            template <class SM, class TAction, class TEvent>
            void log_action(const TAction& a, const TEvent& ev) {
                zsys_debug("[%s][action] %s %s",
                           tname(SM{}), tname(a), tname(ev));
            }

            template <class SM, class TSrcState, class TDstState>
            void log_state_change(const TSrcState& src, const TDstState& dst) {
                zsys_debug("[%s][transition] %s -> %s",
                           tname(SM{}), src.c_str(), dst.c_str());
            }
        };
    }
}
#endif
