# Zoogi Roll - moving to a new laptop
Branch: feat/p5-style (this worktree was C:\dev\ZoogiRollUnreal-p5style on the desktop PC).
1. Install Unreal Engine 5.6, Visual Studio 2022 (Game dev with C++), Git (+ Git LFS).
2. git clone https://github.com/evanjamesuh/ZoogiRollUE.git C:\dev\ZoogiRollUnreal-p5style  then  git checkout feat/p5-style
3. Copy the files listed in LARGE_FILES_NOT_IN_GIT.txt from the desktop PC (same paths) - GitHub refuses files over 100 MB.
4. Right-click ZoogiRoll.uproject > Generate Visual Studio project files, then open it (it builds the C++ the first time).
5. Build notes: Docs/handoff/HANDOFF.md. The PC build scripts (p5run.py, bNNNios.py, gobNNNgo.py) are in Docs/handoff/ios -
   they expect C:\dev\ios and the MacinCloud Mac for the iOS / TestFlight step.
