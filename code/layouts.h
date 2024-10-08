/*
    ZNO Music Player
    Copyright (C) 2024  Jamie Dennis

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef LAYOUTS_H
#define LAYOUTS_H

static const char *DEFAULT_LAYOUT_INI =
"[Window][##main_dockspace_window]\n"
"Pos=-8,14\n"
"Size=1874,992\n"
"Collapsed=0\n"
"\n"
"[Window][###Library]\n"
"Pos=331,22\n"
"Size=1015,976\n"
"Collapsed=0\n"
"DockId=0x00000002,0\n"
"\n"
"[Window][###AlbumList]\n"
"Pos=331,22\n"
"Size=1015,976\n"
"Collapsed=0\n"
"DockId=0x00000002,2\n"
"\n"
"[Window][###Metadata]\n"
"Pos=0,22\n"
"Size=329,482\n"
"Collapsed=0\n"
"DockId=0x00000003,0\n"
"\n"
"[Window][###UserPlaylists]\n"
"Pos=0,506\n"
"Size=329,492\n"
"Collapsed=0\n"
"DockId=0x00000004,0\n"
"\n"
"[Window][###PlaylistTracks]\n"
"Pos=331,22\n"
"Size=1015,976\n"
"Collapsed=0\n"
"DockId=0x00000002,1\n"
"\n"
"[Window][Debug##Default]\n"
"Pos=60,60\n"
"Size=400,400\n"
"Collapsed=0\n"
"\n"
"[Window][Edit hotkeys]\n"
"Pos=331,22\n"
"Size=1015,976\n"
"Collapsed=0\n"
"DockId=0x00000002,4\n"
"\n"
"[Window][###SearchResults]\n"
"Pos=331,22\n"
"Size=1015,976\n"
"Collapsed=0\n"
"DockId=0x00000002,3\n"
"\n"
"[Window][###ThemeEditor]\n"
"Pos=1348,22\n"
"Size=510,976\n"
"Collapsed=0\n"
"DockId=0x00000006,0\n"
"\n"
"[Window][###Queue]\n"
"Pos=1348,22\n"
"Size=510,976\n"
"Collapsed=0\n"
"DockId=0x00000006,0\n"
"\n"
"[Window][About]\n"
"Pos=179,55\n"
"Size=547,906\n"
"Collapsed=0\n"
"\n"
"[Window][Preferences]\n"
"Pos=331,22\n"
"Size=1015,976\n"
"Collapsed=0\n"
"DockId=0x00000002,5\n"
"\n"
"[Table][0x70A4391D,4]\n"
"RefScale=16\n"
"Column 0  Width=344\n"
"Column 1  Width=150\n"
"Column 2  Width=150\n"
"Column 3  Width=150\n"
"\n"
"[Table][0xAF517B16,4]\n"
"RefScale=16\n"
"Column 0  Width=244\n"
"Column 1  Width=150\n"
"Column 2  Width=150\n"
"Column 3  Width=150\n"
"\n"
"[Table][0x0A32FE7C,4]\n"
"RefScale=16\n"
"Column 0  Width=160\n"
"Column 1  Width=150\n"
"Column 2  Width=90\n"
"Column 3  Width=150\n"
"\n"
"[Docking][Data]\n"
"DockSpace       ID=0x241994F5 Window=0x92AD2571 Pos=0,22 Size=1858,976 Split=X Selected=0x5B8E20CB\n"
"  DockNode      ID=0x00000005 Parent=0x241994F5 SizeRef=1346,976 Split=X\n"
"    DockNode    ID=0x00000001 Parent=0x00000005 SizeRef=329,976 Split=Y Selected=0x2BD97785\n"
"      DockNode  ID=0x00000003 Parent=0x00000001 SizeRef=237,482 Selected=0x2F2C3127\n"
"      DockNode  ID=0x00000004 Parent=0x00000001 SizeRef=237,492 Selected=0x2BD97785\n"
"    DockNode    ID=0x00000002 Parent=0x00000005 SizeRef=1015,976 CentralNode=1 Selected=0x394F861E\n"
"  DockNode      ID=0x00000006 Parent=0x241994F5 SizeRef=510,976 Selected=0x55C68BEA\n"
"\n"
"[ZNO][Queue]\n"
"Open = 1\n"
"[ZNO][AlbumList]\n"
"Open = 1\n"
"[ZNO][Metadata]\n"
"Open = 1\n"
"[ZNO][UserPlaylists]\n"
"Open = 1\n"
"[ZNO][PlaylistTracks]\n"
"Open = 1\n"
"[ZNO][ThemeEditor]\n"
"Open = 0\n"
"[ZNO][SearchResults]\n"
"Open = 1\n"
"[ZNO][Library]\n"
"Open = 1\n"
"\n";

#endif //LAYOUTS_H
