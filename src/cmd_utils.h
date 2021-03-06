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

#ifndef FXITE_RUNCMD_H
#define FXITE_RUNCMD_H


class TopWindowBase;

class CommandUtils: public FXObject {
private:
  FXAccelTable* saved_accels;
  FXAccelTable* temp_accels;
  bool command_busy;
  FXHotKey killkey;
  MenuSpec*killcmd;
  TopWindow*tw;
  FXApp*app;
#ifdef WIN32
  FXuint winkey;
#endif
  void SetKillKey(FXHotKey k);
  void InitKillKey();
public:
  static void Freeze(FXWindow*win, bool frozen);
  static const char* DontFreezeMe();
  FXHotKey KillKey() { return killkey; }
  void SetKillCommandAccelKey(FXHotKey acckey);
  void DisableUI(bool disabled);
  void CommandBusy(bool bsy) { command_busy=bsy; }
  bool CommandBusy() { return command_busy; }
  bool IsCommandReady();
  bool IsMacroCancelled(bool &command_timeout);
  static void SetShellEnv(const char*file, long line);
  static const FXString FixUpCmdLineEnv(const FXString&command);
  CommandUtils(TopWindowBase*w, MenuSpec*kill_spec);
  ~CommandUtils();
};

#endif

