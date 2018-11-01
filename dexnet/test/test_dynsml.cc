#include <iostream>
#include "boost/sml.hpp"
namespace sml = boost::sml;

class Context {
public:
    void chirp() {
        std::cout << "chirp\n";
    }
};

struct evX{};
struct Init{};
struct Idle{};

const auto see_x = [](Context& ctx, const auto& ev) {
    ctx.chirp();
};

struct FSM {


    auto operator()() const noexcept {
        using namespace boost::sml;
        return make_transition_table (
            * state<Init> = state<Idle>
            , state<Idle> + event<evX> / see_x = X 
            );
    }
};


typedef sml::sm<FSM> FSM_t;

class Foo {
    Context ctx;
    FSM_t fsm;
public:
    Foo() : fsm{ctx} {}
    void proc() {
        fsm.process_event(evX{});
    }

};


int main()
{
    Foo foo;
    
    foo.proc();

    return 0;
}
