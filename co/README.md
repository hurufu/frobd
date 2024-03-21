# On implementation of cooperating routines

I'm well aware of [C++ critique of "fibers"][1]. They mention this as a problem:

> [...] it [1:N fibers] creates even bigger problem: any blocking call
> completely stops progress of all N fibers [...]

I consider this as an actual desirable property, to guarantee sequential access
to all shared variables.

I think most problems with fibers are because implementers need to handle
arbitrary code and be very generic and with advanced scheduling. My cooperative
tasks have are use-case specific and this avoids a lot of problems.

I've considered a lot of alternatives, they are all too fat: e.g. [pth][2].

Also interesting to read:

  * https://en.wikipedia.org/wiki/Computer_multitasking after reading this my
      implementation is better named *multiprogramming* or even *multiroutining*?
  * https://en.wikipedia.org/wiki/Batch_processing
  * https://en.wikipedia.org/wiki/Spooling
  * https://en.wikipedia.org/wiki/Coroutine

[1]: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1364r0.pdf "Fibers under the magnifying glass"
[2]: https://www.gnu.org/software/pth/pth-manual.html "GNU Portable Threads"
