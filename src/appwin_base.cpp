/*
  FXiTe - The Free eXtensIble Text Editor
  Copyright (c) 2009-2012 Jeffrey Pohlmeyer <yetanothergeek@gmail.com>

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
#include <cerrno>

#include "appname.h"
#include "appmain.h"
#include "prefs.h"
#include "lang.h"

#include "scidoc.h"
#include "filer.h"
#include "toolmgr.h"
#include "prefdlg.h"
#include "theme.h"
#include "tagread.h"
#include "recorder.h"
#include "macro.h"
#include "search.h"
#include "backup.h"
#include "menuspec.h"
#include "mainmenu.h"
#include "toolbar.h"
#include "doctabs.h"
#include "outpane.h"
#include "statusbar.h"
#include "runcmd.h"
#include "cmd_utils.h"
#include "scidoc_util.h"
#include "foreachtab.h"
#include "compat.h"

#include "intl.h"
#include "appwin_base.h"


#define STALECHECK 5 /* seconds between checks for external changes. */


FXDEFMAP(TopWindowBase) TopWindowBaseMap[]={
  FXMAPFUNC(SEL_TIMEOUT,  TopWindowBase::ID_TIMER,        TopWindowBase::onTimer),
  FXMAPFUNC(SEL_TIMEOUT,  TopWindowBase::ID_CLOSEWAIT,    TopWindowBase::onCloseWait),
  FXMAPFUNC(SEL_CHORE,    TopWindowBase::ID_CLOSEWAIT,    TopWindowBase::onCloseWait),
  FXMAPFUNC(SEL_CHORE,    TopWindowBase::ID_CHECK_STALE,  TopWindowBase::CheckStale),
  FXMAPFUNC(SEL_CHORE,    TopWindowBase::ID_CHECK_STYLE,  TopWindowBase::CheckStyle),
  FXMAPFUNC(SEL_CHORE,    TopWindowBase::ID_FOCUS_DOC,    TopWindowBase::onFocusDoc),
  FXMAPFUNC(SEL_FOCUSIN,  TopWindowBase::ID_SCINTILLA,    TopWindowBase::onFocusSci),
  FXMAPFUNC(SEL_FOCUSIN,  0,                              TopWindowBase::onFocusIn),
};
FXIMPLEMENT(TopWindowBase,MainWinWithClipBrd,TopWindowBaseMap,ARRAYNUMBER(TopWindowBaseMap))


static bool topwin_closing = false;
static TopWindowBase* global_top_window_instance=NULL;


bool TopWindowBase::Closing() { return topwin_closing; }
void TopWindowBase::Closing(bool is_closing) { topwin_closing=is_closing; }

TopWindowBase* TopWindowBase::instance() { return global_top_window_instance; }

const FXString& TopWindowBase::ConfigDir() { return ((AppClass*)(FXApp::instance()))->ConfigDir(); }

const FXString& TopWindowBase::Connector() { return ((AppClass*)(FXApp::instance()))->Connector(); }

const FXString& TopWindowBase::SessionFile() { return ((AppClass*)getApp())->SessionFile(); }


void TopWindowBase::Cut()   { SciDocUtils::Cut(FocusedDoc()); }

void TopWindowBase::Copy()  { SciDocUtils::Copy(FocusedDoc()); }

void TopWindowBase::Paste() { SciDocUtils::Paste(FocusedDoc()); }





#define PACK_UNIFORM ( PACK_UNIFORM_WIDTH | PACK_UNIFORM_HEIGHT )

TopWindowBase::TopWindowBase(FXApp* a):MainWinWithClipBrd(a,EXE_NAME,NULL,NULL,DECOR_ALL,0,0,600,400)
{
  FXASSERT(!global_top_window_instance);
  global_top_window_instance=this;
  active_widget=NULL;
  recorder=NULL;
  recording=NULL;
  need_status=0;
  stale_ticks=0;
  save_ticks=0;
  skipfocus=false;
  destroying=false;
  close_all_confirmed=false;
  kill_commands_confirmed=false;
  topwin_closing=false;
  StyleDef*sd=GetStyleFromId(Settings::globalStyle(), STYLE_CALLTIP);
  tips=new FXToolTip(getApp(),0);
  RgbToHex(getApp()->getTipbackColor(), sd->bg);
  RgbToHex(getApp()->getTipforeColor(), sd->fg);
  prefs=new Settings(this, ConfigDir());
  SciDoc::DefaultStyles(prefs->Styles());
  menubar=new MainMenu(this);
  outerbox=new FXVerticalFrame(this,FRAME_NONE|LAYOUT_FILL,0,0,0,0,0,0,0,0,0,0);
  innerbox=new FXVerticalFrame(outerbox,FRAME_NONE|LAYOUT_FILL,0,0,0,0,4,4,4,4);
  toolbar=new ToolBarFrame(innerbox);
  hsplit=new FXSplitter(innerbox,this, 0, SPLITTER_VERTICAL|SPLITTER_REVERSED|LAYOUT_FILL|SPLITTER_TRACKING);
  tabbook=new DocTabs(hsplit,this,0,FRAME_NONE|PACK_UNIFORM|LAYOUT_FILL);
  tabbook->setTabStyleByChar(prefs->DocTabPosition);
  tabbook->setTabsCompact(prefs->DocTabsPacked);
  tabbook->MaxTabWidth(prefs->TabTitleMaxWidth);
  outlist=new OutputList(hsplit,NULL,0,LAYOUT_SIDE_TOP|LAYOUT_FILL);
  statusbar=new StatusBar(outerbox,(void*)CommandUtils::DontFreezeMe());
  backups=new BackupMgr(this, ConfigDir());
  completions=new AutoCompleter();
  srchdlgs=new SearchDialogs(this);
  srchdlgs->SetPrefs(prefs->SearchOptions,prefs->SearchWrap,prefs->SearchVerbose);
  cmdutils=new CommandUtils(this);
  filedlgs=new FileDialogs(this,0,prefs->FileFilters);
}


