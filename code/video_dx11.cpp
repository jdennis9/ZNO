#include <d3d11.h>
#include <dxgi.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include "defines.h"
#include "platform.h"
#include "video.h"

static ID3D11Device *g_device;
static ID3D11DeviceContext *g_context;
static IDXGISwapChain *g_swapchain;
static ID3D11RenderTargetView *g_render_target;
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
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL feature_levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL feature_level;

	HRESULT result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		flags, feature_levels, ARRAY_LENGTH(feature_levels), D3D11_SDK_VERSION,
		&swapchain, &g_swapchain, &g_device, &feature_level, &g_context);

	if (result != S_OK) {
		show_message_box(MESSAGE_BOX_TYPE_ERROR, "Graphics device does not support DirectX10");
		return false;
	}

	create_window_render_target();

	return true;
}

void video_init_imgui(void *hwnd) {
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_device, g_context);
}

void video_deinit() {
	ImGui_ImplWin32_Shutdown();
	ImGui_ImplDX11_Shutdown();

	destroy_window_render_target();
	if (g_swapchain) g_swapchain->Release();
	if (g_device) g_device->Release();
}

void video_invalidate_imgui_objects() {
	ImGui_ImplDX11_InvalidateDeviceObjects();
}

void video_create_imgui_objects() {
	ImGui_ImplDX11_CreateDeviceObjects();
}

bool video_begin_frame() {
	// Clear buffer
	const float clear_color[4] = {0.f, 0.f, 0.f, 1.f};
	g_context->OMSetRenderTargets(1, &g_render_target, NULL);
	g_context->ClearRenderTargetView(g_render_target, clear_color);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	return true;
}

bool video_end_frame() {
	// Render ImGui state
	ImDrawData *draw_data = ImGui::GetDrawData();
	if (draw_data) {
		ImGui_ImplDX11_RenderDrawData(draw_data);
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
	// Staging texture where we write the image data to
	ID3D11Texture2D *texture = NULL;
	// Staging texture gets copied to this texture and
	// a shader resource view for this is returned
	ID3D11Texture2D *dst_texture = NULL;
	ID3D11ShaderResourceView *view = NULL;

	// We use a separate texture for writing the image data
	// because for some reason passing the image data to CreateTexture2D
	// causes CPU memory usage to sky rocket and never go down.

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = image->width;
	desc.Height = image->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	// Create staging texture
	g_device->CreateTexture2D(&desc, NULL, &texture);
	if (!texture) return NULL;
	defer(texture->Release());

	// Create texture used in shader
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	g_device->CreateTexture2D(&desc, NULL, &dst_texture);
	if (!dst_texture) return NULL;
	defer(dst_texture->Release());

	D3D11_MAPPED_SUBRESOURCE mapped;
	g_context->Map(texture, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	u8 *out = (u8*)mapped.pData;
	u8 *in = image->data;

	for (i32 row = 0; row < image->height; ++row) {
		i32 row_offset = row * mapped.RowPitch;
		for (i32 col = 0; col < image->width; ++col) {
			i32 col_offset = col * 4;
			out[row_offset+col_offset+0] = in[0];
			out[row_offset+col_offset+1] = in[1];
			out[row_offset+col_offset+2] = in[2];
			out[row_offset+col_offset+3] = in[3];

			in += 4;
		}
	}

	g_context->Unmap(texture, D3D11CalcSubresource(0, 0, 1));
	g_context->CopyResource(dst_texture, texture);

	D3D11_SHADER_RESOURCE_VIEW_DESC sr = {};
	sr.Format = desc.Format;
	sr.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	sr.Texture2D.MipLevels = 1;
	g_device->CreateShaderResourceView(dst_texture, &sr, &view);

	return view;
}

void destroy_texture(Texture **texture) {
	if (*texture) ((ID3D11ShaderResourceView*)*texture)->Release();
	*texture = NULL;
}

static void create_window_render_target() {
	ID3D11Texture2D *texture;
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

