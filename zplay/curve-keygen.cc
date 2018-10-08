#include <czmq.h>

int main(void)
{
    zcert_t *client_cert = zcert_new ();
    zcert_set_meta (client_cert, "name", "CurveZMQ Test Certificate");
    zcert_save_public (client_cert, "testcert.pub");
    int rc = zcert_save (client_cert, "testcert.crt");
    assert (rc == 0);
    zcert_destroy (&client_cert);
    return 0;
}
