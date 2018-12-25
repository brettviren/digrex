#!/usr/bin/env python3
'''
This spews out trigger primitives
'''
import sys
import zmq
import time
import click
import numpy
from collections import namedtuple, defaultdict

def log(string):
    sys.stderr.write(string + '\n')
    sys.stderr.flush()

def make_socket(sockspec, bc="bind"):
    '''
    Return a final socket given spec string like:
      [<connect|bind>:]<type>:<scheme>://<host>:<port>
    '''
    if sockspec.startswith("bind") or sockspec.startswith("connect"):
        bc, sockspec = sockspec.split(":", 1)
    stype, endpoint = sockspec.split(":", 1)
    context = zmq.Context()
    stypenum = getattr(zmq, stype.upper())
    sock = context.socket(stypenum)
    getattr(sock, bc)(endpoint)
    log ("make socket: %s %s %s" %(bc, stype, endpoint))
    return sock


@click.group()
@click.pass_context
def cli(ctx, **kwds):
    ctx.obj.update(kwds)

@cli.command("source")
@click.option('-r','--rate', default=1.0, type=float,
              help='Mean packet rate in Hz')
@click.option('-j','--jitter', default=1.0, type=float,
              help='Gaussian jitter time in seconds')
@click.option('-n','--number', default=0, type=int,
              help='Number to send or zero to run forever')
@click.option('-t','--topic', default="*",
              help='Topic string, if pub socket')
@click.argument('sockspec', nargs=1)
@click.pass_context
def source(ctx, rate, jitter, number, topic, sockspec):
    '''
    A soure of messages.
    '''
    sock = make_socket(sockspec, bc="bind")
    count = 0
    if "pub:" in sockspec.lower() and topic:
        log ('setting topic to "%s"' % topic)
    while True:
        count += 1
        if number and count > number:
            break

        # this sleep emulates some physical process
        wait = numpy.random.exponential(1.0/rate)
        time.sleep(wait)

        # this time emulates the time some data is sampled plus some jitter
        now = time.time() + numpy.random.normal(0, jitter)
        nows = int(now)
        nowns = int((now-nows)*1e9)
        msg = "%s %d %d %d"%(topic, count, nows, nowns)
        sock.send_string(msg)
        #log (msg)

@cli.command("sink")
@click.option('-n','--number', default=0, type=int,
              help='Number to recv or zero to run forever')
@click.option('-t','--topic', default="*",
              help='Topic string, if pub socket')
@click.argument('sockspecs', nargs=-1)
@click.pass_context
def sink(ctx, number, topic, sockspecs):
    '''
    A sink of messages.
    '''
    poller = zmq.Poller()
    socks = list()
    for ss in sockspecs:
        s = make_socket(ss, bc='connect')
        if 'sub' in ss.lower() and topic:
            s.setsockopt_string(zmq.SUBSCRIBE, topic)
        poller.register(s, zmq.POLLIN)
        socks.append(s)

    count = 0
    keep_going = True
    while keep_going:
        socks = dict(poller.poll())
        for sock in socks:
            count += 1
            if number and count > number:
                break
            msg = sock.recv_string()
            #log("%s %s" % (msg, sock.LAST_ENDPOINT))
        

@cli.command("fanin")
@click.option('-n','--number', default=0, type=int,
              help='Number to recv or zero to run forever')
@click.option('-t','--topic', default="*",
              help='Topic string, if pub socket')
@click.option('-T','--tardy', default=1.0,
              help='Allowed time delay in seconds after which a source is considered tardy')
@click.option('-D','--dwell', default=1.0,
              help='Dwell time to consider messages unrelated')
@click.option('-o','--output-sockspec', type=str,
              help='Output sockspec')
