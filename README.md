# frobd - auto responder/bouncer/forwarder that implements FROB ECR-EFT protocol

** Currnetly this project is even less than PoC, don't use yet. **

You can find specification at <frob.pl>

Sending `SIGPWR` will print some internal statistics to the log
Sending `SIGINT` will enter debug console

Pros:

  * Written in C
  * Doesn't use dynamic memory allocation
      * Suitable for use on highly constrained devices
      * Uses compilation time configurable buffer for incomming data
      * No memory leaks (duh!)
  * Good (but opinionated) programming style
      * Only single global variable - `g_log_level`

Cons:

  * Written in C
  * A little bit late to the party
