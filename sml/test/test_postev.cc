#include <iostream>
#include <cassert>
#include <boost/sml.hpp>

namespace sml = boost::sml;

struct e1 {};
struct e2 {};

const auto emit = [](auto& ev, auto&sm, auto& dep, auto& sub){ sm.process_event(e2{}, dep, sub); };

struct table {
    auto operator()() const noexcept {
        using namespace sml;
        return make_transition_table(
            *"s1"_s + event<e1> / emit = "s3"_s
//            *"s1"_s + event<e1>  = "s3"_s
            ,"s1"_s + event<e2> = "s2"_s
            ,"s3"_s + event<e2> = "s4"_s
        );
    }
};

int main() {
    using namespace sml;
    sm<table> sm;

    sm.process_event(e1{});
    assert(sm.is("s4"_s));
}