TopWindowBase::~TopWindowBase()
{
  destroying=true;
  delete filedlgs;
  delete backups;
  delete cmdutils;
  delete recorder;
  delete getIcon();
  delete getMiniIcon();
  delete completions;
  global_top_window_instance=NULL;
}



const
#include "fxite.xpm"


void TopWindowBase::create()
{
  AppClass* a=(AppClass*)getApp();
  a->setWheelLines(prefs->WheelLines);
  position(prefs->Left, prefs->Top,prefs->Width,prefs->Height);
  MainWinWithClipBrd::create();
  tips->create();
  show(prefs->placement);
  FXIcon*ico=NULL;
  FXString icon_file=ConfigDir()+"icon.xpm";
  if (FXStat::isFile(icon_file)) {
    FXFileStream*icon_strm=new FXFileStream();
    if (icon_strm->open(icon_file.text(),FXStreamLoad)) {
      ico=new FXXPMIcon(a,NULL,0,IMAGE_KEEP);
      if ( ((FXXPMIcon*)ico)->loadPixels(*icon_strm) ) {
        ico->scale(32,32);
      } else {
        delete ico;
        ico=NULL;
      }
      icon_strm->close();
    }
    delete icon_strm;
    if (!ico) { fxwarning(_("NOTE: Failed to load custom icon.\n")); }
  }
  if (!ico) { ico=new FXXPMIcon(a,icon32x32_xpm,0,IMAGE_KEEP); }
  ico->create();
  setIcon(ico);
  SetupXAtoms(this, a->ServerName().text(), APP_NAME);
  if (prefs->Maximize) { maximize(); }
  FXFont fnt(a, prefs->FontName, prefs->FontSize/10);
  GetFontDescription(prefs->fontdesc,&fnt);
  save_hook.format("%s%s%c%s%c%s.lua", ConfigDir().text(), "tools", PATHSEP, "hooks", PATHSEP, "saved");
  UpdateToolbar();
  MainWinWithClipBrd::create();
  RunHookScript("startup");
  ParseCommands(a->Commands());
  a->addTimeout(this,ID_TIMER,ONE_SECOND,NULL);
#ifndef FOX_1_6
  if (!a->migration_errors.empty()) {
    NewFile(false);
    SciDoc*sci=ControlDoc();
    sci->SetText(a->migration_errors.text());
    a->migration_errors="";
  }
#endif

}



#define confirm(hdr,fmt,arg) \
  ( FXMessageBox::question(this,MBOX_OK_CANCEL,hdr,fmt,arg) == MBOX_CLICKED_OK )

#define not_confirmed() {\
 close_all_confirmed=false; \
 kill_commands_confirmed=false; \
 Closing(false); \
}


FXbool TopWindowBase::close(FXbool notify)
{
  if (Closing()) { return false; }
  if (!getApp()->getActiveWindow()) {
    hide();
    getApp()->runWhileEvents();
    if (Closing()) { return false; }
    show();
    getApp()->runWhileEvents();
    if (Closing()) { return false; }
  }
  if ((!tabbook->isEnabled()) && (!kill_commands_confirmed) ) {
    if (!confirm(_("Command in progress"), "%s", _("External command in progress - exit anyway?"))) {
      not_confirmed();
      return false;
    }
    kill_commands_confirmed=true;
    command_timeout=true;
    getApp()->addChore(this,ID_CLOSEWAIT, NULL);
    getApp()->runWhileEvents();
    return false;
  }

  if (prefs->PromptCloseMultiExit && (tabbook->Count()>1) && (!close_all_confirmed)) {
    if (!confirm(_("Multiple files"), _("Editing %d files - \nclose all tabs?"), tabbook->Count() ) ) {
      not_confirmed();
      FocusedDoc()->setFocus();
      return false;
    }
    close_all_confirmed=true;
  }

  session_data="";
  if (!RunHookScript("shutdown")) { return false; }
  prefs->LastFocused=FocusedDoc()->Filename();
  if (!CloseAll(true)) {
    not_confirmed();
    prefs->LastFocused="";
    FocusedDoc()->setFocus();
    return false;
  }

  if (!session_data.empty()) {
     FXFile fh(SessionFile(), FXIO::Writing);
     if (fh.isOpen()) {
       FXival wrote=0;
       wrote=fh.writeBlock(session_data.text(),session_data.length());
       if (!(fh.close() && (wrote==session_data.length()))) {
         fxwarning(_(EXE_NAME": Could not save %s (%s)"), SessionFile().text(), SystemErrorStr());
       }
     } else {
       fxwarning(_(EXE_NAME": Could not open %s for writing (%s)"), SessionFile().text(), SystemErrorStr());
     }
     session_data="";
  }
  prefs->Maximize=isMaximized();
  if (prefs->Maximize) {
    restore();
    repaint();
    getApp()->runWhileEvents();
  }
  prefs->Left=getX();
  prefs->Top=getY();
  prefs->Width=getWidth();
  prefs->Height=getHeight();

  getApp()->runWhileEvents();
  if (Closing()) { return false; }
  Closing(true);
  delete srchdlgs;
  delete prefs;
  delete menubar;
  return FXMainWindow::close(notify);
}



