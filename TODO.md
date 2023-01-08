#### Additional programs

Write GUI that uses REPL from SIGINT. It should be able to connect to a remote
live ECR or EFT.

Add line edit interface using rlwrap program.

#### Signal handling

  - [x] SIGINT should enter the interactive console
  - [ ] SIGQUIT should gracefully quit (without core dump)
  - [x] SIGINFO/SIGPWR should display status or statistics
  - [ ] SIGSTP/SIGUSR1 should enter "busy" mode - can be used for synchronization
        between different instances

#### Configurability

It will be useful if configuration will define some sort of scriptlets that
specify how frobd should react to different messages and conditions.

What shall be specifiable:

  * Enabled protocols versions and attached devices parameters to respond to
    `Tx` and `D4`.
  * Timeouts and re-transmission policy, eg don't re-transmit non-idempotent
    messages like `S1`.
  * Chat scenarios
