#include <boost/sml.hpp>
#include <cassert>
#include <iostream>
#include <typeinfo>

namespace sml = boost::sml;

namespace {
    template <class R, class... Ts>
    auto call_impl(R (*f)(Ts...)) {
        return [f](Ts... args) { return f(args...); };
    }
    template <class T, class R, class... Ts>
    auto call_impl(T* self, R (T::*f)(Ts...)) {
        return [self, f](Ts... args) { return (self->*f)(args...); };
    }
    template <class T, class R, class... Ts>
    auto call_impl(const T* self, R (T::*f)(Ts...) const) {
        return [self, f](Ts... args) { return (self->*f)(args...); };
    }
    template <class T, class R, class... Ts>
    auto call_impl(const T* self, R (T::*f)(Ts...)) {
        return [self, f](Ts... args) { return (self->*f)(args...); };
    }
/**
 * Simple wrapper to call free/member functions
 * @param args function, [optional] this
 * @return function(args...)
 */
    auto call = [](auto... args) { return call_impl(args...); };

    struct Context { int i{}; };

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

    struct e1 {};
    struct e2 {};
    struct e3 {};
    struct e4 {};
    struct e5 {};

    auto guard1 = [] {
        std::cout << "guard1" << std::endl;
        return true;
    };

    auto guard2 = [](Context& ctx) {
        assert(42 == ctx.i);
        std::cout << "guard2" << std::endl;
        return false;
    };

    bool guard3(Context& ctx) {
        assert(42 == ctx.i);
        std::cout << "guard3" << std::endl;
        return true;
    }

    auto action1 = [](auto e) { std::cout << "action1: " << typeid(e).name() << std::endl; };
    struct action2 {
        void operator()(Context& ctx) {
            assert(42 == ctx.i);
            std::cout << "action2" << std::endl;
        }
    };

    struct actions_guards {
        auto operator()() noexcept {
            using namespace sml;
            return make_transition_table(
                *"idle"_s + event<e1> = "s1"_s
                , "s1"_s + event<e2> [ guard2 ] / action1 = "s2"_s
                , "s1"_s + event<e2> [ !guard2 ] / action1 = "s2"_s
                , "s2"_s + event<e3> [ guard1 && ![] { return false;} ] / (action1, action2{}) = "s3"_s
                , "s3"_s + event<e4> [ !guard1 || guard2 ] / (action1, [] { std::cout << "action3" << std::endl; }) = "s4"_s
                , "s3"_s + event<e4> [ guard1 ] / ([] { std::cout << "action4" << std::endl; }, [this] { action4(); }) = "s5"_s
                , "s5"_s + event<e5> [ call(guard3) || guard2 ] / call(this, &actions_guards::action5) = X
                );
        }

        void action4() const { std::cout << "action4" << std::endl; }

        void action5(Context& ctx, const e5&) {
            assert(42 == ctx.i);
            std::cout << "action5" << std::endl;
        }
    };
}  // namespace

int main() {
    Context ctx{42};
    Logger log;

    sml::sm<actions_guards> sm{ctx};
    sm.process_event(e1{});
    sm.process_event(e2{});
    sm.process_event(e3{});
    sm.process_event(e4{});
    sm.process_event(e5{});
    assert(sm.is(sml::X));
}