/*
  When in split-view mode, ControlDoc() refers to the view at the top (or left) of
  the split,and FocusedDoc() refers to whichever view has the focus; In single-view
  mode there is no difference between the ControlDoc and the FocusedDoc.
*/
SciDoc*TopWindowBase::ControlDoc()
{
  FXWindow *page=tabbook->ActivePage();
  return page?((SciDoc*)page->getFirst()):NULL;
}



SciDoc*TopWindowBase::FocusedDoc()
{
  return (SciDoc*)tabbook->ActiveView();
}



long TopWindowBase::CheckStale(FXObject*o, FXSelector sel, void*p)
{
  static bool CheckingStale=false;
  SciDoc*sci=ControlDoc();
  if (!sci) { return 1; }
  switch (sci->Stale()) {
    case 0: { break; }
    case 1: {
      stale_ticks=0;
      if ( !CheckingStale ) {
        if (!IsDesktopCurrent(this)) { return 1; }
        CheckingStale=true;
        FXPopup *popup=getApp()->getPopupWindow();
        if (popup) { popup->popdown(); }
        if (filedlgs->AskReloadForExternalChanges(sci)) {
          if (sci->GetReadOnly()) { SetTabLocked(sci,true); }
        } else {
          sci->DoStaleTest(false);
        }
        CheckingStale=false;
      }
      break;
    }
    case 2: {
      stale_ticks=0;
      if ( !CheckingStale ) {
        if (!IsDesktopCurrent(this)) { return 1; }
        CheckingStale=true;
        FXPopup *popup=getApp()->getPopupWindow();
        if (popup) { popup->popdown(); }
        if (filedlgs->AskSaveMissingFile(sci)) {
          if (!filedlgs->SaveFile(sci,sci->Filename())) { ShowSaveAsDlg(sci); }
        } else {
          sci->DoStaleTest(false);
        }
        CheckingStale=false;
      }
      break;
    }
  }
  return 1;
}




/*
  This chore updates a document's settings, and adds a chore to do the same
  for the next document that needs updating. (See StyleNextDocCB() for more.)
*/
long TopWindowBase::CheckStyle(FXObject*o, FXSelector sel, void*p)
{
  SciDoc*sci=(SciDoc*)p;
  if (sci && sci->NeedStyled()) {
    SciDocUtils::SetSciDocPrefs(sci, prefs);
    sci->UpdateStyle();
    sci->NeedStyled(false);
  }
  tabbook->ForEachTab(TabCallbacks::StyleNextDocCB,this,false);
  return 1;
}



long TopWindowBase::onTimer(FXObject*o, FXSelector sel, void*p)
{
  if (prefs->WatchExternChanges) {
    if (stale_ticks < STALECHECK)  {
      stale_ticks++;
    } else {
      if (getApp()->getActiveWindow() && (getApp()->getActiveWindow()->getShell()==this)) {
        CheckStale(NULL,0,NULL);
      }
    }
  }
  if (prefs->Autosave) {
    if (save_ticks < prefs->AutosaveInterval) {
      save_ticks++;
    } else {
      save_ticks=0;
      tabbook->ForEachTab(TabCallbacks::AutoSaveCB,backups);
    }
  }
  if (prefs->Autosave||prefs->WatchExternChanges) { getApp()->addTimeout(this,ID_TIMER, ONE_SECOND, NULL); }
  return 1;
}



long TopWindowBase::onCloseWait(FXObject*o, FXSelector sel, void*p)
{
  if (FXSELTYPE(sel)==SEL_CHORE) {
    getApp()->addTimeout(this,ID_CLOSEWAIT, ONE_SECOND/10, NULL);
  } else {
    close();
  }
  return 1;
}



long TopWindowBase::onFocusIn(FXObject*o, FXSelector sel, void*p)
{
  long rv=MainWinWithClipBrd::onFocusIn(o,sel,p);
  if (active_widget==outlist) {
    outlist->setFocus();
  } else if (FocusedDoc()) {
    FocusedDoc()->setFocus();
  }
  return rv;
}



long TopWindowBase::onFocusDoc(FXObject*o, FXSelector sel, void*p ) {
  if (skipfocus) {
    skipfocus=false;
  } else {
    if (getApp()->getActiveWindow()==this) {
      SciDoc*sci=FocusedDoc();
      if (sci) { sci->setFocus(); }
    }
  }
  return 0;
}



long TopWindowBase::onFocusSci(FXObject*o,FXSelector s,void*p)
{
  getApp()->addChore(this, ID_CHECK_STALE);
  if (((SciDoc*)o)->NeedStyled()) { getApp()->addChore(this, ID_CHECK_STYLE,o); }
  active_widget=(SciDoc*)o;
  return 1;
}



// Set or unset the '*' or '#' prefix of a tab's title...
void TopWindowBase::SetTabTag(SciDoc*sci, char mark, bool set)
{
  DocTab*tab=(DocTab*)sci->getParent()->getPrev();
  if (set) {
    if (tab->getText()[0]!=mark) { tab->setText(tab->getText().prepend(mark)); }
  } else {
    if (tab->getText()[0]==mark) { tab->setText(tab->getText().erase(0,1)); }
  }
}



// Add '*' prefix to tab's title to denote file has unsaved changes
void TopWindowBase::SetTabDirty(SciDoc*sci, bool dirty)
{
  SetTabTag(sci,'*', dirty);
  sci->Dirty(dirty);
}



