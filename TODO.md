#### Additional programs

Write GUI that uses REPL from SIGINT. It should be able to connect to a remote
live ECR or EFT.

Add line edit interface to the debug console using rlwrap program.

Forward all normally auto-responded messages to a special channel with a rule
system attached on the other side, for more inteligent reponses. Key topics:
  * Situation calculus
  * Fluents
  * Action language

#### Signal handling

  - [x] SIGINT should enter the interactive console
  - [ ] SIGQUIT should gracefully quit without core dump, because there are
        plenty of signals that generate core dumps, and this signal can be sent
        from the controling terminal (in comparison to SIGTERM)
  - [x] SIGINFO/SIGPWR should display status or statistics
  - [ ] SIGSTP/SIGUSR1 should enter "busy" mode - can be used for synchronization
        between different instances

#### Configurability

It will be useful if configuration will define some sort of scriptlets that
specify how frobd should react to different messages and conditions.

What shall be considered:

  * Enabled protocols versions and attached devices parameters to respond to
    `Tx` and `D4`.
  * Timeouts and re-transmission policy, eg don't re-transmit non-idempotent
    messages like `S1`.
  * Chat scenarios
