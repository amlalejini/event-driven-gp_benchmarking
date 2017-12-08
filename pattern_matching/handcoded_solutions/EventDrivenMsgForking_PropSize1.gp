Fn-0000000000000000:
  SetMem(15,1)
  SetMem(14,0)
  SetMem(13,8)
  GetRoleCnt(12)

  Countdown(13)
  Close

  GetRole(8)
  TestEqu(8,12,1)
  If(1)
    SetMem(8,0)
    SetRole(8)
    Commit(8,9)
  Close

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


Fn-1111111111111111:
  Input(8,8)
  SetRole(8)

Fn-0000000011111111:
  Input(9,9)
  Commit(9,9)
  SetMem(0,2)
  Div(9,0,8)
  SetRole(8)
