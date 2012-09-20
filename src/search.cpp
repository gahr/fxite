/*
  FXiTe - The Free eXtensIble Text Editor
  Copyright (c) 2009-2011 Jeffrey Pohlmeyer <yetanothergeek@gmail.com>

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License version 3 as
  published by the Free Software Foundation.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <fx.h>
#include <fxkeys.h>

#include "compat.h"
#include "scidoc.h"
#include "scisrch.h"
#include "histbox.h"

#include "appwin_pub.h"

#include "intl.h"
#include "search.h"



#define SetPadLRTB(o,l,r,t,b) \
  o->setPadLeft(l); \
  o->setPadRight(r); \
  o->setPadTop(t); \
  o->setPadBottom(b);

#define SetPad(o,p) SetPadLRTB(o,p,p,p,p)



FXDEFMAP(SciSearchOptions) SciSearchOptionsMap[]={
  FXMAPFUNC(SEL_COMMAND,SciSearchOptions::ID_TOGGLE_CHK,SciSearchOptions::onToggleChk),
};

FXIMPLEMENT(SciSearchOptions,FXHorizontalFrame,SciSearchOptionsMap,ARRAYNUMBER(SciSearchOptionsMap))


enum {
  MODELIST_NORMAL,
  MODELIST_REGEXP,
  MODELIST_WHOLEWORD
};

void SciSearchOptions::SetSciFlags(FXuint n)
{
  // Favor REGEXP over WHOLEWORD
  if ( (n&SCFIND_WHOLEWORD) && (n&SCFIND_REGEXP) ) { n&=!SCFIND_WHOLEWORD; }
  matchcase->setCheck((n & SCFIND_MATCHCASE)?1:0);
  if (n & SCFIND_WHOLEWORD) {
    modelist->setCurrentItem(MODELIST_WHOLEWORD);
  } else if (n & SCFIND_REGEXP) {
    modelist->setCurrentItem(MODELIST_REGEXP);
  } else {
    modelist->setCurrentItem(MODELIST_NORMAL);
  }
}



long SciSearchOptions::onToggleChk(FXObject*o, FXSelector sel, void*p)
{
  FXuval n=(FXuval)modelist->getItemData(modelist->getCurrentItem());
  if ((o==modelist) && (modelist->getCurrentItem()==MODELIST_REGEXP)) { matchcase->setCheck(true); }
  if (matchcase->getCheck()) { n|=SCFIND_MATCHCASE; }
  if (target) { target->handle(this, FXSEL(SEL_COMMAND,message), (void*)(FXuval)n); }
  return 1;
}



SciSearchOptions::SciSearchOptions(FXComposite *p, FXObject *tgt, FXSelector sel):FXHorizontalFrame(p, 0)
{
  target=tgt;
  message=sel;
  new FXLabel(this,_(" M&ode:"));
  modelist=new FXListBox(this,this,ID_TOGGLE_CHK);
  modelist->appendItem(FXString::null);
  modelist->appendItem(FXString::null);
  modelist->appendItem(FXString::null);
  modelist->setNumVisible(modelist->getNumItems());
  modelist->setItem(MODELIST_NORMAL,    _("normal"),     NULL, (void*)(FXuval)0);
  modelist->setItem(MODELIST_REGEXP,    _("reg. expr."), NULL, (void*)(FXuval)SCFIND_REGEXP);
  modelist->setItem(MODELIST_WHOLEWORD, _("whole word"), NULL, (void*)(FXuval)SCFIND_WHOLEWORD);
  matchcase = new FXCheckButton(this, _("&Match case "), this, ID_TOGGLE_CHK);
}



class SciReplGui: public FXObject {
FXDECLARE(SciReplGui)
SciReplGui(){}
private:
  FXVerticalFrame *txt_fields;
  FXVerticalFrame *btn_ctrls;
  FXLabel*srch_lab;
  HistoryTextField*srch_hist;
  FXLabel*repl_lab;
  HistoryTextField*repl_hist;
  SciSearchOptions*opts;
  FXCheckButton* rev_chk;
  FXuint sciflags;
  FXHorizontalFrame *btns;
  FXButton*fwd_btn;
  FXButton*bwd_btn;
  FXButton*cncl_btn;
  FXButton*repall_indoc_btn;
  FXButton*repall_insel_btn;
  FXButton*repl_once_btn;
  FXObject*target;
  FXSelector message;
  void EnableSearch();
public:
  SciReplGui(FXComposite*p, FXObject*tgt=NULL, FXSelector sel=0, bool find_only=false);
  void stop(FXuint stopval);
  long onSciOpts(FXObject*o, FXSelector sel, void*p);
  long onSrchHist(FXObject*o, FXSelector sel, void*p);
  long onButton(FXObject*o, FXSelector sel, void*p);
  long onFakeBtnUp(FXObject*o, FXSelector sel, void*p);
  void setSearchText(const FXString& text);
  FXString getSearchText() const;
  void setReplaceText(const FXString& text);
  FXString getReplaceText() const;
  void setHaveSelection(bool have_sel);
  FXuint getSearchMode() { return sciflags; }
  void setSearchMode(FXuint mode);
  FXuint getSearchReverse() { return rev_chk->getCheck(); }
  void setSearchReverse(bool reverse) { rev_chk->setCheck(reverse); }
  void AppendHist(const FXString& search,const FXString& replace,FXuint mode);
  void DoExecute(bool first_time);
  void HandleKeyPress(FXint code);
  virtual void create();
  virtual void destroy();
  FXVerticalFrame *TextFrame() { return txt_fields; }
  FXVerticalFrame *ButtonFrame() {return btn_ctrls; }
  enum {
    ID_SCI_OPTS=1,
    ID_SRCH_HIST,
    ID_REPL_HIST,
    ID_PREV,
    ID_NEXT,
    ID_REPLACE,
    ID_ALL_INDOC,
    ID_ALL_INSEL,
    ID_CANCEL_SRCH,
    ID_REV_CHK,
    ID_FAKE_BTN_UP,
    ID_LAST
  };
  enum {
    DONE           = 0,  // Cancel search
    SEARCH         = 1,  // Search first occurrence
    SEARCH_NEXT    = 2,  // Search next occurrence
    REPL_ALL_INDOC = 3,  // Replace all occurrences in document
    REPL_ALL_INSEL = 4   // Replace all occurrences in selection
  };
};



FXDEFMAP(SciReplGui) SciReplGuiMap[]={
  FXMAPFUNC(  SEL_COMMAND,  SciReplGui::ID_SCI_OPTS,    SciReplGui::onSciOpts),
  FXMAPFUNCS( SEL_COMMAND,  SciReplGui::ID_SRCH_HIST,   SciReplGui::ID_REPL_HIST,   SciReplGui::onSrchHist),
  FXMAPFUNC(  SEL_CHANGED,  SciReplGui::ID_SRCH_HIST,   SciReplGui::onSrchHist),
  FXMAPFUNC(  SEL_PICKED,   SciReplGui::ID_SRCH_HIST,   SciReplGui::onSrchHist),
  FXMAPFUNCS( SEL_COMMAND,  SciReplGui::ID_PREV,        SciReplGui::ID_CANCEL_SRCH, SciReplGui::onButton),
  FXMAPFUNC(  SEL_TIMEOUT,  SciReplGui::ID_FAKE_BTN_UP, SciReplGui::onFakeBtnUp),
};

FXIMPLEMENT(SciReplGui,FXObject,SciReplGuiMap,ARRAYNUMBER(SciReplGuiMap))



#define ICO_SIZE 8

static const char close_icon[] =
  "X_____X_"
  "_X___X__"
  "__X_X___"
  "___X____"
  "__X_X___"
  "_X___X__"
  "X_____X_"
  "________"
;



SciReplGui::SciReplGui(FXComposite*p, FXObject*tgt, FXSelector sel, bool find_only)
{
  sciflags=0;
  target=tgt;
  message=sel;
  FXuint textopts=TEXTFIELD_ENTER_ONLY|FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_X;
  const char*group=find_only?"Search":"Replace";

  txt_fields=new FXVerticalFrame(p,FRAME_NONE|LAYOUT_FILL_X);
  SetPad(txt_fields,0);

  srch_lab=new FXLabel(txt_fields, _("Se&arch for:"));
  srch_hist=new HistoryTextField(txt_fields,48,group,"SM",this,ID_SRCH_HIST,textopts);
  repl_lab=new FXLabel(txt_fields, _("Replace &with:"));
  repl_hist=new HistoryTextField(txt_fields,48,group,"R",this,ID_REPL_HIST,textopts);

  btn_ctrls=new FXVerticalFrame(p,FRAME_NONE);
  SetPad(btn_ctrls,0);

  opts=new SciSearchOptions(btn_ctrls,this,ID_SCI_OPTS);
  rev_chk=new FXCheckButton(opts, _("&Backward"),this,ID_REV_CHK);
  srch_hist->setWatch(&sciflags);

  btns=new FXHorizontalFrame(btn_ctrls,LAYOUT_FILL_X);
  FXHorizontalFrame*nav_btns=new FXHorizontalFrame(btns);
  SetPadLRTB(nav_btns,DEFAULT_SPACING,DEFAULT_SPACING*2,0,0);
  bwd_btn=new FXButton(nav_btns," &< ",NULL,this,ID_PREV);
  fwd_btn=new FXButton(nav_btns," &> ",NULL,this,ID_NEXT);
  if (find_only) {
    repl_once_btn=new FXButton(btns,_("F&ind"),NULL,this,ID_REPLACE);
  } else {
    repl_once_btn=new FXButton(btns,_("&Replace"),NULL,this,ID_REPLACE);
  }
  repall_indoc_btn=new FXButton(btns,_("Re&place All"),NULL,this,ID_ALL_INDOC);
  repall_insel_btn=new FXButton(btns,_("All in Sele&cted"),NULL,this,ID_ALL_INSEL);

  FXVerticalFrame*cncl_box=new FXVerticalFrame(btns,LAYOUT_RIGHT|LAYOUT_SIDE_BOTTOM|PACK_UNIFORM_HEIGHT|LAYOUT_FILL_Y);
  cncl_box->setVSpacing(0);
  SetPad(cncl_box,0);
  cncl_btn=new FXButton(cncl_box,FXString::null,NULL,this,ID_CANCEL_SRCH,BUTTON_NORMAL|LAYOUT_BOTTOM);
  if (!find_only) {
    new FXVerticalFrame(cncl_box,FRAME_NONE);
    FXHorizontalFrame*cncl_frm=new FXHorizontalFrame(btn_ctrls,LAYOUT_FILL_X|LAYOUT_FILL_Y|LAYOUT_SIDE_BOTTOM);
    SetPad(cncl_frm,0);
    cncl_box->reparent(cncl_frm);
  }
  cncl_btn->setTipText(_("Close   {ESC}"));
  SetPad(btns,0);

  FXColor ico_buf[ICO_SIZE*ICO_SIZE];
  FXColor bg=cncl_btn->getBackColor();
  FXColor fg=cncl_btn->getTextColor();
  for (FXint i=0; i<ICO_SIZE*ICO_SIZE; i++) { ico_buf[i]=close_icon[i]=='_'?bg:fg; }
  FXIcon *ico=new FXIcon(cncl_btn->getApp(),ico_buf,0,IMAGE_OPAQUE,ICO_SIZE,ICO_SIZE);
  ico->create();
  cncl_btn->setIcon(ico);

  if (find_only) {
    repl_lab->hide();
    repl_hist->hide();
    repl_hist->disable();
    repall_indoc_btn->hide();
    repall_insel_btn->hide();
  } else {
    srch_hist->enslave(repl_hist);
  }
  setSearchText(FXString::null);
  EnableSearch();
}



void SciReplGui::AppendHist(const FXString& search,const FXString& replace,FXuint mode)
{
  FXString tmp;
  FXuint oldmode=sciflags;
  sciflags=mode;
  if (!search.empty()) {
    tmp=getSearchText();
    setSearchText(search);
    srch_hist->append();
    setSearchText(tmp);
  }
  if (!replace.empty()) {
    tmp=getReplaceText();
    setReplaceText(replace);
    repl_hist->append();
    setSearchText(tmp);
  }
  sciflags=oldmode;
}



void SciReplGui::stop(FXuint stopval)
{
  srch_hist->append();
  repl_hist->append();
  if (target) { target->handle(this,FXSEL(SEL_COMMAND,message),(void*)(FXuval)stopval); }
}



long SciReplGui::onFakeBtnUp(FXObject*o, FXSelector sel, void*p)
{
 ((FXButton*)p)->setState(STATE_UP);
  return 0;
}



long SciReplGui::onButton(FXObject*o, FXSelector sel, void*p)
{
  if ( getSearchText().empty() && (FXSELID(sel)!=ID_CANCEL_SRCH) ) { return 0; }
  FXButton*btn=NULL;
  switch (FXSELID(sel)) {
    case ID_PREV:
    {
      rev_chk->setCheck(true);
      btn=bwd_btn;
      stop(SEARCH_NEXT);
      break;
    }
    case ID_NEXT:
    {
      rev_chk->setCheck(false);
      btn=fwd_btn;
      stop(SEARCH_NEXT);
      break;
    }
    case ID_REPLACE: {
      stop(SEARCH);
      break;
    }
    case ID_ALL_INDOC: {
      stop(REPL_ALL_INDOC);
      break;
    }
    case ID_ALL_INSEL: {
      stop(REPL_ALL_INSEL);
      break;
    }
    case ID_CANCEL_SRCH: {
       stop(DONE);
      break;
    }
    default: {return 0; }
  }
  if (btn && !o) { // If sender was NULL, fake the appearance of a button press.
    btn->setFocus();
    btn->setState(STATE_DOWN);
    btn->update();
    btn->getApp()->addTimeout(this,ID_FAKE_BTN_UP,ONE_SECOND/10,(void*)btn);
  }
  return 1;
}



void SciReplGui::HandleKeyPress(FXint code)
{
  switch (code) {
    case KEY_less:
    case KEY_comma: {
      handle(NULL,FXSEL(SEL_COMMAND,SciReplGui::ID_PREV),NULL);
      break;
    }
    case KEY_period:
    case KEY_greater: {
      handle(NULL,FXSEL(SEL_COMMAND,SciReplGui::ID_NEXT),NULL);
      break;
    }
    case KEY_Escape: {
      stop(SciReplGui::DONE);
      break;
    }
  }
}



void SciReplGui::EnableSearch()
{
  if (srch_hist->getText().empty()) {
    bwd_btn->disable();
    fwd_btn->disable();
    repl_once_btn->disable();
    repall_indoc_btn->disable();
    repall_insel_btn->disable();
  } else {
    bwd_btn->enable();
    fwd_btn->enable();
    repl_once_btn->enable();
    repall_indoc_btn->enable();
    repall_insel_btn->enable();
  }
}



long SciReplGui::onSrchHist(FXObject*o, FXSelector sel, void*p)
{
  if (FXSELID(sel)!=ID_SRCH_HIST) { return 0; }
  switch (FXSELTYPE(sel)) {
    case SEL_CHANGED: {
      EnableSearch();
      return 1;
    }
    case SEL_PICKED: {
      sciflags=(FXuint)((FXival)p);
      opts->SetSciFlags(sciflags);
      EnableSearch();
      return 1;
    }
    case SEL_COMMAND: {
      if (!getSearchText().empty()) {
        if (repl_hist->isEnabled()) {
           repl_hist->setFocus();
        } else {
          stop(SEARCH);
        }
      }
      return 1;
    }
    default: return 0;
  }
}



long SciReplGui::onSciOpts(FXObject*o, FXSelector sel, void*p)
{
  sciflags=(FXuint)((FXival)p);
  return 1;
}



void SciReplGui::setSearchText(const FXString& text)
{
  srch_hist->setText(text);
}



FXString SciReplGui::getSearchText() const
{
  return srch_hist->getText();
}



void SciReplGui::setReplaceText(const FXString& text)
{
  repl_hist->setText(text);
}



FXString SciReplGui::getReplaceText() const
{
  return repl_hist->getText();
}



void SciReplGui::setHaveSelection(bool have_sel)
{
  if (have_sel) { repall_insel_btn->enable(); } else { repall_insel_btn->disable(); }
}



void SciReplGui::setSearchMode(FXuint mode)
{
  sciflags=mode;
  opts->SetSciFlags(mode);
}



void SciReplGui::create()
{
  srch_hist->setFocus();
}



void SciReplGui::destroy()
{
  srch_hist->killSelection();
  repl_hist->killSelection();
}



void SciReplGui::DoExecute(bool first_time)
{
  if (first_time) { setSearchText(""); }
  if ( (!bwd_btn->hasFocus())&&!fwd_btn->hasFocus() ) { srch_hist->setFocus(); }
  srch_hist->start();
  repl_hist->start();
}



class SciReplPan: public FXHorizontalFrame {
  FXDECLARE(SciReplPan);
  SciReplPan(){}
  SciReplGui*gui;
public:
  SciReplPan(FXComposite*p, FXObject*tgt=NULL, FXSelector sel=0, bool find_only=false);
  SciReplGui*Gui() { return gui; }
  long onKeyPress(FXObject*o, FXSelector sel, void*p);
  void FillX(bool fill_x);
};



FXDEFMAP(SciReplPan) SciReplPanMap[]={
  FXMAPFUNC(SEL_KEYPRESS, 0, SciReplPan::onKeyPress),
};

FXIMPLEMENT(SciReplPan,FXHorizontalFrame,SciReplPanMap,ARRAYNUMBER(SciReplPanMap))



SciReplPan::SciReplPan(FXComposite*p, FXObject*tgt, FXSelector sel, bool find_only):FXHorizontalFrame(p,FRAME_GROOVE|LAYOUT_FILL_X)
{
  gui=new SciReplGui(this,tgt,sel,find_only);
}



void SciReplPan::FillX(bool fill_x)
{
  gui->TextFrame()->setLayoutHints(fill_x?LAYOUT_FILL_X:0);
  setLayoutHints(fill_x?LAYOUT_FILL_X:0);
}



long SciReplPan::onKeyPress(FXObject*o, FXSelector sel, void*p)
{
  gui->HandleKeyPress(((FXEvent*)p)->code);
  return FXHorizontalFrame::onKeyPress(o,sel,p);
}



FXDEFMAP(SearchDialogs) SearchDialogsMap[] = {
  FXMAPFUNC(SEL_COMMAND, SearchDialogs::ID_SEARCH,    SearchDialogs::onSearch),
  FXMAPFUNC(SEL_CHORE,   SearchDialogs::ID_REPL_DONE, SearchDialogs::onReplDone),
};

FXIMPLEMENT(SearchDialogs, FXObject, SearchDialogsMap, ARRAYNUMBER(SearchDialogsMap));



class MainPanel: public FXHorizontalFrame {
  FXDECLARE(MainPanel)
  MainPanel() {}
public:
  long onConfigure(FXObject*o, FXSelector sel, void*p);
  MainPanel(FXComposite *p):FXHorizontalFrame(p,FRAME_NONE|LAYOUT_FILL_X) {}
};



FXDEFMAP(MainPanel) MainPanelMap[]={
  FXMAPFUNC(SEL_CONFIGURE, 0, MainPanel::onConfigure)
};

FXIMPLEMENT(MainPanel,FXHorizontalFrame,MainPanelMap,ARRAYNUMBER(MainPanelMap));



long MainPanel::onConfigure(FXObject*o, FXSelector sel, void*p)
{
  for (FXWindow*w=getFirst(); w; w=w->getNext()) {
    ((SciReplPan*)w)->FillX( w->getDefaultWidth() > getWidth() );
  }
  return FXHorizontalFrame::onConfigure(o,sel,p);
}



SearchDialogs::SearchDialogs(FXComposite*p, FXObject*trg, FXSelector sel) {
  defaultsearchmode=0;
  searchdirn=SCI_SEARCHNEXT;
  searchstring=FXString::null;
  target=trg;
  message=sel;
  parent=p;
  find_initial=true;
  repl_initial=true;
  srchpan=new MainPanel(parent);
  SetPad(srchpan,0);
  find_dlg=new SciReplPan(srchpan, this, ID_SEARCH,true);
  find_dlg->hide();
  repl_dlg=NULL;
}



SearchDialogs::~SearchDialogs()
{
  delete find_dlg;
  find_dlg=NULL;
}



void SearchDialogs::SetPrefs(FXuint mode, FXuint wrap, bool verbose)
{
  searchmode=mode;
  defaultsearchmode=mode;
  searchwrap=(SearchWrapPolicy)wrap;
  searchverbose=verbose;
}



bool SearchDialogs::SearchFailed()
{
  if (searchverbose) {
    FXMessageBox::error(parent->getShell(), MBOX_OK, _("Not found"), "%s.", _("Search term not found"));
  }
  return false;
}



bool SearchDialogs::FindText(bool forward, bool wrap)
{
  SciDoc*sci=TopWinPub::FocusedDoc();
  if (sci->search->FindText(searchstring,searchmode,forward,wrap)) {
    if (target) { target->handle(this,FXSEL(SEL_COMMAND,message),NULL); }
    return true;
  } else {
    return false;
  }
}



bool SearchDialogs::DoFind(bool forward)
{
  SciDoc*sci=TopWinPub::FocusedDoc();
  searchdirn=forward?SCI_SEARCHNEXT:SCI_SEARCHPREV; // <= this is only used for the macro recorder
  switch (searchwrap) {
    case SEARCH_WRAP_NEVER: {
      return FindText(forward,false)?true:SearchFailed();
    }
    case SEARCH_WRAP_ALWAYS: {
      return FindText(forward,true)?true:SearchFailed();
    }
    case SEARCH_WRAP_ASK: {
      if (FindText(forward,false)) { return true; }
      long pos=sci->sendMessage(SCI_GETCURRENTPOS,0,0);
      if (forward) {
        if (pos==0) { return SearchFailed(); }
      } else {
        if (pos==sci->sendMessage(SCI_GETLENGTH,0,0)) { return SearchFailed(); }
      }

      if (FXMessageBox::question(sci->getShell(), MBOX_YES_NO, _("Wrap search?"), "%s:\n%s",
            _("Search term not found"),
            _("Wrap search and try again?"))==MBOX_CLICKED_YES) {
        return (FindText(forward,true))?true:SearchFailed();
      }
    }
    default: { return false; }
  }
}



void SearchDialogs::FindNext()
{
  searchmode &= ~SEARCH_BACKWARD;
  if (searchstring.empty()) {
    ShowFindDialog();
  } else {
    DoFind(true);
  }
}



void SearchDialogs::FindPrev()
{
  searchmode |= SEARCH_BACKWARD;
  if (searchstring.empty()) {
    ShowFindDialog();
  } else {
    DoFind(false);
  }
}



void SearchDialogs::FindPhrase(const char* phrase, FXuint mode, bool forward)
{
  searchstring=phrase;
  searchmode=mode;
  if (forward) { FindNext(); } else { FindPrev(); }
}



bool GetPrimarySelection(SciDoc*sci, FXString&target)
{
  return sci->getShell()->getDNDData(FROM_SELECTION,FXWindow::stringType, target);
}



void SearchDialogs::FindSelected(bool forward)
{
  SciDoc*sci=TopWinPub::FocusedDoc();
  FXString srch=FXString::null;
  if (!GetPrimarySelection(sci,srch)) {
    if (sci->GetSelLength()>0) { sci->GetSelText(srch); }
  }
  if (!srch.empty()) {
    searchmode=defaultsearchmode;
    searchstring=srch;
    DoFind(forward);
    find_dlg->Gui()->AppendHist(searchstring,"",defaultsearchmode);
  }
}



bool SearchDialogs::GoToSelected()
{
  SciDoc*sci=TopWinPub::FocusedDoc();
  FXString tmp;
  if (sci && GetPrimarySelection(sci,tmp)) {
    tmp.trim();
    return sci->GoToStringCoords(tmp.text());
  } else {
    return false;
  }
}



bool SearchDialogs::ShowGoToDialog()
{
  SciDoc*sci=TopWinPub::FocusedDoc();
  FXInputDialog dlg(sci->getShell(),_("Go to line"), _("Enter line number\n(or line:column)"));
  bool rv=dlg.execute(PLACEMENT_OWNER);
  if (rv) { sci->GoToStringCoords(dlg.getText().text()); }
  sci->setFocus();
  return rv;
}



void SearchDialogs::ShowFindDialog()
{
  if (repl_dlg) {
    delete repl_dlg;
    repl_dlg=NULL;
  }
  find_dlg->create();
  find_dlg->show();
  parent->layout();
  find_dlg->setFocus();
  find_dlg->Gui()->setSearchMode(searchmode);
  find_dlg->Gui()->DoExecute(find_initial);
}



FXuint SearchDialogs::NextSearch(FXuint code)
{
  searchstring=find_dlg->Gui()->getSearchText();
  searchmode=find_dlg->Gui()->getSearchMode();
  if (code!=SciReplGui::DONE) {
    find_initial=false;
    switch(code) {
      case SciReplGui::DONE:{
        break;
      }
      case SciReplGui::SEARCH:
      case SciReplGui::SEARCH_NEXT: {
        DoFind(!find_dlg->Gui()->getSearchReverse());
        break;
      }
    }
  }
  TopWinPub::FocusedDoc()->update();
  return code;
}



void SearchDialogs::ShowReplaceDialog()
{
  if (!repl_dlg) {
    repl_dlg=new SciReplPan(srchpan, this, ID_SEARCH, false);
    repl_dlg->create();
  }
  find_dlg->hide();
  repl_dlg->Gui()->setSearchMode(searchmode);
  repl_ready=false;
  repl_initial=false;
  repl_dlg->Gui()->setHaveSelection(TopWinPub::FocusedDoc()->GetSelLength()>0);
  repl_dlg->Gui()->DoExecute(repl_initial?PLACEMENT_OWNER:PLACEMENT_DEFAULT);
}



FXuint SearchDialogs::NextReplace(FXuint code)
{
  SciDoc*sci=TopWinPub::FocusedDoc();
  FXString replacestring;
  searchmode=repl_dlg->Gui()->getSearchMode();
  searchstring=repl_dlg->Gui()->getSearchText();
  replacestring=repl_dlg->Gui()->getReplaceText();
  switch (code) {
    case SciReplGui::SEARCH:{ // Replace selection
      if (!repl_ready) { repl_ready=DoFind(!repl_dlg->Gui()->getSearchReverse()); }
      if (repl_ready) {
        sci->search->ReplaceSelection(replacestring,searchmode);
        repl_ready=false;
      }
      break;
    }
    case SciReplGui::SEARCH_NEXT:{ // Find next
      repl_ready=DoFind(!repl_dlg->Gui()->getSearchReverse());
      break;
    }
    case SciReplGui::REPL_ALL_INDOC:{ // Replace all
      if (sci->search->ReplaceAllInDoc(searchstring, replacestring, searchmode)==0) {
        SearchFailed();
      }
      break;
    }
    case SciReplGui::REPL_ALL_INSEL: { // Replace all in selection
      if (sci->search->ReplaceAllInSel(searchstring,replacestring,searchmode)==0) {
        SearchFailed();
      }
      break;
    }
    default:{ // Cancel
      break;
    }
  }
  return code;
}



long SearchDialogs::onReplDone(FXObject*o, FXSelector sel, void *p)
{
  delete repl_dlg;
  repl_dlg=NULL;
  TopWinPub::FocusedDoc()->setFocus();
  return 1;
}



long SearchDialogs::onSearch(FXObject*o, FXSelector sel, void *p)
{
  FXuint code=(FXuint)(FXuval)p;
  if (repl_dlg) {
    if (code==SciReplGui::DONE) {
      find_dlg->getApp()->addChore(this,ID_REPL_DONE,NULL);
    } else {
      NextReplace(code);
    }
  } else {
    switch (code) {
      case SciReplGui::SEARCH:
      case SciReplGui::SEARCH_NEXT: {
        NextSearch(code);
        break;
      }
      case SciReplGui::DONE: {
        find_dlg->hide();
        parent->layout();
        TopWinPub::FocusedDoc()->setFocus();
        break;
      }
    }
  }
  return 1;
}



void SearchDialogs::setHaveSelection(bool have_sel)
{
  if (repl_dlg) { repl_dlg->Gui()->setHaveSelection(have_sel); }
}



void SearchDialogs::hide()
{
  if (repl_dlg) {
    delete repl_dlg;
    repl_dlg=NULL;
  } else {
    find_dlg->hide();
    parent->layout();
  }
}

