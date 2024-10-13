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
#include "audio.h"
#include "platform.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <atlbase.h>

struct WASAPI_Instance {
    HANDLE ready_semaphore;
    HANDLE interrupt_semaphore;
    HANDLE thread;
    int channel_count;
    int sample_rate;
    u64 latency_ms;
    i32 buffer_duration_ms;
    IAudioStreamVolume *volume_controller;
    CComPtr<IAudioMeterInformation> meter;
    Fill_Audio_Buffer_Callback *callback;
    void *callback_data;
    bool want_close;
};

#define CHECK(code) {\
HRESULT error_ = code;\
if (error_ != S_OK) {\
log_debug("%s return 0x%x\n", #code, (u32)error_);\
show_last_error_in_message_box(L ## #code);\
}\
}
#define SAFE_RELEASE(obj) if (obj) { (obj)->Release(); (obj) = nullptr; }

static IMMDeviceEnumerator *g_device_enumerator;

static DWORD audio_thread_entry(LPVOID user_data) {
	WAVEFORMATEX *format;
	u32 buffer_frame_count;
	IMMDevice *device;
	IAudioClient *audio_client;
	IAudioRenderClient *render_client;
	u8 *buffer;
	WASAPI_Instance *instance = (WASAPI_Instance*)user_data;
    Audio_Buffer_Spec buffer_spec = {};
	DWORD buffer_duration_ms;
    
    (void)CoInitialize(NULL);
    
	{
		// @Note: stream is invalidated after ready semaphore is signaled
		g_device_enumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &device);
		device->Activate(__uuidof(audio_client), CLSCTX_ALL, NULL, (void **)&audio_client);
		audio_client->GetMixFormat(&format);
        
		audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, (REFERENCE_TIME)1e7, 0, format, NULL);
		audio_client->GetBufferSize(&buffer_frame_count);
		audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&render_client);
		audio_client->GetService(__uuidof(IAudioStreamVolume), (void**)&instance->volume_controller);
		render_client->GetBuffer(buffer_frame_count, &buffer);
		render_client->ReleaseBuffer(buffer_frame_count, 0);
        
        buffer_duration_ms = (format->nSamplesPerSec*1000) / buffer_frame_count;
        
        buffer_spec.channel_count = format->nChannels;
		buffer_spec.sample_rate = format->nSamplesPerSec;
        
        instance->channel_count = buffer_spec.channel_count;
        instance->sample_rate = buffer_spec.sample_rate;
		instance->buffer_duration_ms = buffer_duration_ms;
        
		// Device is ready for streaming
		ReleaseSemaphore(instance->ready_semaphore, 1, NULL);
	}
    
    log_debug("WASAPI ready. Buffer length = %dms\n", buffer_duration_ms);
    
	audio_client->Start();
	while (1) {
		u32 frame_padding;
		u32 available_frames = 0;
		u32 capture_packet_size = 0;
        
		// Wait for half of buffer duration, or handle interrupt signal.
		if (WaitForSingleObject(instance->interrupt_semaphore, buffer_duration_ms/2) 
            != WAIT_TIMEOUT) {	
			// Upon an interruption, stop the stream and reset the audio clock
			audio_client->Stop();
			audio_client->Reset();
			audio_client->Start();
		}
        
		if (instance->want_close) break;
        
		audio_client->GetCurrentPadding(&frame_padding);
		available_frames = buffer_frame_count - frame_padding;
        
		render_client->GetBuffer(available_frames, &buffer);
        buffer_spec.frame_count = available_frames;
        instance->callback(instance->callback_data, (f32*)buffer, &buffer_spec);
		render_client->ReleaseBuffer(available_frames, 0);
	}
    
	ReleaseSemaphore(instance->ready_semaphore, 1, 0);
	CoTaskMemFree(format);
	CloseHandle(instance->ready_semaphore);
	SAFE_RELEASE(render_client);
	SAFE_RELEASE(instance->volume_controller);
	SAFE_RELEASE(audio_client);
	SAFE_RELEASE(device);
    
	return 0;
}

static void wasapi_interrupt(void *data) {
    WASAPI_Instance *instance = (WASAPI_Instance*)data;
    ReleaseSemaphore(instance->interrupt_semaphore, 1, 0);
}

static void wasapi_set_volume(void *data, float volume) {
    WASAPI_Instance *instance = (WASAPI_Instance*)data;
    float volumes[8] = {volume, volume, volume, volume, volume, volume, volume, volume};
    if (instance->volume_controller)
        instance->volume_controller->SetAllVolumes(instance->channel_count, volumes);
}

static float wasapi_get_volume(void *data) {
    WASAPI_Instance *instance = (WASAPI_Instance*)data;
    float volume = 0.f;
    if (instance->volume_controller)
        instance->volume_controller->GetChannelVolume(0, &volume);
    return volume;
}

bool open_wasapi_audio_stream(Fill_Audio_Buffer_Callback *callback, void *callback_data, Audio_Stream *stream) {
    if (!g_device_enumerator) {
        HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
                                          __uuidof(IMMDeviceEnumerator), (void**)&g_device_enumerator);
        if (result) {
            log_error("WASAPI failed to create device enumerator with error code: %d\n", result);
            return false;
        }
        
    }
    
    WASAPI_Instance *instance = new WASAPI_Instance;
    *instance = WASAPI_Instance{};
    instance->interrupt_semaphore = CreateSemaphore(NULL, 0, 1, NULL);
    instance->ready_semaphore = CreateSemaphore(NULL, 0, 1, NULL);
    instance->thread = CreateThread(NULL, 0, &audio_thread_entry, instance, 0, NULL);
    instance->callback = callback;
    instance->callback_data = callback_data;
    instance->want_close = false;
    WaitForSingleObject(instance->ready_semaphore, INFINITE);
    
    stream->data = instance;
    stream->sample_rate = instance->sample_rate;
    stream->channel_count = instance->channel_count;
    stream->latency_ms = (i32)instance->latency_ms;
    stream->buffer_duration_ms = instance->buffer_duration_ms;
    stream->interrupt_fn = &wasapi_interrupt;
    stream->set_volume_fn = &wasapi_set_volume;
    stream->get_volume_fn = &wasapi_get_volume;
    
    return true;
}
    
    