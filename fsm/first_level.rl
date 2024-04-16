%%{
    machine frob_first_level;

    FOREIGN_FRAME_LRC_OK      = 0;
    FOREIGN_FRAME_LRC_NOK     = 1;
    FOREIGN_ACK               = 2;
    FOREIGN_NAK               = 3;
    TIMEOUT                   = 4;
    INTERNAL_IDEMPOTENT_FRAME = 5;
    INTERNAL_FRAME            = 6;

    action Retransmit { }
    action Send { }
    action Arm { }
    action Disarm { }
    action Forward { }
    action Ack { }
    action Nak { }

    _ack     = FOREIGN_ACK @Disarm;
    _nak     = FOREIGN_NAK @Retransmit _ack;
    foreign  = (FOREIGN_FRAME_LRC_OK @Ack @Forward | FOREIGN_FRAME_LRC_NOK @Nak);
    ack      = INTERNAL_FRAME @Send FOREIGN_ACK;
    nak      = INTERNAL_FRAME @Send FOREIGN_NAK @Retransmit FOREIGN_ACK;

    ack_idem = INTERNAL_IDEMPOTENT_FRAME @Arm @Send _ack;
    nak_idem = INTERNAL_IDEMPOTENT_FRAME @Arm @Send _nak;
    timeout  = INTERNAL_IDEMPOTENT_FRAME @Send @Arm TIMEOUT @Retransmit |
               INTERNAL_IDEMPOTENT_FRAME @Send @Arm TIMEOUT @Retransmit TIMEOUT @Retransmit;
    internal = ack | nak | ack_idem | nak_idem | timeout (_ack | _nak);

    main := (foreign | internal)*;

    write data;
}%%
