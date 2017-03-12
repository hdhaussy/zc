# zc
A netcat like utility for ZMQ

    Usage: zc [OPTION]... TYPE ENDPOINT
    OPTION: one of the following options:
     -h --help : print this message
     -b --bind : bind instead of connect
     -n --nbiter : number of iterations (0 for infinite loop)
     -v --verbose : print some messages in stderr
    TYPE: set ZMQ socket type in req/rep/pub/sub/push/pull
    ENDPOINT: a string consisting of two parts as follows: transport://address (see zmq documentation)


Some examples of use:

    $ echo "Reply" | zc -b -n 1 rep ipc://reqreptest &
    $ echo "Request" | zc -v -n 1 req ipc://reqreptest

    $ zc -b sub ipc://pubsubtest &
    $ echo "Hello" | zc -n 1 pub ipc://pubsubtest
    $ echo "World" | zc -n 1 pub ipc://pubsubtest