@click.argument('input-sockspecs', nargs=-1, required=True)
@click.pass_context
def fanin(ctx, number, topic, tardy, dwell, output_sockspec, input_sockspecs):
    '''
    Knife goes in, guts come out.
    '''
    poller = zmq.Poller()
    insocks = list()
    lasts = dict()
    for iss in input_sockspecs:
        s = make_socket(iss, bc='connect')
        if 'sub' in iss.lower() and topic:
            s.setsockopt_string(zmq.SUBSCRIBE, topic)
        poller.register(s, zmq.POLLIN)
        lasts[s] = None
        insocks.append(s)
    outsock = None
    if output_sockspec:
        outsock = make_socket(output_sockspec, bc="bind")

    

    # dtime=data time, rtime=recieved time
    Message = namedtuple("Message", "topic,count,dtime,rtime")
    def msg_to_str(msg):
        return "%s %d %d %d" % msg
    def make_message(string):
        chunks = msg.split()
        return Message(chunks[0], int(chunks[1]),
                       float(chunks[2]) + 1e-9* int(chunks[3]),
                       time.time())

    class Source(object):
        queue = list()
        rtime = None            # most recent
        dtime = None            # oldest
        def push(self, msg):
            m = make_message(msg)
            self.rtime = m.rtime
            if self.dtime is None:
                self.dtime = m.dtime
            self.queue.append(m)
            return m

        def have(self, dtime):
            'Return number of messages with m.dtime < dtime'
            ind=0
            for m in self.queue:
                if m.dtime < dtime:
                    ind += 1
                else:
                    break
            return ind

        def pop(self, dtime):
            'Return and pop all messages with m.dtime < dtime'
            ind = self.have(dtime)
            ret = self.queue[0:ind]
            self.queue = self.queue[ind:]
            if self.queue:
                self.dtime = self.queue[0].dtime
            elif ret:
                self.dtime = ret[-1].dtime
            return ret

    class Sources(object):
        sources = defaultdict(Source)
        dtime = None
        rtime = None

        def push(self, sock, msg):
            return self.sources[sock].push(msg)

        def too_eager(self):
            if self.rtime is None:
                self.rtime = time.time()
                return True
            tardy_time = self.rtime + tardy
            their_rtimes = [s.rtime for s in self.sources.values() if s.rtime]
            their_early = [t < tardy_time for t in their_rtimes]
            if any(their_early):
                return True
            self.rtime = min(their_rtimes)
            return False


        def set_dtime(self):
            assert(self.sources)
            dtimes = [s.dtime for s in self.sources.values() if s.dtime]
            assert(dtimes)
            self.dtime = min(dtimes)
            #log ("my dtime=%f" % (self.dtime))
            assert(self.dtime)

        def pop_late(self):
            'Remove any late arrivals'
            lost = [s.pop(self.dtime) for s in self.sources.values()]
            nlost = sum(map(len, lost))
            if nlost > 0:
                log ("lost %d data at %f" % (nlost, self.dtime))

            return lost
            
        def pop_up_to_gap(self):
            min_dtime = min([s.dtime for s in self.sources.values()])
            ret = list()

            while True:
                fresh = list()
                for s in self.sources.values():
                    some = s.pop(min_dtime + dwell)
                    if not some:
                        continue
                    min_dtime = some[-1].dtime
                    fresh += some
                if fresh:
                    ret += fresh
                    continue
                else:
                    #log ("Nothing fresh at %f" % (min_dtime))
                    pass
                break
            ret.sort(key=lambda m: m.dtime)
            if ret:
                self.dtime = ret[-1].dtime
            #log ("pop to %f" %(self.dtime,))
            return ret

        def proc(self):
            if self.dtime is None:
                self.set_dtime()
            if self.too_eager():
                return ()

            # at this point dtime is all that matters.

            lost = self.pop_late()

            return self.pop_up_to_gap()

    sources = Sources()

    count = 0
    keep_going = True
    last_dtime = None
    while keep_going:
        socks = dict(poller.poll())
        for sock in socks:
            msg = sock.recv_string()
            sources.push(sock, msg)

        msgs = sources.proc()
        if not msgs:
            continue
        if last_dtime is None:
            last_dtime = sources.dtime
        else:
            dt = sources.dtime - last_dtime
            n = len(msgs)
            log("send: %d in %.2f %.1f Hz" %(n,dt,n/dt))
            last_dtime = sources.dtime
        for msg in msgs:
            if outsock:         # fixme: should aggregate
                outsock.send_string(msg_to_str(msg))
            

if __name__ == '__main__':
    cli(obj=dict())
