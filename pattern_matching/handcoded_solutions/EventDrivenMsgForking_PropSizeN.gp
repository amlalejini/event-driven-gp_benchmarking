Fn-0000000000000000:
  GetUID(11)
  SetOpinion(11)
  SetMem(15,1)
  SetMem(14,0)
  SetMem(13,4)
  GetRoleCnt(12)

  RotDir(14)
  Countdown(13)
    ActivateFacing
    RotCW
    Call()[1100000000000000]
  Close

  While(15)
    Call()[1100000000000000]

    SetMem(2,0)
    GetUID(0)
    GetOpinion(1)

    TestEqu(0,1,2)

    If(2)
      SetMem(8,0)
      SetRole(8)
      Commit(8,9)
    Close

    GetRole(8)

    RotDir(14)
    Output(8,8)
    ActivateFacing
    SendMsgFacing()[1111111111111111]

    RotCW
    Pull(9,9)
    Inc(9)
    Output(9,9)
    ActivateFacing
    SendMsgFacing()[0000000011111111]
  Close

Fn-1111111111111111:
  Input(8,8)
  SetRole(8)

Fn-0000000011111111:
  Input(9,9)
  Commit(9,9)
  SetMem(0,2)
  Div(9,0,8)
  SetRole(8)

Fn-1000000000000000:
  Input(11,1)
  GetOpinion(0)
  TestLess(1,0,2)
  If(2)
    SetOpinion(1)

Fn-1100000000000000:
  GetOpinion(11)
  Output(11,11)
  BroadcastMsg()[1000000000000000]
