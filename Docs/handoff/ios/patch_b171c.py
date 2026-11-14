import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
p=R+'Private/ZoogiFront.cpp'; s=open(p,'rb').read().decode('utf-8')
old='''		if (!bTrickChecked && T > 1.4f && Pop == EPop::None && Screen == 0)'''
new='''		{	// b171: -ZoogiTrickTest - opens the daily trick shot pop-up and shoots it (Evan: "fill the white with the image")
			static const bool bTrickTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTrickTest"));
			static int32 TrickTestStep = 0;
			if (bTrickTest && TrickTestStep == 0 && T > 2.5f) { Pop = EPop::Trick; PopT = T; TrickTestStep = 1; }
			else if (bTrickTest && TrickTestStep == 1 && T - PopT > 1.0f) { Shot(TEXT("trick_pop")); TrickTestStep = 2; UE_LOG(LogZoogi, Log, TEXT("FRONT trick test shot")); }
		}
		if (!bTrickChecked && T > 1.4f && Pop == EPop::None && Screen == 0)'''
for o,n in ((old,new),(old.replace('\n','\r\n'),new.replace('\n','\r\n'))):
    if s.count(o)==1: open(p,'wb').write(s.replace(o,n).encode('utf-8')); print('OK'); break
else: print('MISS'); sys.exit(1)
