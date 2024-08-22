//-------------------------------------------------------------------------------------------------------
//  VSTGUI Test View plugin
//  Copyright (c) 2004 Arne Scheffler. All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//-------------------------------------------------------------------------------------------------------

#ifndef __pdrawtesteditor__
#define __pdrawtesteditor__

#ifndef __vstcontrols__
#include "vstcontrols.h"
#endif

class ControlsGUI;

class DrawTestEditor : public AEffGUIEditor, CControlListener
{
public:
	DrawTestEditor (void* effect);
	virtual ~DrawTestEditor ();

	void setTabView (CFrame* frame, const CRect& r, long position);

	virtual void valueChanged (CDrawContext *pContext, CControl *pControl);

protected:
	virtual bool open (void *ptr);
	virtual void close ();
	virtual void idle ();

	CBitmap* backgroundBitmap;
	ControlsGUI* controlsView;
};

#endif