// Add '#' prefix to tab's title to denote document is marked read-only
void TopWindowBase::SetTabLocked(SciDoc*sci, bool locked)
{
  SetTabTag(sci,'#', locked);
  sci->sendMessage(SCI_SETREADONLY,locked?1:0,0);
}



bool TopWindowBase::ShowSaveAsDlg(SciDoc*sci)
{
  FXString orig=sci->Filename();
  if (filedlgs->SaveFileAs(sci)) {
    if (!orig.empty()) { menubar->PrependRecentFile(orig); }
    menubar->RemoveRecentFile(sci->Filename());
    return true;
  }
  return false;
}



void TopWindowBase::ShowInsertFileDlg()
{
  FXString* filename=NULL;
  SciDoc*sci=FocusedDoc();
  if (!sci->GetReadOnly()) {
    if (filedlgs->GetOpenFilenames(sci,filename,false)&&!filename->empty()) {
      SciDocUtils::InsertFile(sci,*filename);
      delete[] filename;
    }
  }
  sci->setFocus();
}


void TopWindowBase::ToggleRecorder()
{
  if (recording) {
    recording->sendMessage(SCI_STOPRECORD, 0, 0);
    recording=NULL;
  } else {
    if (!recorder) {recorder=new MacroRecorder(); }
    recorder->clear();
    recording=ControlDoc();
    recording->sendMessage(SCI_STARTRECORD, 0, 0);
  }
  statusbar->Recording(recording);
  menubar->Recording(recording,recorder);
  ControlDoc()->setFocus();  
}



bool TopWindowBase::IsMacroCancelled()
{
  return cmdutils->IsMacroCancelled(command_timeout);
}



class MyCmdIO: public CmdIO {
  virtual bool IsCancelled() { return ((TopWindowBase*)win)->IsMacroCancelled(); }
public:
  MyCmdIO(FXMainWindow *window, const char*shellcmd="/bin/sh -c"):CmdIO(window,shellcmd){}
};



bool TopWindowBase::FilterSelection(SciDoc *sci, const FXString &cmd, const FXString &input)
{
  if (!cmdutils->IsCommandReady()) { return false; }
  cmdutils->CommandBusy(true);
  cmdutils->SetShellEnv(sci->Filename().text(),sci->GetLineNumber());
  bool rv=false;
  if (!cmd.empty()) {
    MyCmdIO cmdio(this, prefs->ShellCommand.text());
    FXString output="";
    command_timeout=false;
    getApp()->beginWaitCursor();
    statusbar->Running(_("command"));
    cmdutils->DisableUI(true);
    if (cmdio.filter(cmd.text(), input, output, &command_timeout)) {
      sci->sendString(SCI_REPLACESEL, 0, output.text());
      sci->ScrollWrappedInsert();
      rv=true;
    }
    cmdutils->DisableUI(false);
    statusbar->Normal();
    getApp()->endWaitCursor();
  }
  sci->setFocus();
  need_status=1;
  cmdutils->CommandBusy(false);
  return rv;
}



bool TopWindowBase::RunCommand(SciDoc *sci, const FXString &cmd)
{
  if (!cmdutils->IsCommandReady()) { return false; }
  cmdutils->CommandBusy(true);
  cmdutils->SetShellEnv(sci->Filename().text(),sci->GetLineNumber());
  bool success=false;
  if (!cmd.empty()) {
    MyCmdIO cmdio(this, prefs->ShellCommand.text());
    command_timeout=false;
    outlist->clearItems(true);
    update();
    repaint();
    if (!prefs->ShowOutputPane) { ShowOutputPane(true); }
    getApp()->beginWaitCursor();
    statusbar->Running(_("command"));
    cmdutils->DisableUI(true);
    getApp()->runWhileEvents();
    success=cmdio.lines(cmd.text(), outlist, outlist->ID_CMDIO, &command_timeout, true);
    cmdutils->DisableUI(false);
    statusbar->Normal();
    getApp()->endWaitCursor();
    if (success) {
      outlist->appendItem(_("Command succeeded."));
    } else {
      if (command_timeout) {
        outlist->appendItem(_("Command cancelled."));
      } else {
        outlist->appendItem(_("Command failed."));
        outlist->SelectFirstError();
      }
    }
  }
  if (FocusedDoc() && (GetNetActiveWindow()==id())) { FocusedDoc()->setFocus(); }
  need_status=1;
  cmdutils->CommandBusy(false);
  return success;
}



bool TopWindowBase::RunMacro(const FXString &script, bool isfilename)
{
  if (!cmdutils->IsCommandReady()) { return false; }
  cmdutils->CommandBusy(true);
  MacroRunner macros;
  command_timeout=false;
  statusbar->Running(_("macro"));
  update();
  cmdutils->DisableUI(true);
  getApp()->runWhileEvents();
  bool rv=isfilename?macros.DoFile(script):macros.DoString(script);
  if (!destroying) {
    tabbook->ForEachTab(TabCallbacks::ResetUndoLevelCB,NULL);
    cmdutils->DisableUI(false);
    statusbar->Normal();
    if (FocusedDoc() && (GetNetActiveWindow()==id())) { FocusedDoc()->setFocus(); }
    need_status=1;
  }
  cmdutils->CommandBusy(false);
  return rv;
}



