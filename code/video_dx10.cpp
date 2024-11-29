#include <d3d10.h>
#include <dxgi.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx10.h>
#include "defines.h"
#include "platform.h"
#include "video.h"

static ID3D10Device *g_device;
static IDXGISwapChain *g_swapchain;
static ID3D10RenderTargetView *g_render_target;
static bool g_window_is_obscured;

static void create_window_render_target();
static void destroy_window_render_target();

bool video_init(void *hwnd) {
	DXGI_SWAP_CHAIN_DESC swapchain = {};
	swapchain.BufferCount = 2;
	swapchain.BufferDesc.Width = 0;
	swapchain.BufferDesc.Height = 0;
	swapchain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain.BufferDesc.RefreshRate.Numerator = 60;
	swapchain.BufferDesc.RefreshRate.Denominator = 1;
	swapchain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapchain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain.OutputWindow = (HWND)hwnd;
	swapchain.SampleDesc.Count = 1;
	swapchain.SampleDesc.Quality = 0;
	swapchain.Windowed = TRUE;
	swapchain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	int flags = 0;
#ifndef NDEBUG
	flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	HRESULT result = D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL,
		flags, D3D10_SDK_VERSION,
		&swapchain, &g_swapchain, &g_device);

	if (result != S_OK) {
		show_message_box(MESSAGE_BOX_TYPE_ERROR, "Graphics device does not support DirectX10");
		return false;
	}

	create_window_render_target();

	return true;
}

void video_init_imgui(void *hwnd) {
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX10_Init(g_device);
}

void video_deinit() {
	ImGui_ImplWin32_Shutdown();
	ImGui_ImplDX10_Shutdown();

	destroy_window_render_target();
	if (g_swapchain) g_swapchain->Release();
	if (g_device) g_device->Release();
}

void video_invalidate_imgui_objects() {
	ImGui_ImplDX10_InvalidateDeviceObjects();
}

void video_create_imgui_objects() {
	ImGui_ImplDX10_CreateDeviceObjects();
}

bool video_begin_frame() {
	// Clear buffer
	const float clear_color[4] = {0.f, 0.f, 0.f, 1.f};
	g_device->OMSetRenderTargets(1, &g_render_target, NULL);
	g_device->ClearRenderTargetView(g_render_target, clear_color);

	ImGui_ImplDX10_NewFrame();
	ImGui_ImplWin32_NewFrame();

	return true;
}

bool video_end_frame() {
	// Render ImGui state
	ImDrawData *draw_data = ImGui::GetDrawData();
	if (draw_data) {
		ImGui_ImplDX10_RenderDrawData(draw_data);
	}

	// Present
	return g_swapchain->Present(1, 0) != DXGI_STATUS_OCCLUDED;
}

void video_resize_window(int width, int height) {
	destroy_window_render_target();
	g_swapchain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, 0);
	create_window_render_target();
}

static DXGI_FORMAT image_format_to_dxgi(int format) {
	switch (format) {
	case IMAGE_FORMAT_R8G8B8A8: return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	ASSERT(false && "Unknown image format");
	return DXGI_FORMAT_UNKNOWN;
}

static int image_format_bytes_per_pixel(int format) {
	switch (format) {
	case IMAGE_FORMAT_R8G8B8A8: return 4;
	}

	ASSERT(false && "Unknown image format");
	return 0;
}

Texture *create_texture_from_image(Image *image) {
	ID3D10ShaderResourceView *view;
	ID3D10Texture2D *texture;

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = image->width;
	desc.Height = image->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = image_format_to_dxgi(image->format);
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D10_USAGE_IMMUTABLE;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

	D3D10_SUBRESOURCE_DATA data = {};
	data.pSysMem = image->data;
	data.SysMemPitch = image->width * image_format_bytes_per_pixel(image->format);

	g_device->CreateTexture2D(&desc, &data, &texture);
	if (!texture) {
		return NULL;
	}
	defer(texture->Release());

	D3D10_SHADER_RESOURCE_VIEW_DESC sr = {};
	sr.Format = desc.Format;
	sr.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	sr.Texture2D.MipLevels = 1;
	g_device->CreateShaderResourceView(texture, &sr, &view);

	return view;
}

void destroy_texture(Texture **texture) {
	if (*texture) ((ID3D10ShaderResourceView*)*texture)->Release();
	*texture = NULL;
}

static void create_window_render_target() {
	ID3D10Texture2D *texture;
	g_swapchain->GetBuffer(0, IID_PPV_ARGS(&texture));
	if (!texture) return;
	g_device->CreateRenderTargetView(texture, NULL, &g_render_target);
	texture->Release();
}

static void destroy_window_render_target() {
	if (g_render_target) {
		g_render_target->Release();
		g_render_target = NULL;
	}
}

