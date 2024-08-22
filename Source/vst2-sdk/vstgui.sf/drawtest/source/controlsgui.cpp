#ifndef __controlsgui__
#include "controlsgui.h"
#endif

#include "cfileselector.h"

#include <stdio.h>

enum
{
	// bitmaps
	kBackgroundBitmap = 10001,
	
	kSliderHBgBitmap,
	kSliderVBgBitmap,
	kSliderHandleBitmap,

	kSwitchHBitmap,
	kSwitchVBitmap,

	kOnOffBitmap,

	kKnobHandleBitmap,
	kKnobBgBitmap,

	kDigitBitmap,
	kRockerBitmap,

	kVuOnBitmap,
	kVuOffBitmap,

	kSplashBitmap,

	kMovieKnobBitmap,

	kMovieBitmap,

	// others
	kBackgroundW = 420,
	kBackgroundH = 210
};
//-----------------------------------------------------------------------------
// CLabel declaration
//-----------------------------------------------------------------------------
class CLabel : public CParamDisplay
{
public:
	CLabel (CRect &size, char *text);

	void draw (CDrawContext *pContext);

	void setLabel (char *text);
	virtual bool onDrop (CDrawContext* context, CDragContainer* drag, const CPoint& where);
	virtual void onDragEnter (CDrawContext* context, CDragContainer* drag, const CPoint& where);
	virtual void onDragLeave (CDrawContext* context, CDragContainer* drag, const CPoint& where);
	virtual void onDragMove (CDrawContext* context, CDragContainer* drag, const CPoint& where);

protected:
	char label[256];
	bool focus;
};

//-----------------------------------------------------------------------------
// CLabel implementation
//-----------------------------------------------------------------------------
CLabel::CLabel (CRect &size, char *text)
: CParamDisplay (size)
, focus (false)
{
	strcpy (label, "");
	setLabel (text);
}

//------------------------------------------------------------------------
void CLabel::setLabel (char *text)
{
	if (text)
		strcpy (label, text);
	setDirty ();
}

bool CLabel::onDrop (CDrawContext* context, CDragContainer* drag, const CPoint& where)
{
	long size, type;
	void* ptr = drag->first (size, type);
	if (ptr)
	{
		setLabel ((char*)ptr);
	}
	return true;
}

void CLabel::onDragEnter (CDrawContext* context, CDragContainer* drag, const CPoint& where)
{
	getFrame ()->setCursor (kCursorCopy);
	focus = true;
	setDirty ();
}

void CLabel::onDragLeave (CDrawContext* context, CDragContainer* drag, const CPoint& where)
{
	getFrame ()->setCursor (kCursorNotAllowed);
	focus = false;
	setDirty ();
}

void CLabel::onDragMove (CDrawContext* context, CDragContainer* drag, const CPoint& where)
{
}

//------------------------------------------------------------------------
void CLabel::draw (CDrawContext *pContext)
{
	pContext->setFillColor (backColor);
	pContext->fillRect (size);
	pContext->setLineWidth (focus ? 2 : 1);
	pContext->setFrameColor (fontColor);
	pContext->drawRect (size);

	pContext->setFont (fontID);
	pContext->setFontColor (fontColor);
	pContext->drawString (label, size, false, kCenterText);
	setDirty (false);
}

enum
{
	kSliderHTag = 0,
	kSliderVTag,
	kKnobTag,

	kNumParams,

	kOnOffTag,
	kKickTag,
	kMovieButtonTag,
	kAutoAnimationTag,
	kOptionMenuTag,

	kRockerSwitchTag,
	kSwitchHTag,
	kSwitchVTag,

	kSplashTag,
	kMovieBitmapTag,
	kAnimKnobTag,
	kDigitTag,
	kTextEditTag,

	kAbout
};

