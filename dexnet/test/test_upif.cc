
#include <czmq.h>
#include "upif.h"

int main () {
    upif::cache c;
    auto plugin = c.add("dexnet");
    assert(plugin);
    auto raw = plugin->rawsym("funcs_test");
    assert(raw);

    // pointer to function
    typedef int (*funcptr)(int);
    funcptr fnp;

    bool ok = plugin->symbol("funcs_test", fnp);
    assert(ok);

    const int x = 42;
    const int y = (*fnp)(x);
    assert(x == y);
    
    return 0;
}
