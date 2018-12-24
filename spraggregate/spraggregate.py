#!/usr/bin/env python3
'''
This spews out trigger primitives
'''

import zmq
import time
import click
import numpy

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
    print ("make socket: %s %s %s" %(bc, stype, endpoint))
    return sock


@click.group()
@click.pass_context
def cli(ctx, **kwds):
    ctx.obj.update(kwds)

@cli.command("generator")
@click.option('-r','--rate', default=1.0, type=float,
              help='Mean packet rate in Hz')
@click.option('-n','--number', default=0, type=int,
              help='Number to send or zero to run forever')
@click.option('-t','--topic', default="",
              help='Topic string, if pub socket')
@click.argument('sockspec', nargs=1)
@click.pass_context
def generator(ctx, rate, number, topic, sockspec):
    '''
    Generate messages at Poisson random times
    '''
    sock = make_socket(sockspec, bc="bind")
    count = 0
    if "pub:" in sockspec.lower() and topic:
        print ('setting topic to "%s"' % topic)
    while True:
        count += 1
        if number and count > number:
            break
        wait = numpy.random.exponential(1.0/rate)
        time.sleep(wait)
        print ("[%s] %d %f" % (topic, count, wait))
        if topic:
            sock.send_string("%s %d"%(topic, count))
        else:
            sock.send_string("%d"%(count,))
    
@cli.command("consumer")
@click.option('-n','--number', default=0, type=int,
              help='Number to recv or zero to run forever')
@click.option('-t','--topic', default="",
              help='Topic string, if pub socket')
@click.argument('sockspecs', nargs=-1)
@click.pass_context
def consumer(ctx, number, topic, sockspecs):
    '''
    Consume messages.
    '''
    poller = zmq.Poller()
    socks = list()
    lasts = dict()
    for ss in sockspecs:
        s = make_socket(ss, bc='connect')
        if 'sub' in ss.lower() and topic:
            s.setsockopt_string(zmq.SUBSCRIBE, topic)
        poller.register(s, zmq.POLLIN)
        lasts[s] = None
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
            t,c = msg.split()
            c = int(c)
            last = lasts[sock]
            if last is None:
                lasts[sock] = c
                continue
            if c-last != 1:
                print ("missed. this=%d last=%d on %s" % (c, last, sock.LAST_ENDPOINT))
            lasts[sock] = c
        


if __name__ == '__main__':
    cli(obj=dict())