bool TopWindowBase::RunHookScript(const char*hookname)
{
  static bool running_hook_script=false;
  FXString hook;
  if (running_hook_script) { return false; }
  hook.format("%s%s%c%s%c%s.lua", ConfigDir().text(), "tools", PATHSEP, "hooks", PATHSEP, hookname);
  if (FXStat::isFile(hook)) {
   bool rv;
   running_hook_script=true;
   rv=RunMacro(hook, true);
   running_hook_script=false;
   return rv;
  } else {
    return true;
  }
}



void TopWindowBase::FileSaved(SciDoc* saved)
{
  SciDoc *active=ControlDoc();
  FXStat st;
  if ( FXStat::stat(save_hook,st) && (st.size()>0) && st.isFile() ) {
    if (active!=saved) {
      tabbook->ActivateTab((DocTab*)(saved->getParent()->getPrev()));
    }
    RunHookScript("saved");
    if ( TabCallbacks::IsDocValid(active,tabbook) && (active!=saved) ) {
      tabbook->ActivateTab((DocTab*)(active->getParent()->getPrev()));
    }
  }
}



void TopWindowBase::AskReload()
{
  SciDoc*sci=ControlDoc();
  if (sci->Filename().empty()) {
    FXMessageBox::error(this,MBOX_OK,_("Unamed file"), _("File has no name, can't reload."));
    return;
  }
  if (!sci->Dirty()) {
    if ( FXMessageBox::question(this, MBOX_YES_NO, _("Reload file"),
           _("Reload current document?") ) != MBOX_CLICKED_YES ) { return; }
  }
  filedlgs->AskReload(sci);  
}



void TopWindowBase::AddFileToTagsMenu(const FXString &filename)
{
  menubar->AddFileToTagsList(filename);
}



bool TopWindowBase::RemoveFileFromTagsMenu(const FXString &filename)
{
  return menubar->RemoveFileFromTagsList(filename);
}



bool TopWindowBase::SetLanguage(FXMenuRadio *mnu)
{
  if (mnu) {
    LangStyle*ls=(LangStyle*) mnu->getUserData();
    SciDoc*sci=ControlDoc();
    sci->setLanguage(ls);
    menubar->SetLanguageCheckmark(ls);
    return true;
  } else { return false; }
}



bool TopWindowBase::SetLanguage(const FXString &name)
{
  return SetLanguage(menubar->GetMenuForLanguage(name));
}



void TopWindowBase::AddOutput(const FXString&line)
{
  outlist->appendItem(line.text());
}



void TopWindowBase::ClearOutput()
{
  outlist->clearItems();
}



FXMenuCaption*TopWindowBase::TagFiles() {
  return (FXMenuCaption*)(menubar->TagsMenu()->getMenu()->getFirst());
}



void TopWindowBase::SetKillCommandAccelKey(FXHotKey acckey)
{
  cmdutils->SetKillCommandAccelKey(acckey);
}



bool TopWindowBase::SaveAll(bool break_on_fail)
{
  DocTab*curr=tabbook->ActiveTab();
  FXWindow*tab,*page;
  bool success=true;
  for (tab=tabbook->getFirst(); tab && (page=tab->getNext()); tab=page->getNext()) {
    SciDoc*sci=(SciDoc*)page->getFirst();
    if (sci->Dirty()) {
      if (sci->Filename().empty() ) {
        tabbook->ActivateTab((DocTab*)tab);
      }
      if (!filedlgs->SaveFile(sci,sci->Filename())) {
        if (break_on_fail) { return false; } else { success=false; }
      }
    }
  }
  if (curr!=tabbook->ActiveTab()) { tabbook->ActivateTab((curr)); }
  return success;
}



// Tell the window manager we want the focus back after dialogs close...
void TopWindowBase::ClosedDialog()
{
  setFocus();
  FocusedDoc()->setFocus();
}



static bool AvoidMultiLineCommand(TopWindowBase*w, const FXString &cmd)
{
  if (cmd.contains('\n')) {
    FXMessageBox::error(w, MBOX_OK, _("Command Error"),
      _("Multiline commands are not supported."));
    return false;
  } else {
    return true;
  }
}



void TopWindowBase::ShowFilterDialog(bool is_filter)
{
  SciDoc *sci=FocusedDoc();
  HistBox *dlg;
  bool save_first;
  if(is_filter) {
    dlg=new HistBox(this, _("Filter selection"), _("Command:"), "Filters");
    save_first=prefs->SaveBeforeFilterSel;
  } else {
    dlg=new HistBox(this, _("Insert output of command"), _("Command:"), "InsertOutput");
    save_first=prefs->SaveBeforeInsCmd;
  }
  dlg->setNumColumns(48);
  if ( dlg->execute(PLACEMENT_OWNER) ) {
    FXString cmd=dlg->getText();
    if (AvoidMultiLineCommand(this, cmd)) {
      if ( (!save_first) || SaveAll(true) ) {
        FXString input="";
        if (is_filter) { sci->GetSelText(input); }
        FilterSelection(sci, cmd, input);
      }
    }
  }
  delete dlg;
  ClosedDialog();
}



void TopWindowBase::ShowCommandDialog()
{
  SciDoc *sci=FocusedDoc();
  HistBox dlg(this, _("Run command"), _("Command:"), "Commands");
  dlg.setNumColumns(48);
  if ( dlg.execute(PLACEMENT_OWNER) ) {
    FXString cmd=dlg.getText();
    if (AvoidMultiLineCommand(this,cmd)) {
      if ( (!prefs->SaveBeforeExecCmd) || SaveAll(true) ) {
        ClosedDialog();
        RunCommand(sci,cmd);
      }
    }
  } else {
    ClosedDialog();
  }
}




