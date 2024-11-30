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
#pragma comment(lib, "windowsapp")
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Media.h>
#include <winrt/windows.applicationmodel.h>
#include <winrt/windows.foundation.collections.h>
#include "media_controls.h"
#include "playback.h"
#include "library.h"
#include "main.h"

using namespace winrt::Windows::Media;
using namespace winrt::Windows::Foundation;
static SystemMediaTransportControls g_smtc(nullptr);

static void handle_button_pressed(SystemMediaTransportControls sender,
    SystemMediaTransportControlsButtonPressedEventArgs args) {
    switch (args.Button()) {
    case SystemMediaTransportControlsButton::Pause:
        notify(NOTIFY_REQUEST_PAUSE);
        break;
    case SystemMediaTransportControlsButton::Play:
        notify(NOTIFY_REQUEST_PLAY);
        break;
    case SystemMediaTransportControlsButton::Next:
        notify(NOTIFY_REQUEST_NEXT_TRACK);
        break;
    case SystemMediaTransportControlsButton::Previous:
        notify(NOTIFY_REQUEST_PREV_TRACK);
        break;
    }
}

void update_media_controls_state() {
    Playback_State playback_state = playback_get_state();

    switch (playback_state) {
    case PLAYBACK_STATE_PAUSED:
        g_smtc.PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
    case PLAYBACK_STATE_PLAYING:
        g_smtc.PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
    default:
        g_smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
        break;
    }
}

void update_media_controls_metadata(Track track) {
    Metadata md;
    library_get_track_metadata(track, &md);

    auto updater = g_smtc.DisplayUpdater();
    updater.Type(MediaPlaybackType::Music);
   
    // Strings need to be converted to utf16
    wchar_t str_buf[128];
    utf8_to_wchar(md.artist, str_buf, ARRAY_LENGTH(str_buf));
    updater.MusicProperties().Artist(str_buf);
    utf8_to_wchar(md.album, str_buf, ARRAY_LENGTH(str_buf));
    updater.MusicProperties().AlbumTitle(str_buf);
    utf8_to_wchar(md.title, str_buf, ARRAY_LENGTH(str_buf));
    updater.MusicProperties().Title(str_buf);
    updater.Update();
}

void install_media_controls_handler() {
    log_debug("Installing SystemMediaTransportControls handler...\n");
    g_smtc = Playback::BackgroundMediaPlayer::Current().SystemMediaTransportControls();
    g_smtc.IsPlayEnabled(true);
    g_smtc.IsPauseEnabled(true);
    g_smtc.IsNextEnabled(true);
    g_smtc.IsPreviousEnabled(true);

    g_smtc.ButtonPressed(&handle_button_pressed);
}
