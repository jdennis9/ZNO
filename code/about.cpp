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
#include "ui_functions.h"
#include "main.h"
#include <sndfile.h>
#include <imgui.h>

void show_license_info() {
    ImGui::SeparatorText("ZNO Music Player");
    ImGui::TextUnformatted("Version: " APP_VERSION_STRING);
    ImGui::TextUnformatted("Copyright (C) 2024  Jamie Dennis");
    
    ImGui::SeparatorText("libsndfile");
    ImGui::TextUnformatted("Copyright (C) 1999-2016 Erik de Castro Lopo <erikd@mega-nerd.com>\nAll rights reserved.");
    ImGui::TextLinkOpenURL("https://libsndfile.github.io/libsndfile/");
    
    ImGui::SeparatorText("libsamplerate");
    ImGui::TextUnformatted("Copyright (C) 2012-2016, Erik de Castro Lopo <erikd@mega-nerd.com>\nAll rights reserved.");
    ImGui::TextLinkOpenURL("https://libsndfile.github.io/libsamplerate/");
    
    ImGui::SeparatorText("FLAC");
    ImGui::TextUnformatted("Copyright (C) 2011-2024 Xiph.Org Foundation");
    ImGui::TextLinkOpenURL("https://xiph.org/flac/");
    
    ImGui::SeparatorText("Opus");
    ImGui::TextUnformatted("Copyright (C) 2001-2023 Xiph.Org, Skype Limited, Octasic,\n"
                           "                    Jean-Marc Valin, Timothy B. Terriberry,\n"
                           "                    CSIRO, Gregory Maxwell, Mark Borgerding,\n"
                           "                    Erik de Castro Lopo, Mozilla, Amazon");
    ImGui::TextLinkOpenURL("https://opus-codec.org/");
    
    ImGui::SeparatorText("OGG");
    ImGui::TextUnformatted("Copyright (C) 2002, Xiph.org Foundation");
    ImGui::TextLinkOpenURL("https://www.xiph.org/ogg/");
    
    ImGui::SeparatorText("libmp3lame");
    ImGui::TextUnformatted("Copyright (C) 1999 Mark Taylor");
    ImGui::TextLinkOpenURL("https://www.mp3dev.org/");
    
    ImGui::SeparatorText("Vorbis");
    ImGui::TextUnformatted("Copyright (C) 2002-2020 Xiph.org Foundation");
    ImGui::TextLinkOpenURL("https://xiph.org/vorbis/");
    
    ImGui::SeparatorText("mpg123");
    ImGui::TextUnformatted("Copyright (C) 1995-2020 by Michael Hipp and others, "
                           "free software under the terms of the LGPL v2.1");
    ImGui::TextLinkOpenURL("https://mpg123.de/");
    
    ImGui::SeparatorText("FreeType");
    ImGui::TextUnformatted("Copyright (C) 1996-2002, 2006 by "
                           "David Turner, Robert Wilhelm, and Werner Lemberg");
    ImGui::TextLinkOpenURL("https://freetype.org/");
    
    ImGui::SeparatorText("Brotli");
    ImGui::TextUnformatted("Copyright (C) 2009, 2010, 2013-2016 by the Brotli Authors.");
    ImGui::TextLinkOpenURL("https://www.brotli.org/");
    
    ImGui::SeparatorText("libpng");
    ImGui::TextUnformatted("Copyright (C) 1995-2024 The PNG Reference Library Authors.");
    ImGui::TextLinkOpenURL("http://www.libpng.org/pub/png/libpng.html");
    
    ImGui::SeparatorText("TagLib");
    ImGui::TextUnformatted("Copyright (C) 2002 - 2008 by Scott Wheeler");
    ImGui::TextLinkOpenURL("https://taglib.org/");
    
    ImGui::SeparatorText("zlib");
    ImGui::TextUnformatted("Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler");
    ImGui::TextLinkOpenURL("https://www.zlib.net/");
    
    ImGui::SeparatorText("bzip2");
    ImGui::TextUnformatted("Copyright (C) 1996-2019 Julian R Seward");
    ImGui::TextLinkOpenURL("https://sourceware.org/bzip2/");

    ImGui::SeparatorText("KISS FFT");
    ImGui::TextUnformatted("Copyright (c) 2003-2010 Mark Borgerding . All rights reserved.");
    ImGui::TextLinkOpenURL("https://github.com/mborgerding/kissfft");
}
