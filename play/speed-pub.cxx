#include <zmq.hpp>
#include <string>
#include <iostream>
#include <ctime>

#include "dune.pb.h"

const char PA_TOPIC = 'P';

const std::string ADDR = "127.0.0.1";
const std::string PORT = "9876";


class PASource {
public:
    PASource() {
    }

    void run() {
        
};

int main (int argc, char* argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;


    int fragid = 1;
    if (argc > 1) {
        atoi(argv[1]);
    }

    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_PUB);
    socket.connect ("tcp://"+ADDR+":"+PORT);

    const time_t t0 = time(0);

    int count = 0;
    while (true) {
        count += 1;
        const time_t t = time(0);
        const int ref_tick = int((t-t0)/ 0.5e-6);
        
        PrimitiveActivity pa;
        pa.set_fragid(fragid);
        pa.set_count(count);
        pa.set_reference_tick(ref_tick);

        for (int ind = 0; ind < 10; ++ind) {
            auto * ca = pa.add_primitives();
            ca->set_channel(ind+48);
            ca->set_start_tick(40);
            ca->set_duration(std::abs(ind-5));
            ca->set_activity((ind-5)*(ind-5));
        }

        std::string dat;
        pa.SerializeToString(&dat);

        zmq::message_t reply (dat.size());
        memcpy ((void *) reply.data (), dat.c_str(), dat.size());
        socket.send (reply);

    }
    return 0;
}
