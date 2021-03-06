/*
  FXiTe - The Free eXtensIble Text Editor
  Copyright (c) 2009-2014 Jeffrey Pohlmeyer <yetanothergeek@gmail.com>

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
#include "toolbar.h"
#include "menuspec.h"
#include "tooltree.h"
#include "shmenu.h"

#include "intl.h"
#include "cfg_toolbar.h"



FXDEFMAP(ToolbarPrefs) ToolbarPrefsMap[]={
  FXMAPFUNC(SEL_COMMAND,ToolbarPrefs::ID_ITEM_REMOVE,ToolbarPrefs::onRemoveItem),
  FXMAPFUNC(SEL_COMMAND,ToolbarPrefs::ID_INSERT_CUSTOM,ToolbarPrefs::onInsertCustomItem),
  FXMAPFUNC(SEL_COMMAND,ToolbarPrefs::ID_CHANGE_BTN_SIZE,ToolbarPrefs::onChangeBtnSize),
  FXMAPFUNC(SEL_COMMAND,ToolbarPrefs::ID_CHANGE_BTN_WRAP,ToolbarPrefs::onChangeBtnWrap),
  FXMAPFUNC(SEL_QUERY_TIP, 0, ToolbarPrefs::onQueryTip),
};

FXIMPLEMENT(ToolbarPrefs,DualListForm,ToolbarPrefsMap,ARRAYNUMBER(ToolbarPrefsMap));



ToolbarPrefs::ToolbarPrefs(FXComposite*p, ToolBarFrame*tbar, UserMenu**ums, MenuMgr*mmgr):DualListForm(p,NULL,0,TBAR_MAX_BTNS)
{
  mnumgr=mmgr;
  user_menus=ums;
  toolbar=tbar;
  invalid=mnumgr->LastID();

  FXHorizontalFrame* AvailBtns=new FXHorizontalFrame( left_column,
                                                     FRAME_RAISED|LAYOUT_FILL_X|LAYOUT_CENTER_X|PACK_UNIFORM_WIDTH);
  custom_btn=new FXButton( AvailBtns, _("Custom &Tools..."),
                                NULL,this, ID_INSERT_CUSTOM,BUTTON_NORMAL|LAYOUT_CENTER_X);

  FXLabel*btn_size_cpn=new FXLabel(mid_column, _("Button size:"), NULL, LABEL_NORMAL|JUSTIFY_LEFT);
  btn_size_cpn->setPadTop(48);
  FXListBox *size_list=new FXListBox(mid_column,this,ID_CHANGE_BTN_SIZE);
  size_list->appendItem(_("small"),  NULL,NULL);
  size_list->appendItem(_("medium"), NULL,NULL);
  size_list->appendItem(_("large"),  NULL,NULL);
  size_list->setNumVisible(3);
  size_list->setCurrentItem(toolbar->ButtonSize());

  FXCheckButton*wrap_tbar_chk=new FXCheckButton(mid_column,_("Wrap buttons"),this,ID_CHANGE_BTN_WRAP);
  wrap_tbar_chk->setCheck(toolbar->Wrapped());
}


long ToolbarPrefs::onQueryTip(FXObject*o, FXSelector sel, void*p)
{
  FXList*w=dynamic_cast<FXList*>((FXObject*)p);
  if (w) {
    FXint index,cx,cy;
    FXuint btns;
    if (w->getCursorPosition(cx,cy,btns) && (index=w->getItemAt(cx,cy))>=0) {
      MenuSpec*spec=(MenuSpec*)(w->getItem(index)->getData());
      if (spec) {
        FXString tip=FXString::null;
        mnumgr->GetTBarBtnTip(spec,tip);
        o->handle(this,FXSEL(SEL_COMMAND,ID_SETSTRINGVALUE),(void*)&tip);
        return 1;
      }
    }
  }
  return 0;
}



long ToolbarPrefs::onChangeBtnSize(FXObject*o,FXSelector sel,void*p)
{
  toolbar->ButtonSize((FXuchar)((FXival)p));
  return 1;
}



long ToolbarPrefs::onChangeBtnWrap(FXObject*o,FXSelector sel,void*p)
{
  toolbar->Wrapped((bool)((FXival)p));
  return 1;
}



long ToolbarPrefs::onRemoveItem(FXObject*o,FXSelector sel,void*p)
{
  FXint iUsed=used_items->getCurrentItem();
  MenuSpec*spec=(MenuSpec*)used_items->getItemData(iUsed);
  if (spec->type=='u') {
    MenuMgr::RemoveTBarUsrCmd(toolbar, spec);
    used_items->removeItem(iUsed);
    CheckCount();
  } else {
    DualListForm::onRemoveItem(o,sel,p);
  }
  return 1;
}



long ToolbarPrefs::onInsertCustomItem(FXObject*o,FXSelector sel,void*p)
{
  FXMenuCommand*mc;
  if (ToolsTree::SelectTool(this, user_menus, mc)) {
    const char*newpath=(const char*)mc->getUserData();
    if (newpath) {
      // If the command is already in the used items list, just select it...
      for (FXint i=0; i<used_items->getNumItems(); i++) {
        const char*oldpath=MenuMgr::GetUsrCmdPath((MenuSpec*)used_items->getItemData(i));
        if (oldpath && (strcmp(newpath, oldpath)==0)) {
          used_items->selectItem(i);
          used_items->setCurrentItem(i);
          used_items->makeItemVisible(i);
          CheckIndex();
          return 1;
        }
      }
    }
    MenuSpec*spec=MenuMgr::AddTBarUsrCmd(mc);
    const char *path=MenuMgr::GetUsrCmdPath(spec);
    FXString s;
    if (path && UserMenu::MakeLabelFromPath(path,s)) {
      s=stripHotKey(s.section('\t',0));
    } else { s=spec->pref; }
    FXListItem*item=new FXListItem(s.text(),NULL,(void*)spec);
    InsertItem(item);
  }
  return 1;
}



void ToolbarPrefs::CheckCount()
{
  FXint*btns=mnumgr->TBarBtns();
  FXint iUsed;
  for (iUsed=0; iUsed<max_items; iUsed++) {
    btns[iUsed]=invalid;
  }
  for (iUsed=0; iUsed<used_items->getNumItems(); iUsed++) {
    MenuSpec*spec=(MenuSpec*)(used_items->getItemData(iUsed));
    btns[iUsed]=spec->sel;
  }
  DualListForm::CheckCount();
  if (ins_btn->isEnabled()) { custom_btn->enable(); } else { custom_btn->disable(); }
}



void ToolbarPrefs::PopulateAvail()
{
  for (MenuSpec*spec=mnumgr->MenuSpecs(); spec->sel!=invalid; spec++) {
    if (spec->ms_mc) {
      avail_items->appendItem(new FXListItem(spec->pref, NULL, (void*)spec));
    }
  }
}



void ToolbarPrefs::PopulateUsed()
{
  for (FXint*isel=mnumgr->TBarBtns(); *isel!=invalid; isel++) {
    MenuSpec* spec=mnumgr->LookupMenu(*isel);
    if (spec) {
      FXint found=avail_items->findItemByData((void*)spec);
      if (found>=0) {
        FXListItem*item=avail_items->extractItem(found);
        item->setSelected(false);
        used_items->appendItem(item);
      } else {
        used_items->appendItem(new FXListItem(FXString::null,NULL,(void*)spec));
      }
    }
  }
}