void TopWindowBase::SetBookmark()
{
  SciDoc*sci=ControlDoc();
  bookmarked_file=sci->Filename();
  bookmarked_tab=tabbook->ActiveTab();
  bookmarked_pos=sci->GetCaretPos();
}


bool TopWindowBase::SetReadOnly(SciDoc*sci, bool rdonly)
{
  if (!sci) { return false; }
  if (rdonly && sci->Dirty()) {
    FXMessageBox::error(this, MBOX_OK, _("Unsaved changes"),
      _("Cannot mark a modified document as read-only.\n"
        "You should save or undo your changes first.")
    );
    return false;
  }
  SetTabLocked(sci,rdonly);
  menubar->SetReadOnlyCheckmark(rdonly);
  need_status=32;
  return true;
}


void TopWindowBase::EnableUserFilters(bool enabled)
{
  toolbar->EnableFilterBtn(enabled);
  menubar->EnableFilterMenu(enabled);
}



void TopWindowBase::RemoveTBarBtnData(void*p)
{
  toolbar->NullifyButtonData(p);
}


// Exposes "userland" search behavior to scripting engine.
bool TopWindowBase::FindText(const char*searchstring, FXuint searchmode, bool forward)
{
  return srchdlgs->FindPhrase(ControlDoc(),searchstring,searchmode,forward);
}



UserMenu**TopWindowBase::UserMenus() const
{
  return menubar->UserMenus();
}



void TopWindowBase::AdjustIndent(SciDoc*sci, char ch)
{
  SciDocUtils::AdjustIndent(sci, ch, prefs, recording);
}



void TopWindowBase::OpenSelected()
{
  SciDocUtils::OpenSelected(this, FocusedDoc());
}



// Create a new tab and editor panel
bool TopWindowBase::OpenFile(const char*filename, const char*rowcol, bool readonly, bool hooked)
{
  if (Closing()) return false;
  FXString fn="";
  SciDoc*sci=NULL;
  if ( tabbook->Count() >= prefs->MaxFiles ) { return false; }
  if (filename) {
    fn=FXPath::simplify(FXPath::absolute(filename));
#ifdef WIN32
     FileDialogs::ReadShortcut(this,fn);
#endif
    tabbook->ForEachTab(TabCallbacks::FileAlreadyOpenCB,&fn);
    if (fn.empty()) {
      if (rowcol && *rowcol) { ControlDoc()->GoToStringCoords(rowcol); }
      if (hooked) { RunHookScript("opened"); }
      return true;
    }
  }
  if (!FileDialogs::FileExistsOrConfirmCreate(this,fn)) { return false; }
  DocTab*tab=tabbook->NewTab(fn.empty()?_("Untitled"):FXPath::name(fn));
  sci=SciDocUtils::NewSci((FXComposite*)tab->getNext(),this,prefs);
  if (!fn.empty()) {
    if (!sci->LoadFromFile(fn.text())) {
      if (!sci->GetLastError().contains(SciDoc::BinaryFileMessage())) {
         FXMessageBox::error(this, MBOX_OK, _("Error opening file"), "%s:\n%s\n%s",
             _("Could not open file"),  fn.text(),  sci->GetLastError().text());
      }
      SciDocUtils::DoneSci(sci,recording);
      return false;
    }
    if (!sci->SetLanguageForHeader(fn)) {
      if (!sci->setLanguageFromFileName(FXPath::name(fn).text())) {
        sci->setLanguageFromContent();
      }
    }
  } else {
    sci->SetUTF8(!prefs->DefaultToAscii);
    sci->UpdateStyle();
  }
  SetTabLocked(sci,readonly);
  tabbook->ActivateTab(tabbook->Count()-1);
  menubar->AppendDocList(sci->Filename(), tab);
  if (shown()) {
    sci->create();
    if (rowcol && *rowcol) { sci->GoToStringCoords(rowcol); }
  }

  //  If the only thing we had open prior to this file was a single,
  //  empty, untitled, unmodified document, then close it...
  if ((tabbook->Count()==2) && (!sci->Filename().empty())) {
    SciDoc*sc0=(SciDoc*)(tabbook->PageAt(0)->getFirst());
    if ( sc0->Filename().empty() && (sc0->GetTextLength()==0) && (sc0->Dirty()==false)  ) {
      tabbook->ActivateTab(0);
      CloseFile(false,false);
    }
  }
  sci->setFocus();
  if (hooked) { RunHookScript("opened"); }
  return true;
}



bool TopWindowBase::NewFile(bool hooked)
{
  return OpenFile(NULL,NULL,false,hooked);
}



bool TopWindowBase::CloseFile(bool close_last, bool hooked)
{
  FXint i=tabbook->getCurrent();
  DocTab*tab=tabbook->ActiveTab();
  SciDoc*sci=ControlDoc();
  if (!filedlgs->TryClose(sci,tab->getText().text())) { return false; }
  menubar->PrependRecentFile(sci->Filename());
  if (hooked) { RunHookScript("closing"); }
  if (close_last) {
    if (!sci->Filename().empty()) {
      FXString line;
      line.format("-%c\n+%ld,%ld\n%s\n", sci->GetReadOnly()?'r':'w',
        sci->GetLineNumber()+1, sci->GetColumnNumber(), sci->Filename().text());
      session_data.append(line);
    }
  }
  backups->RemoveBackup(sci);
  if (tabbook->numChildren()==2) {
    if (!close_last) { NewFile(false); }
    SciDocUtils::DoneSci(sci,recording);
  } else {
    SciDocUtils::DoneSci(sci,recording);
    FXWindow *w=tabbook->childAtIndex(i*2);
    if (!w) {
      i--;
      w=tabbook->childAtIndex(i*2);
    }
    tabbook->ActivateTab(i);
  }
  need_status=1;
  return true;
}



