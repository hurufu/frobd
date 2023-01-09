## Features to consider

### Signal handling

  - [x] SIGINT should enter the interactive console
  - [ ] SIGQUIT should gracefully quit without core dump, because there are
        plenty of signals that generate core dumps, and this signal can be sent
        from the controling terminal (in comparison to SIGTERM)
  - [x] SIGINFO/SIGPWR should display status or statistics
  - [ ] SIGSTP/SIGUSR1 should enter "busy" mode - can be used for synchronization
        between different instances

### Configurability

It will be useful if configuration will define some sort of scriptlets that
specify how frobd should react to different messages and conditions.

What shall be considered:

  * Enabled protocols versions and attached devices parameters to respond to
    `Tx` and `D4`.
  * Timeouts and re-transmission policy, eg don't re-transmit non-idempotent
    messages like `S1`.
  * Chat scenarios

### Additional programs

#### GUI

Write GUI that uses REPL from SIGINT. It should be able to connect to a remote
live ECR or EFT.

#### LUI

Add line edit interface to the debug console using rlwrap program.

#### Rule system

Forward all normally auto-responded messages to a special channel with a rule
system attached on the other side, for more inteligent reponses.

Sample rule (tbd):

```Prolog
% Matches response message according to recevied one and current environment
env_received_sent(Env, msg([type(k,X)|_]), msg([type(k,0),result(999)])) :-
    env_property_value(Env, busy, false),
    maplist(dif(X), [0,1]).
```

Possible topics to have a look at:
  * [Situation calculus][1]
  * [Event calculus][2]
  * [Fluents][3]
  * [Action language][4]

[1]: https://en.wikipedia.org/wiki/Situation_calculus
[2]: https://en.wikipedia.org/wiki/Event_calculus
[3]: https://en.wikipedia.org/wiki/Fluent_(artificial_intelligence)
[4]: https://www.researchgate.net/publication/2276002_Reasoning_about_Fluents_in_Logic_Programming
