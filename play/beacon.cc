#include <czmq.h>

int main(int argc, char* argv[])
{
    zactor_t *beacon = zactor_new (zbeacon, NULL);

    zactor_destroy (&beacon);


    return 0;
}