bool TopWindowBase::CloseAll(bool close_last)
{
  bool rv;
  do {
    tabbook->ActivateTab(0);
  } while ((tabbook->numChildren()>0) && CloseFile(true,true));
  rv=tabbook->numChildren()==0;
  if (!close_last) { NewFile(false); }
  return rv;
}



void TopWindowBase::ChangeCase(bool to_upper)
{
  SciDoc*sci=FocusedDoc();
  if (sci->GetSelLength()==0) { return; }
  if (to_upper) { sci->SelectionToUpper(); } else { sci->SelectionToLower(); }
  if (recording==sci) { recorder->record(to_upper?SCI_UPPERCASE:SCI_LOWERCASE,0,0); }
}



void TopWindowBase::FindTag()
{
  FXString filename;
  FXString locn;
  FXString pattern;
  if ( !TagHandler::FindTag(FocusedDoc(), TagFiles(), filename, locn, pattern) ) { return; }
  if ( filename.empty()) { return; }
  if (OpenFile(filename.text(), locn.text(),false,true)) {
    if (locn.empty()) { TagHandler::GoToTag(ControlDoc(),pattern); }
  }
}



void TopWindowBase::GoToBookmark()
{
  if (!bookmarked_file.empty()) {
    if (OpenFile(bookmarked_file.text(),NULL,false,false)) {
      FocusedDoc()->GoToPos(bookmarked_pos);
    }
  } else {
    tabbook->ForEachTab(TabCallbacks::BookmarkCB,this);
  }
}



bool TopWindowBase::FoundBookmarkedTab(DocTab*tab)
{
  if (bookmarked_tab == tab) {
    tabbook->ActivateTab(tab);
    FocusedDoc()->GoToPos(bookmarked_pos);
    return true;
  }
  return false;
}



void TopWindowBase::UpdateToolbar()
{
  toolbar->CreateButtons((TopWindow*)this);
  menubar->UpdateDocTabSettings();
  menubar->Recording(recording,recorder);
  toolbar->EnableFilterBtn(FocusedDoc()&&(FocusedDoc()->GetSelLength()>0));
}



void TopWindowBase::ShowPrefsDialog()
{
  PrefsDialog prefdlg(this, prefs);
  prefdlg.execute(PLACEMENT_DEFAULT);
  ClosedDialog();
  srchdlgs->SetPrefs(prefs->SearchOptions,prefs->SearchWrap,prefs->SearchVerbose);
  tabbook->MaxTabWidth(prefs->TabTitleMaxWidth);
  tabbook->ForEachTab(TabCallbacks::PrefsCB, NULL);
  CheckStyle(NULL,0,ControlDoc());
  if ((prefs->WatchExternChanges||prefs->Autosave) && !getApp()->hasTimeout(this,ID_TIMER)) {
    getApp()->addTimeout(this,ID_TIMER, ONE_SECOND, NULL);
  }
  getApp()->setWheelLines(prefs->WheelLines);
  if ( PrefsDialog::ChangedToolbar() & ToolbarChangedLayout ) {
    UpdateToolbar();
  }
  if ( PrefsDialog::ChangedToolbar() & ToolbarChangedWrap ) {
    toolbar->handle(toolbar,FXSEL(SEL_CONFIGURE,0),NULL);
  }
  if ( PrefsDialog::ChangedToolbar() & ToolbarChangedFont ) {
    toolbar->SetTBarFont();
  }
  filedlgs->patterns(prefs->FileFilters);
  if (Theme::changed() & ThemeChangedColors) {
    Theme::apply(this);
    Theme::apply(srchdlgs->FindDialog());
    tips->setBackColor(getApp()->getTipbackColor());
    tips->setTextColor(getApp()->getTipforeColor());
    statusbar->Colorize();
  }
  tabbook->ActivateTab(tabbook->ActiveTab());
  toolbar->SetToolbarColors();
  EnableUserFilters(FocusedDoc()->GetSelLength());  
}




void TopWindowBase::ShowToolManagerDialog()
{
  ToolsDialog tooldlg(this,UserMenus());
  tooldlg.execute(PLACEMENT_SCREEN);
  RescanUserMenu();
  ClosedDialog();
}



void TopWindowBase::RescanUserMenu()
{
  menubar->RescanUserMenus();
  MenuMgr::PurgeTBarCmds();
  UpdateToolbar();
}



