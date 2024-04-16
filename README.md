# frobd - auto responder/bouncer/forwarder that implements FROB ECR-EFT protocol

> **Warning**
> This project is in a PoC state and __a lot__ of essentials features are missing.

More information for the protocol on the [official website][b]. Publicly
available specification can be downloaded [here][c]. I know that frobd is a bad
name for such program, but I couldn't come up with anything better.

## Usage

It doesn't support any command line switches and is supposed to run only from
the source tree. This will be rectified in the future.

It can be used as a standalone program or as a librarty (feature that I can
implement upon request).

#### Build and test (requires `ragel` and `check` packages)

```shell
make && make test
```

#### Start TCP server/client (requires `s6-networking` package)

```shell
make tcp-server
# or
make tcp-client
```

#### Show frame matching FSM diagram (requires additionally `graphviz` and `feh`)

```shell
make graph-dot
# or
make graph-fdp
```

## What is it?

__frobd__ is a program that listens for ECR-EFT messages on stdin and responds
to them on stdout (or different file descriptors) while forwarding some messages
through other channels. It is expected to be a part of a larger system
integrated with embedded PoS or ECR software (the client). It is planned to
handle as much as possible messages by itself (`T1`-`T5`, `D0`-`DA`, `B1`-`B4`,
`L1`) and support unlimited number of connections, while forwarding only
essential messages to the client, taking care that it will receive only messages
relevant to an actual transaction and only from a single connected device at a
time per transaction, so it has to deal only with a small number of messages
using a simple interface.

For example a payment software can stop worrying about one ECR asking for a
previous transaction status while other ECR is requesting a new transaction ;).

### Planned features

It should take care of protocol version negotiation, encryption, synchronisation
between multiple clients, parameters delivery, printer control, etc.

### Current state

It runs! It parses messages. It can reply to some messages with hard-coded
responses and if enabled in the code it will forward objects to any other file
descriptor.

## Why?

Communication protocols are a notorious source of security vulnerabilities and
plain bugs. Everything including parsing of actual data units, dealing with
incomplete and/or ambiguous specification, correct operation in the presence of
races between different clients, and handling incorrect input is quite expensive
for companies to develop correctly and is often overlooked in exchange for
delivery time and features that are demanded by business.

A good proof of my words is a [table on the official website][e]. If this
specification was so easy to implement there wouldn't be a need in this
monstrous compatibility matrix.

This is an attempt to create and open source implementation, that given enough
demand can give a rise to reference implementation shared by different
businesses, so they can concentrate on features and not on the technicalities
of the protocol.

### Pros

  * Written in C
  * Parsing is done using [Ragel][d] FSM compiler – very robust
  * Doesn't use dynamic memory allocation
      * Suitable for use on highly constrained devices
      * Uses compilation time configurable buffer for incoming data
      * No memory leaks (duh!)
  * Good (but opinionated) programming style[^1]
      * Only single global variable - `g_log_level`
      * Plays nicely with [s6][f] suite of tools

### Cons

  * Written in C
  * Late to the party, there are [plenty][a] of implementations already.
  * Doesn't use dynamic memory allocation
      * Can't handle messages larger than 4Kb.
      * Uses [VLA][g] in couple of places.
  * Far from finished

### Asynchronous control using signals

  * Sending `SIGPWR` will print some internal statistics to the log
  * Sending `SIGINT` will enter debug console

### But really why to even bother writing it?

I just wanted to write something using Ragel compiler, to gain more experience
with it. And also because many commercial implementations _really_ s*ck and that
gave me a little bit of motivation to do something with my free time.

[a]: https://docs.google.com/spreadsheets/d/e/2PACX-1vRiI60ObK9intEg0UeKCH4VsrhnbugVFbIsJbklCG4XqIVjvR3YCbW9a07BpVjrbw/pubhtml/sheet?gid=832359086
[b]: https://frob.pl/protokol-ecr-eft/
[c]: https://archiwum_mpit.bip.gov.pl/kasy-on-line/kasy-on-line.html
[d]: http://www.colm.net/open-source/ragel/
[e]: https://frob.pl/protokol-ecr-eft/#ECREFT
[f]: https://skarnet.org/software/s6/
[g]: https://en.cppreference.com/w/c/language/array#Variable-length_arrays

[^1]: Yet to be achieved

# On implementation of cooperating routines

I'm well aware of [C++ critique of "fibers"][1]. They mention this as a problem:

> [...] it [1:N fibers] creates even bigger problem: any blocking call
> completely stops progress of all N fibers [...]

I consider this as an actual desirable property, to guarantee sequential access
to all shared variables. The rest of critique doesn't apply to my use case.

I think most problems with fibers are because implementers need to handle
arbitrary code and be very generic and with advanced scheduling. My cooperative
tasks have are use-case specific and this avoids a lot of problems.

I've considered a lot of alternatives, they are all too fat: e.g. [pth][2],
[qemu][3], etc.

I've used and idea of a special channel for `Waitmsg` events from [Plan 9][4].

Also interesting to read:

  * [Computer multitasking](https://en.wikipedia.org/wiki/Computer_multitasking)
    after reading this my implementation is better named *multiprogramming* or
    even *multiroutining*?
  * [Batch processing](https://en.wikipedia.org/wiki/Batch_processing)
  * [Spooling](https://en.wikipedia.org/wiki/Spooling)
  * [Coroutine](https://en.wikipedia.org/wiki/Coroutine)

[1]: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1364r0.pdf "Fibers under the magnifying glass"
[2]: https://www.gnu.org/software/pth/pth-manual.html "GNU Portable Threads"
[3]: https://gsb16.github.io/qemu/qemu/coroutine.html "QEMU coroutine implementation"
[4]: https://9fans.github.io/plan9port/man/man3/thread.html
