#pragma comment(lib, "windowsapp")
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Media.h>
#include <winrt/windows.applicationmodel.h>
#include <winrt/windows.foundation.collections.h>
#include "media_controls.h"
#include "playback.h"
#include "playlist.h"
#include "main.h"

using namespace winrt::Windows::Media;
using namespace winrt::Windows::Foundation;
static SystemMediaTransportControls g_smtc(nullptr);

static void handle_button_pressed(SystemMediaTransportControls sender,
    SystemMediaTransportControlsButtonPressedEventArgs args) {
    switch (args.Button()) {
    case SystemMediaTransportControlsButton::Pause:
        notify(NOTIFY_PAUSE);
        break;
    case SystemMediaTransportControlsButton::Play:
        notify(NOTIFY_PLAY);
        break;
    case SystemMediaTransportControlsButton::Next:
        notify(NOTIFY_NEXT_TRACK);
        break;
    case SystemMediaTransportControlsButton::Previous:
        notify(NOTIFY_PREV_TRACK);
        break;
    }
}

void update_media_controls_state() {
    Playback_State playback_state = get_playback_state();

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
    retrieve_metadata(track.metadata, &md);

    auto updater = g_smtc.DisplayUpdater();
    updater.Type(MediaPlaybackType::Music);
   
    // Strings need to be converted to utf16
    wchar_t str_buf[128];
    utf8_to_wchar(md.artist, str_buf, LENGTH_OF_ARRAY(str_buf));
    updater.MusicProperties().Artist(str_buf);
    utf8_to_wchar(md.album, str_buf, LENGTH_OF_ARRAY(str_buf));
    updater.MusicProperties().AlbumTitle(str_buf);
    utf8_to_wchar(md.title, str_buf, LENGTH_OF_ARRAY(str_buf));
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