ControlsGUI::ControlsGUI (const CRect &inSize, CFrame *frame, CBitmap *pBackground)
: CViewContainer (inSize, frame, pBackground)
{
	setMode (kOnlyDirtyUpdate);

	// get version
	int version = (VSTGUI_VERSION_MAJOR << 16) + VSTGUI_VERSION_MINOR;
	int verMaj = (version & 0xFF00) >> 16;
	int verMin = (version & 0x00FF);

	// init the background bitmap
	CBitmap *background = new CBitmap (kBackgroundBitmap);

	setBackground (background);

	background->forget ();

	CPoint point (0, 0);

	//--COnOffButton-----------------------------------------------
	CBitmap *onOffButton = new CBitmap (kOnOffBitmap);

	CRect size (0, 0, onOffButton->getWidth (), onOffButton->getHeight () / 2);
	size.offset (20, 20);
	cOnOffButton = new COnOffButton (size, this, kOnOffTag, onOffButton);
	addView (cOnOffButton);


	//--CKickButton-----------------------------------------------
 	size.offset (70, 0);
	point (0, 0);
	cKickButton = new CKickButton (size, this, kKickTag, onOffButton->getHeight() / 2, onOffButton, point);
	addView (cKickButton);


	//--CKnob--------------------------------------
	CBitmap *knob   = new CBitmap (kKnobHandleBitmap);
	CBitmap *bgKnob = new CBitmap (kKnobBgBitmap);

 	size (0, 0, bgKnob->getWidth (), bgKnob->getHeight ());
	size.offset (140 + 15, 15);
	point (0, 0);
	cKnob = new CKnob (size, this, kKnobTag, bgKnob, knob, point);
	cKnob->setInsetValue (7);
	addView (cKnob);
	knob->forget ();
	bgKnob->forget ();


	//--CMovieButton--------------------------------------
 	size (0, 0, onOffButton->getWidth (), onOffButton->getHeight () / 2);
	size.offset (210 + 20, 20);
	point (0, 0);
	cMovieButton = new CMovieButton (size, this, kMovieButtonTag, onOffButton->getHeight () / 2, onOffButton, point);
	addView (cMovieButton);

	onOffButton->forget ();


	//--CAnimKnob--------------------------------------
	CBitmap *movieKnobBitmap = new CBitmap (kMovieKnobBitmap);

	size (0, 0, movieKnobBitmap->getWidth (), movieKnobBitmap->getHeight () / 7);
	size.offset (280 + 15, 15);
	point (0, 0);
	cAnimKnob = new CAnimKnob (size, this, kAnimKnobTag, 7, movieKnobBitmap->getHeight () / 7, movieKnobBitmap, point);
	addView (cAnimKnob);
	
	movieKnobBitmap->forget ();


	//--COptionMenu--------------------------------------
	size (0, 0, 50, 14);
	size.offset (350 + 10, 30);

	long style = k3DIn | kMultipleCheckStyle;
	cOptionMenu = new COptionMenu (size, this, kOptionMenuTag, bgKnob, 0, style);
	if (cOptionMenu)
	{
		cOptionMenu->setFont (kNormalFont);
		cOptionMenu->setFontColor (kWhiteCColor);
		cOptionMenu->setBackColor (kRedCColor);
		cOptionMenu->setFrameColor (kWhiteCColor);
		cOptionMenu->setHoriAlign (kLeftText);
		int i;
		for (i = 0; i < 3; i++)
		{
			char txt[256];
			sprintf (txt, "Entry %d", i);
			cOptionMenu->addEntry (txt);
		}
		cOptionMenu->addEntry ("-");
		for (i = 3; i < 60; i++)
		{
			char txt[256];
			sprintf (txt, "Entry %d", i);
			cOptionMenu->addEntry (txt);
		}

		addView (cOptionMenu);
	}


	//--CRockerSwitch--------------------------------------
	CBitmap *rocker = new CBitmap (kRockerBitmap);
 	size (0, 0, rocker->getWidth (), rocker->getHeight () / 3);
	size.offset (9, 70 + 29);
	point (0, 0);
	cRockerSwitch = new CRockerSwitch (size, this, kRockerSwitchTag, rocker->getHeight () / 3, rocker, point);
	addView (cRockerSwitch);
	rocker->forget ();


	//--CHorizontalSwitch--------------------------------------
	CBitmap *switchHBitmap = new CBitmap (kSwitchHBitmap);
	size (0, 0, switchHBitmap->getWidth (), switchHBitmap->getHeight () / 4);
	size.offset (70 + 10, 70 + 30);
	point (0, 0);
	cHorizontalSwitch = new CHorizontalSwitch (size, this, kSwitchHTag, 4, switchHBitmap->getHeight () / 4, 4, switchHBitmap, point);
	addView (cHorizontalSwitch);
	switchHBitmap->forget ();


	//--CVerticalSwitch--------------------------------------
	CBitmap *switchVBitmap = new CBitmap (kSwitchVBitmap);

	size (0, 0, switchVBitmap->getWidth (), switchVBitmap->getHeight () / 4);
	size.offset (140 + 30, 70 + 5);
	cVerticalSwitch = new CVerticalSwitch (size, this, kSwitchVTag, 4, switchVBitmap->getHeight () / 4, 4, switchVBitmap, point);
	addView (cVerticalSwitch);
	switchVBitmap->forget ();


	//--CHorizontalSlider--------------------------------------
	CBitmap *sliderHBgBitmap = new CBitmap (kSliderHBgBitmap);
	CBitmap *sliderHandleBitmap = new CBitmap (kSliderHandleBitmap);

	size (0, 0, sliderHBgBitmap->getWidth (), sliderHBgBitmap->getHeight ());
	size.offset (10, 30);

	point (0, 0);
#if 1
	cHorizontalSlider = new CHorizontalSlider (size, this, kSliderHTag, size.left + 2, size.left + sliderHBgBitmap->getWidth () - sliderHandleBitmap->getWidth () - 1, sliderHandleBitmap, sliderHBgBitmap, point, kLeft);
	point (0, 2);
	cHorizontalSlider->setOffsetHandle (point);
#else
	CPoint handleOffset (2, 2);
	cHorizontalSlider = new CHorizontalSlider (size, this, kSliderHTag, handleOffset, size.width () - 2 * handleOffset.h, sliderHandleBitmap, sliderHBgBitmap, point, kLeft);
#endif
	cHorizontalSlider->setFreeClick (false);
	size.offset (0, -30 + 10);

	style =  k3DIn | kCheckStyle;
	COptionMenu *cOptionMenu2 = new COptionMenu (size, this, kOptionMenuTag, bgKnob, 0, style);
	if (cOptionMenu2)
	{
		cOptionMenu2->setFont (kNormalFont);
		cOptionMenu2->setFontColor (kWhiteCColor);
		cOptionMenu2->setBackColor (kRedCColor);
		cOptionMenu2->setFrameColor (kWhiteCColor);
		cOptionMenu2->setHoriAlign (kLeftText);
		int i;
		for (i = 0; i < 3; i++)
		{
			char txt[256];
			sprintf (txt, "Entry %d", i);
			cOptionMenu2->addEntry (txt);
		}
	}

	// add this 2 control in a CViewContainer
	size (0, 0, 70, 45);
	size.offset (210, 70);
	cViewContainer = new CViewContainer (size, frame, background);
	cViewContainer->addView (cHorizontalSlider);
	cViewContainer->addView (cOptionMenu2);
	addView (cViewContainer);

	sliderHBgBitmap->forget ();


	//--CVerticalSlider--------------------------------------
	CBitmap *sliderVBgBitmap = new CBitmap (kSliderVBgBitmap);

	size (0, 0, sliderVBgBitmap->getWidth (), sliderVBgBitmap->getHeight ());
	size.offset (280 + 30, 70 + 5);
#if 1
	point (0, 0);
	cVerticalSlider = new CVerticalSlider (size, this, kSliderVTag, size.top + 2, size.top + sliderVBgBitmap->getHeight () - sliderHandleBitmap->getHeight () - 1, sliderHandleBitmap, sliderVBgBitmap, point, kBottom);
	point (2, 0);
	cVerticalSlider->setOffsetHandle (point);
#else
	point (0, 0);
	CPoint handleOffset (2, 2);
	cVerticalSlider = new CVerticalSlider (size, this, kSliderVTag, handleOffset, 
		size.height () - 2 * handleOffset.v, sliderHandleBitmap, sliderVBgBitmap, point, kBottom);
#endif
	cVerticalSlider->setFreeClick (false);
	addView (cVerticalSlider);

	sliderVBgBitmap->forget ();
	sliderHandleBitmap->forget ();


	//--CTextEdit--------------------------------------
	size (0, 0, 50, 12);
	size.offset (350 + 10, 70 + 30);
	cTextEdit = new CTextEdit (size, this, kTextEditTag, 0, 0, k3DIn);
	if (cTextEdit)
	{
		cTextEdit->setFont (kNormalFontVerySmall);
		cTextEdit->setFontColor (kWhiteCColor);
		cTextEdit->setBackColor (kBlackCColor);
		cTextEdit->setFrameColor (kWhiteCColor);
		cTextEdit->setHoriAlign (kCenterText);
		addView (cTextEdit);
	}

	//--CSplashScreen--------------------------------------
	CBitmap *splashBitmap = new CBitmap (kSplashBitmap);

	size (0, 0, 70, 70);
	size.offset (0, 140);
	point (0, 0);
	CRect toDisplay (0, 0, splashBitmap->getWidth (), splashBitmap->getHeight ());
	toDisplay.offset (100, 50);

	cSplashScreen = new CSplashScreen (size, this, kAbout, splashBitmap, toDisplay, point);
	addView (cSplashScreen);
	splashBitmap->forget ();


	//--CMovieBitmap--------------------------------------
  	CBitmap *movieBitmap = new CBitmap (kMovieBitmap);

	size (0, 0, movieBitmap->getWidth (), movieBitmap->getHeight () / 10);
	size.offset (70 + 15, 140 + 15);
	point (0, 0);	
	cMovieBitmap = new CMovieBitmap (size, this, kMovieBitmapTag, 10, movieBitmap->getHeight () / 10, movieBitmap, point);
	addView (cMovieBitmap);


	//--CAutoAnimation--------------------------------------
	size (0, 0, movieBitmap->getWidth (), movieBitmap->getHeight () / 10);
	size.offset (140 + 15, 140 + 15);
	point (0, 0);
	cAutoAnimation = new CAutoAnimation (size, this, kAutoAnimationTag, 10, movieBitmap->getHeight () / 10, movieBitmap, point);
	addView (cAutoAnimation);
	movieBitmap->forget ();


	//--CSpecialDigit--------------------------------------
	CBitmap *specialDigitBitmap = new CBitmap (kDigitBitmap);

 	size (0, 0, specialDigitBitmap->getWidth () * 7, specialDigitBitmap->getHeight () / 10);
	size.offset (210 + 10, 140 + 30);

	cSpecialDigit = new CSpecialDigit (size, this, kDigitTag, 0, 7, 0, 0, specialDigitBitmap->getWidth (), specialDigitBitmap->getHeight () / 10 , specialDigitBitmap);
	addView (cSpecialDigit);
	specialDigitBitmap->forget ();


	//--CParamDisplay--------------------------------------
	size (0, 0, 50, 15);
	size.offset (280 + 10, 140 + 30);
	cParamDisplay = new CParamDisplay (size);
	if (cParamDisplay)
	{
		cParamDisplay->setFont (kNormalFontSmall);
		cParamDisplay->setFontColor (kWhiteCColor);
		cParamDisplay->setBackColor (kBlackCColor);
		addView (cParamDisplay);
	}


	//--CVuMeter--------------------------------------
	CBitmap* vuOnBitmap  = new CBitmap (kVuOnBitmap);
	CBitmap* vuOffBitmap = new CBitmap (kVuOffBitmap);

	size (0, 0, vuOnBitmap->getWidth (), vuOnBitmap->getHeight ());
	size.offset (350 + 30, 140 + 5);
	cVuMeter = new CVuMeter (size, vuOnBitmap, vuOffBitmap, 14);
	cVuMeter->setDecreaseStepValue (0.1f);
	addView (cVuMeter);
	vuOnBitmap->forget ();
	vuOffBitmap->forget ();

	//--My controls---------------------------------
	//--CLabel--------------------------------------
	size (0, 0, 349, 14);
	size.offset (0, 140);
	cLabel = new CLabel (size, "Type a Key or Drop a file...");
	if (cLabel)
	{
		cLabel->setFont (kNormalFontSmall);
		cLabel->setFontColor (kWhiteCColor);
		cLabel->setBackColor (kGreyCColor);
		addView (cLabel);
	}

	//--CLabel--------------------------------------
	size (0, 0, 65, 12);
	size.offset (1, 40);
	CLabel *cLabel2 = new CLabel (size, "FileSelector");
	if (cLabel2)
	{
		cLabel2->setFont (kNormalFontSmaller);
		cLabel2->setFontColor (kWhiteCColor);
		cLabel2->setBackColor (kGreyCColor);
		addView (cLabel2);
	}
	
	size (inSize.right, inSize.bottom, inSize.right + 100, inSize.bottom + 100);
	CLabel* outsideLabel = new CLabel (size, "This label is outside its superview");
	addView (outsideLabel);
	outsideLabel->setDirty (true);
}