void TopWindowBase::ParseCommands(FXString &commands)
{
  FXString sect="";
  FXString rowcol="";
  FXRex rx_rowcol(":\\d+$",REX_CAPTURE);
  FXchar rw='w';
  bool tagopt=false;
  bool macopt=false;
  bool session_restored=false;
  int i=0;
  if (!((compare(commands,"-q\n",2)==0) || (commands.contains("\n-q\n")))) {
    hide();
    show();
    getApp()->runWhileEvents();
  }
  while (1) {
    sect=commands.section('\n',i++);
    if (sect.empty()) { break; }
    switch (sect.text()[0]) {
      case '+': {
        rowcol=sect.text();
        break;
      }
      case '-': {
        switch (sect.text()[1]) {
          case 'r':
          case 'w': {
            rw=sect.text()[1];
            break;
          }
          case 't': {
            tagopt=true;
            break;
          }
          case 'e': {
            macopt=true;
            break;
          }
          case 'p' : {
            if (!session_restored) {
              session_restored=true;
              FXString sessionname;
              FXFile fh(SessionFile(), FXIO::Reading);
              if (fh.isOpen()) {
                session_data.length(fh.size());
                fh.readBlock((void*)(session_data.text()),fh.size());
                fh.close();
                ParseCommands(session_data);
                if (!prefs->LastFocused.empty()) {
                  if (FXStat::isFile(prefs->LastFocused)) {
                    tabbook->ForEachTab(TabCallbacks::FileAlreadyOpenCB,&prefs->LastFocused);
                  }
                }
              }
            }
            break;
          }
          case 'q' : {
            break; // "quiet" option is invalid for a new instance, so just ignore it.
          }
          default: fprintf(stderr, _("Warning: unrecognized option: %s\n"), sect.text());
        }
        break;
      }
      default: {
        if (tagopt) {
          AddFileToTagsMenu(sect);
          tagopt=false;
        } else {
          if (macopt) {
            macopt=false;
            if (tabbook->numChildren()==0) { /* Be sure we have a document before running a macro */
              NewFile(false);
              RunMacro(sect,false);
              if (tabbook->numChildren()==2) {
                SciDoc*sci=ControlDoc();
                if ( (sci->Filename().empty()) && (sci->GetTextLength()==0) && (!sci->Dirty()) ) {
                  CloseFile(true,false); /* Forget this document if it wasn't changed */
                }
              }
            } else {
              RunMacro(sect,false);
            }
          } else {
            FXString filename=sect.text();
            if (rowcol.empty()) {
              FXint beg,end;
              if (rx_rowcol.match(filename,&beg,&end)) {
                if (!FXStat::isFile(filename)) {
                  rowcol=filename.mid(beg,end-beg).text();
                  filename=filename.left(beg).text();
                  if (!FXStat::isFile(filename)) {
                    rowcol="";
                    filename=sect.text();
                  }
                }
              }
            }
            OpenFile(filename.text(), rowcol.empty()?NULL:(rowcol.text())+1, rw=='r',true);
            rowcol="";
          }
        }
      }
    }
  }
  commands=FXString::null;
  if (tabbook->numChildren()==0) { NewFile(false); }
}



void TopWindowBase::UpdateTitle(long line, long col)
{
  SciDoc*sci=ControlDoc();
  if (sci) {
    DocTab *tab=tabbook->ActiveTab();
    FXString s;
    s.format("%s  %s - %s", tab->getText().text(), FXPath::directory(sci->Filename()).text(), EXE_NAME);
    setTitle(s);
    menubar->SetLanguageCheckmark(sci->getLanguage());
    menubar->SetReadOnlyCheckmark(sci->sendMessage(SCI_GETREADONLY,0,0));
    menubar->SetWordWrapCheckmark(sci->GetWordWrap());
    statusbar->FileInfo(sci->Filename(),sci->GetUTF8(),line,col);
    MenuMgr::UpdateEolMenu(sci);
  } else {
    setTitle(EXE_NAME);
    statusbar->Clear();
  }
}



void TopWindowBase::ShowLineNumbers(bool showit)
{
  prefs->ShowLineNumbers=showit;
  tabbook->ForEachTab(TabCallbacks::LineNumsCB, (void*)(FXival)showit);
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowStatusBar(bool showit)
{
  prefs->ShowStatusBar=showit;
  statusbar->Show(showit);
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowOutputPane(bool showit)
{
  prefs->ShowOutputPane=showit;
  if (showit) {
    if (prefs->OutputPaneHeight<16) { prefs->OutputPaneHeight=16; }
    hsplit->setSplit(1, prefs->OutputPaneHeight);
    outlist->show();
  } else {
    outlist->hide();
    hsplit->setSplit(1,0);
  }
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowWhiteSpace(bool showit)
{
  prefs->ShowWhiteSpace=showit;
  tabbook->ForEachTab(TabCallbacks::WhiteSpaceCB, (void*)(FXival)showit);
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowToolbar(bool showit)
{
  prefs->ShowToolbar=showit;
  if (showit) { toolbar->show(); } else { toolbar->hide(); }
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowMargin(bool showit)
{
  prefs->ShowRightEdge = showit;
  tabbook->ForEachTab(TabCallbacks::ShowMarginCB, (void*)(FXival)showit);
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowIndent(bool showit)
{
  prefs->ShowIndentGuides = showit;
  tabbook->ForEachTab(TabCallbacks::ShowIndentCB, (void*)(FXival)showit);
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::ShowCaretLine(bool showit)
{
  prefs->ShowCaretLine = showit;
  tabbook->ForEachTab(TabCallbacks::ShowCaretLineCB, (void*)prefs);
  menubar->SyncPrefsCheckmarks();
}



void TopWindowBase::SetWordWrap(SciDoc*sci, bool wrapped)
{
  sci->SetWordWrap(wrapped);
  menubar->SetWordWrapCheckmark(wrapped);
}



void TopWindowBase::InvertColors(bool inverted)
{
  prefs->InvertColors=inverted;
  toolbar->SetToolbarColors();
  tabbook->ForEachTab(TabCallbacks::PrefsCB,NULL);
  CheckStyle(NULL,0,ControlDoc());
  menubar->SyncPrefsCheckmarks(); 
}
