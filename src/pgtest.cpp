#include "paragui/pgmultilineedit.h"
#include "paragui/pglistbox.h"
#include "paragui/pglistboxitem.h"
#include "gfx.h"
#include "State.h"
#include "common.h"


SDL_Surface *g_poBackground = NULL;

class CMortalNetUI: public SigC::Object
{
	PG_MultiLineEdit *m_poConsole;
	PG_LineEdit *m_poInputLine;
	PG_Button *m_poChallengeButton;
	PG_ListBox *m_poPeople;

	PG_FontEngine m_oFontEngine;
	PG_Font m_oFont;

public:
	CMortalNetUI();
	~CMortalNetUI();

	void Init();
	void Run();
	bool OnChallenge( PG_Button *a_poSource );
};


CMortalNetUI::CMortalNetUI() :
	m_oFont(OM_DATADIR "/fonts/bradybun.ttf", 24)
{
	m_poConsole= NULL;
	m_poInputLine= NULL;
	m_poChallengeButton= NULL;
	m_poPeople= NULL;
}

CMortalNetUI::~CMortalNetUI()
{
	delete m_poConsole; m_poConsole= NULL;
	delete m_poInputLine; m_poInputLine= NULL;
	delete m_poChallengeButton; m_poChallengeButton= NULL;
	delete m_poPeople; m_poPeople= NULL;
	SDL_FreeSurface( g_poBackground );
}

void CMortalNetUI::Init()
{
	SDL_EnableUNICODE(1);
	
	g_poBackground= LoadBackground("FighterStats.jpg", 64);
	SDL_BlitSurface(g_poBackground, NULL, gamescreen, NULL);
	SDL_Flip(gamescreen);

	int i;

	// LAYOUT VARIABLES

	int FontHeight= m_oFont.GetFontHeight();
	int LayoutSpacing= 10;
	int LayoutMargin= 15;
	int HorizontalDivide= gamescreen->w * 3 / 4;
	int DivideWidth= gamescreen->w - HorizontalDivide - LayoutSpacing - LayoutMargin;

	// LINEEDIT

	int EditHeight= FontHeight;
	int EditTop= gamescreen->h - LayoutMargin - EditHeight;
	PG_Rect r( LayoutMargin, EditTop,
		gamescreen->w - 2*LayoutMargin, EditHeight );
	m_poInputLine = new PG_LineEdit(NULL, r);
	m_poInputLine->SetText("Ez itt, KÉREM az input sor");
	m_poInputLine->SetEditable(true);
	m_poInputLine->Show();

	// BUTTONS

	int ButtonTop= EditTop - FontHeight - 4 - LayoutSpacing;
	r.SetRect( HorizontalDivide + LayoutSpacing, ButtonTop,
		DivideWidth, FontHeight + 4 );
	m_poChallengeButton = new PG_Button( NULL, r );
	m_poChallengeButton->SetText("Challenge!");
	m_poChallengeButton->Show();
	m_poChallengeButton->sigClick.connect(slot(*this, &CMortalNetUI::OnChallenge));

//	my_vscroll->sigScrollPos.connect(slot(*this, &PG_MultiLineEdit::handleScroll));

	// LISTBOX

	r.SetRect( HorizontalDivide + LayoutMargin, LayoutMargin,
		DivideWidth, ButtonTop - LayoutMargin - LayoutSpacing );
	m_poPeople = new PG_ListBox( NULL, r );
	m_poPeople->Show();

	new PG_ListBoxItem( m_poPeople, FontHeight, "Egy vizilo" );
	new PG_ListBoxItem( m_poPeople, FontHeight, "Nagyon hosszu valami" );
	for ( i= 0; i<30; ++i ) 
	{
		char Buffer[128];
		sprintf( Buffer, "%d víziló!", i);
		new PG_ListBoxItem( m_poPeople, FontHeight, Buffer );
	}
	
	// MULTILINEEDIT
	
	int MultiLineEditHeight= gamescreen->h - 3*LayoutMargin - EditHeight;
	MultiLineEditHeight= (MultiLineEditHeight) / FontHeight * FontHeight;
	r.SetRect( LayoutMargin, LayoutMargin,
		HorizontalDivide - LayoutMargin, MultiLineEditHeight );
	m_poConsole = new PG_MultiLineEdit(NULL, r);
	m_poConsole->SetText("Welcome to MortalNet!");
	m_poConsole->SetEditable(true);
	m_poConsole->Show();
}

bool CMortalNetUI::OnChallenge( PG_Button *a_poSource )
{
	PG_ListBoxBaseItem *Item = m_poPeople->GetCurrentItem();
	if (NULL == Item)
	{
		m_poConsole->AddText("Fogalmam sincs, hogy mit akarsz", C_LIGHTRED, true);
	}
	else
	{
		std::string Message("Van egy ötletem. Mi lenne, ha kihívnánk ");
		Message += Item->GetText();
		Message += "-t?";

		int Index = m_poPeople->FindIndex(Item);

		PG_Color Color( 128, (Index % 2) ? 128 : 192, 255 );

		m_poConsole->AddText(Message.c_str(), Color, true);
	}
	
	return false;
}

void CMortalNetUI::Run()
{
	SDL_Event Event;

	while (SDL_WaitEvent(&Event))
	{
		if (Event.type== SDL_QUIT) break;
		if (Event.type == SDL_KEYDOWN && Event.key.keysym.sym == SDLK_ESCAPE) break;
		PG_MessageObject::HandleEvent(&Event);
	}
}


void PgTest()
{
	CMortalNetUI oUi;
	oUi.Init();
	oUi.Run();
}
