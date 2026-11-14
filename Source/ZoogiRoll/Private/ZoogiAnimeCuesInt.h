// b164: shared between ZoogiAnimeCues.cpp (runtime + clip layer) and ZoogiAnimeDirector.cpp (the PC editor panel)
#pragma once
#include "ZoogiAnimeCues.h"
namespace ZoogiAnimeCues
{
	namespace Int
	{
		extern int32 GSel;            // the director's selected cue (-1 none)
		extern bool GEditPortrait;    // the director edits the portrait window (shows a phone frame)
		extern bool GDirector;        // director panel open
		extern bool GEditLayout;      // drag / resize the selected cue's window on screen
		void MarkDirty();             // autosave soon
		void EnsureLayer();
		/** the screen-space rect (px) of a cue window inside the view (Size = layer size), portrait frame when bPortrait */
		FBox2D WindowRect(const FRect01& R, const FVector2D& Size, bool bPortrait);
		FBox2D PhoneFrame(const FVector2D& Size);
		void OpenPanel(); void ClosePanel();
	}
}
