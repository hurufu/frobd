# Integration scenarios of frob program

By design one should spawn a separate frob instance for each connection.
Performance isn't an issue, because normally only few ECRs are connected to the
POS terminal at the same time (usually 1 or 2). The most convenient way to do
this is to use superserver like `s6-tcpserver` from [s6][a]. If you need to
reverse the connection, you can use `s6-tcpclient`. If you need to use serial
port, you can just redirect `/dev/ttyS0` to frob's stdin/stdout.

Because of very flexible I/O mechanism (file descriptors) employed by frob it
can be integrated into a larger system in many different ways.

## Integration with big monolithic payment application

                 |
    +-------+ TCP|socket      UNIX domain socket +------------------+
    | ECR 1 | <--|-----> frob <----------------> |                  |
    +-------+    |                               | Large monolithic |
    +-------+ TCP|socket      UNIX domain socket | application      |
    | ECR 2 | <--|-----> frob <----------------> |                  |
    +-------+    |                               +------------------+
                 |
                 |Physical boundary

For a large application it may be useful to have a single file descriptor for a
bidirectional communication, to which all messages are written. This can be done
by using a UNIX domain socket, created by `socketpair(2)` system call. From which
one end is passed to frob program, and the other end is passed to the payment
application.

## Integration with payment applications consisting of multiple processes

                                Named pipes
    ┌───────┐ TCP|socket      |--ui-------->[1]          ┌─────────────┐
    │ ECR 1 │ <--|-----> frob |--payment--->[2]   [1]--->│ GUI process │
    └───────┘    |            |--storage--->[3]          └─────────────┘
                 |            |<--device----[4]                 ↕
                 |                                       ┌─────────────┐
    ┌───────┐ TCP|socket      |--ui-------->[1]   [2]--->│             │
    │ ECR 2 │ <--|-----> frob |--payment--->[2]   [3]--->│ Terminal SW │
    └───────┘    |            |--storage--->[3]   [4]<---│             │
                 |            |<--device----[4]          └─────────────┘
                 |
                 |Physical boundary


Frob has 4 distinct I/O channels, which can be assigned different file
descriptors and connect to different processes.

## Integration with [nexo][b] payment application using microservice architecture

                            Named pipes or sockets
    +-------+ TCP|socket      |--ui-------->[1]          +-------+
    | ECR 1 | <--|-----> frob |--payment--->[2]   [1]--->| SCAP  |
    +-------+    |            |--storage--->[3]          +-------+
                 |            |<--device----[2]              ↕
                 |                                       +--------------+
                                                  [2]<-->|     FAST     |
    +-------+ TCP|socket      |--ui-------->[1]          +--------------+
    | ECR 2 | <--|-----> frob |--payment--->[2]             ↕       ↕
    +-------+    |            |--storage--->[3]          +-----+ +------+
                 |            |<--device----[2]   [3]--->| HAP | | TMAP |
                 |                                       +-----+ +------+
                 |Physical boundary

Nexo specifies 3 distinct modules that are responsible for different parts of
the payment process. Frob channels are neatly modeled by these modules. Read
more about Nexo in [Nexo standards][b]. Synchronisation is done by frob,
applications don't need to worry about it.


## Integration with a proxy

                 |
    +-------+ TCP|socket      TCP socket +------+
    | ECR 1 | <--|-----> frob <--------> |      |     +-------------+
    +-------+    |                       | Proxy|<--->| Payment     |
    +-------+ TCP|socket      TCP socket |      |     | application |
    | ECR 2 | <--|-----> frob <--------> |      |     +-------------+
    +-------+    |                       +------+
                 |
                 |Physical boundary

Conversely you can convert all communication via a proxy to another ECR
protocol, eg Nexo retaler protocol.

[a]: http://skarnet.org/software/s6/
[b]: http://www.nexo-standards.org/
