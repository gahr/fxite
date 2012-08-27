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

#include "lang.h"
#include "menuspec.h"
#include "intl.h"

#include "statusbar.h"


FXDEFMAP(StatusBar) StatusBarMap[]={};

FXIMPLEMENT(StatusBar,FXHorizontalFrame,StatusBarMap,ARRAYNUMBER(StatusBarMap));



StatusBar::StatusBar(FXComposite *p, void* dont_freeze):
    FXHorizontalFrame(p,LAYOUT_SIDE_BOTTOM|LAYOUT_FILL_X|FRAME_RAISED, 0,0,0,0, 3,3,3,3, 7,3)
{
  coords=new FXTextField(this,12,NULL,FRAME_RAISED|FRAME_SUNKEN|TEXTFIELD_READONLY);
  coords->setEditable(false);

  docname=new FXTextField(this, 64, NULL, FRAME_RAISED|FRAME_SUNKEN|TEXTFIELD_READONLY);
  docname->setEditable(false);

  encname=new FXTextField(this, 6, NULL, FRAME_RAISED|FRAME_SUNKEN|TEXTFIELD_READONLY);
  encname->setEditable(false);

  mode=new FXLabel(this, FXString::null, NULL,JUSTIFY_LEFT|LAYOUT_FIX_Y);
  mode->setY(6);
  mode->hide();
  mode->setUserData(dont_freeze);

  Colorize();
}



void StatusBar::SetKillID(FXSelector kill_id)
{
  id_kill=kill_id;
}



void StatusBar::Coords(long line, long col)
{
  char rowcol[16];
  memset(rowcol,0,sizeof(rowcol));
  snprintf(rowcol,sizeof(rowcol)-1," %ld:%ld",line+1,col);
  coords->setText(rowcol);
}



void StatusBar::FileInfo(const FXString &filename, bool utf8, long line, long column)
{
  docname->setText(filename);
  encname->setText(utf8?"UTF-8":"ASCII");
  Coords(line,column);
}



void StatusBar::Running(const char*what)
{
  FXString status;
  status.format(_("Running %s (press %s to cancel)"), what, MenuMgr::LookupMenu(id_kill)->accel);
  Mode(status.text(), true);
}



void StatusBar::Normal()
{
  Mode("");
}



void StatusBar::Recording(bool recording)
{
  Mode(recording?_("(recording)"):"");
}



void StatusBar::Clear()
{
  Normal();
  docname->setText("");
  encname->setText("");
  Coords(0,0);
}



void StatusBar::Colorize()
{
  FXColor clr=getBaseColor();
  coords->setShadowColor(clr);
  coords->setBackColor(clr);
  coords->setEditable(false);
  docname->setShadowColor(clr);
  docname->setBackColor(clr);
  encname->setShadowColor(clr);
  encname->setBackColor(clr);
  mode->setBackColor(HexToRGB("#FFFFCC"));
  mode->setTextColor(HexToRGB("#FF0000"));
}



void StatusBar::Mode(const char*msg, bool squeeze_docname)
{
  if (squeeze_docname) {
    docname->setNumColumns(24);
  } else {
    docname->setNumColumns(64);
  }
  mode->setText(msg);
  if (msg&&*msg) {
    mode->show();
  } else {
    mode->hide();
  }
}



void StatusBar::Show(bool showit)
{
  if (showit) { show(); } else { hide(); }
  getParent()->layout();  // <=Layout doesn't work right without this
}

