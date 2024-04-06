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