void ControlsGUI::valueChanged (CDrawContext *pContext, CControl *pControl)
{
	// this is only to provide the same behaviour as the original controlsgui editor class in the vst sdk 2.3
	// do not take this as an example on how to code !!!

	float value = pControl->getValue ();
	switch (pControl->getTag ())
	{
		case kSliderVTag:
		case kSliderHTag:
		case kKnobTag:
		case kAnimKnobTag:
		{
			cHorizontalSlider->setValue (value);
			cVerticalSlider->setValue (value);
			cKnob->setValue (value);
			cAnimKnob->setValue (value);
			cSpecialDigit->setValue (1000000 * value);
			cParamDisplay->setValue (value);
			cVuMeter->setValue (value);
			cMovieBitmap->setValue (value);
			break;
		}

		case kOnOffTag:
		{
			if (value > 0.5f)
			{
				VstFileType aiffType ("AIFF File", "AIFF", "aif", "aiff", "audio/aiff", "audio/x-aiff");
				VstFileType aifcType ("AIFC File", "AIFC", "aif", "aifc", "audio/x-aifc");
				VstFileType waveType ("Wave File", "WAVE", "wav", "wav",  "audio/wav", "audio/x-wav");
				VstFileType sdIIType ("SoundDesigner II File", "Sd2f", "sd2", "sd2");
				VstFileType types[] = {aiffType, aifcType, waveType, sdIIType};

				VstFileSelect vstFileSelect;
				memset (&vstFileSelect, 0, sizeof (VstFileSelect));

				vstFileSelect.command     = kVstFileLoad;
				vstFileSelect.type        = kVstFileType;
				strcpy (vstFileSelect.title, "Test for open file selector");
				vstFileSelect.nbFileTypes = 4;
				vstFileSelect.fileTypes   = (VstFileType*)&types;
				vstFileSelect.returnPath  = new char[1024];
				vstFileSelect.initialPath = 0;
				vstFileSelect.future[0] = 1;	// utf-8 path on macosx
				CFileSelector selector (NULL);
				if (selector.run (&vstFileSelect))
				{
					if (cLabel)
						cLabel->setLabel (vstFileSelect.returnPath);
				}
				else
				{
					if (cLabel)
						cLabel->setLabel ("OpenFileSelector: canceled!!!!");
				}
				delete []vstFileSelect.returnPath;
				if (vstFileSelect.initialPath)
					delete []vstFileSelect.initialPath;
			}
			break;
		}
	}
}

void ControlsGUI::onIdle ()
{
	// trigger animation
	if(cAutoAnimation)
		cAutoAnimation->nextPixmap ();
}