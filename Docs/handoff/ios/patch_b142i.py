# b142i (Evan 11:39 "Opponent button doesn't work in training"): found by -ZoogiTrainBotsTest - MenuAction sent every "Train*"
# action to the Royale sandbox (RoyaleTrainAction) before the b133 "TrainBots" branch, so the classic training CPU OPPONENTS press
# did nothing. "TrainBots" now skips that route.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiGameMode.cpp', '''	else if (Action.ToString().StartsWith(TEXT("Train"))) RoyaleTrainAction(Action, Index); // TRAINING sandbox menu (ZoogiRoyale.cpp)''', '''	else if (Action.ToString().StartsWith(TEXT("Train")) && Action != "TrainBots") RoyaleTrainAction(Action, Index); // TRAINING sandbox menu (ZoogiRoyale.cpp); b142i: not the classic CPU OPPONENTS''')
print('b142i ok')
