## Features to consider

### Testing

   * [x] Use DejaGnu to test the program with different configurations and inputs.
   * [x] Use [Mull][6] to perfrom mutation testing on the code.
   * [ ] Use [KLEE][7] to perform symbolic execution on the code.
   * [x] Use gcov to generate coverage reports.
   * [-] Use fuzzing to test the program with random inputs. It can be done with
         [AFL][10] or [libFuzzer][11].

### Signal handling

  - [x] SIGINT should enter the interactive console
  - [x] SIGINFO/SIGPWR should display status or statistics
  - [-] SIGSTP/SIGUSR1 should enter "busy" mode - can be used for synchronization
        between different instances

### Configurability

It will be useful if configuration will define some sort of scriptlets that
specify how frobd should react to different messages and conditions.

What shall be considered:

  * [ ] Enabled protocols versions and attached devices parameters to respond to
        `Tx` and `D4`.
  * [-] Timeouts and re-transmission policy, eg don't re-transmit non-idempotent
        messages like `S1`.
  * [x] Periodic ping using `T1` message.
  * [ ] Chat scenarios
  * [ ] Configurable number of channels and instructions on where each message
        type should go. This can be achieved eg by embeding a scripting language
        like GNU Guile or Lua or something less common; or alternatively by
        executing external shell script.

### Additional programs

#### GUI

Write GUI that uses REPL from SIGINT. It should be able to connect to a remote
live ECR or EFT.

#### LUI

Add line edit interface to the debug console using rlwrap program.

#### Rule system (probably isn't needed for 99% of use cases)

Forward all normally auto-responded messages to a special channel with a rule
system attached on the other side, for more inteligent reponses.

Sample rule (tbd):

When terminal is busy, then always reject any `Kx` message with 993 response code
otherwise don't respond to `K0` message at all, always respond with 0 to `K1`
message, and reject any other `Kx` message with response code 999.

```Prolog
% Matches response message according to recevied one and current environment
env_received_sent(E, [type-[k,_]|_], [type-[k,0],result-993]) :-
    env_property_value(E, busy, true).
env_received_sent(E, R, S) :-
    env_property_value(E, busy, false),
    received_sent_(R, S).

received_sent_([type-[k,0]|_], []).
received_sent_([type-[k,1]|_], [type-[k,0],result-0]).
received_sent_([type-[k,X]|_], [type-[k,0],result-999]) :-
    maplist(dif(X), [0,1]).
```

Problems to solve:
  * How to represent dependencies on previous messages?

Possible topics to have a look at:
  * [Situation calculus][1]
  * [Event calculus][2]
  * [Fluents][3],
    [Fluent programming][5]
  * [Action language][4]

## Interesting links

  * https://github.com/fuzzdb-project/fuzzdb
  * https://www.fuzzingbook.org/
  * https://tratt.net/laurie/src/supuner/
  * https://www.gnu.org/software/coreutils/timeout
  * https://justine.lol/ape.html

[1]: https://en.wikipedia.org/wiki/Situation_calculus
[2]: https://en.wikipedia.org/wiki/Event_calculus
[3]: https://en.wikipedia.org/wiki/Fluent_(artificial_intelligence)
[4]: https://www.researchgate.net/publication/2276002_Reasoning_about_Fluents_in_Logic_Programming
[5]: https://www.researchgate.net/publication/2841600_Fluent_Logic_Programming
[6]: https://github.com/mull-project/mull
[7]: https://klee.github.io/
[10]: http://lcamtuf.coredump.cx/afl/
[11]: https://llvm.org/docs/LibFuzzer.html
