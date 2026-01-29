#include "Graphics.h"
#include "resource.h"
#include "logger.h"
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxguid.lib")

IDXGISwapChain* swapchain = NULL;
ID3D11Device* device = NULL;
ID3D11DeviceContext* device_ctx = NULL;
ID3D11RenderTargetView* render_target = NULL;

ID3D11VertexShader* vertex_shader = NULL;
ID3D11VertexShader* pixel_shader = NULL;

ID3D11InputLayout* input_layout = NULL;

ID3D11Buffer* vertex_buffer = NULL;

UINT32 framebuffer[240][256];
ID3D11Texture2D* framebuffer_texture = NULL;
ID3D11ShaderResourceView* framebuffer_view = NULL;
ID3D11SamplerState* sample_state = NULL;

static const void* load_rcdata(int id,DWORD* outSize)
{
	HMODULE mod = GetModuleHandleW(NULL);
	HRSRC r = FindResourceW(mod, MAKEINTRESOURCEW(id), RT_RCDATA);
	if (!r) return NULL;

	HGLOBAL h = LoadResource(mod, r);
	if (!h) return NULL;

	DWORD sz = SizeofResource(mod, r);
	const void* p = LockResource(h);
	if (!p || !sz) return NULL;

	if (outSize) *outSize = sz;
	return p;
}

int create_graphics_for_window(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC sd = {0};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = 640;
	sd.BufferDesc.Height = 480;
	sd.BufferDesc.RefreshRate.Denominator = 0;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.Windowed = 1;
	sd.Flags = 0;
	sd.OutputWindow = hwnd;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;

	UINT flags = 0;
	#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif

	if (D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		flags,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&swapchain,
		&device,
		NULL,
		&device_ctx
	) < 0) return -1;

	ID3D11Resource* back_buffer;
	swapchain->lpVtbl->GetBuffer(swapchain, 0, &IID_ID3D11Resource,&back_buffer);
	device->lpVtbl->CreateRenderTargetView(device, back_buffer, NULL, &render_target);
	back_buffer->lpVtbl->Release(back_buffer);

	//setup the pipeline
	device_ctx->lpVtbl->IASetPrimitiveTopology(device_ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DWORD vs_size = 0, ps_size = 0;
	const void* vs_byte_code = load_rcdata(IDR_VERTEX_SHADER1, &vs_size);
	const void* ps_byte_code = load_rcdata(IDR_PIXEL_SHADER1, &ps_size);

	D3D11_INPUT_ELEMENT_DESC element_descriptor[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	device->lpVtbl->CreateInputLayout(device, element_descriptor, 2, vs_byte_code, vs_size, &input_layout);
	device_ctx->lpVtbl->IASetInputLayout(device_ctx, input_layout);
	
	typedef struct 
	{
		float x; float y;
		float tx; float ty;
	}Vert;

	Vert quad[] = {
		{-1.0f,-1.0f, 1.0f, 1.0f},
		{-1.0f, 1.0f, 1.0f, 0.0f},
		{ 1.0f,-1.0f, 0.0f, 1.0f},

		{ 1.0f,-1.0f, 0.0f, 1.0f},
		{-1.0f, 1.0f, 1.0f, 0.0f},
		{ 1.0f, 1.0f, 0.0f, 0.0f},
	};

	D3D11_BUFFER_DESC bd = {
		.ByteWidth = sizeof(quad),
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_VERTEX_BUFFER,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
		.StructureByteStride = sizeof(Vert),
	};

	D3D11_SUBRESOURCE_DATA srd =
	{
		.pSysMem = quad,
		.SysMemPitch = 0,
		.SysMemSlicePitch = 0,
	};

	device->lpVtbl->CreateBuffer(device, &bd, &srd, &vertex_buffer);

	UINT stride = sizeof(Vert);
	UINT offset = 0;
	device_ctx->lpVtbl->IASetVertexBuffers(device_ctx, 0, 1, &vertex_buffer, &stride, &offset);

	device->lpVtbl->CreateVertexShader(device, vs_byte_code, vs_size, NULL,&vertex_shader);
	device->lpVtbl->CreatePixelShader(device, ps_byte_code, ps_size, NULL, &pixel_shader);

	device_ctx->lpVtbl->VSSetShader(device_ctx, vertex_shader, NULL, 0);
	device_ctx->lpVtbl->PSSetShader(device_ctx, pixel_shader, NULL, 0);

	int ww = 640;
	int tw = 256;

	int wh = 480;
	int th = 240;

	float scale = min((float)ww / tw, (float)wh / th);
	float drawW = tw * scale;
	float drawH = th * scale;

	D3D11_VIEWPORT vp = {
		.TopLeftX = (ww - drawW) * 0.5f,
		.TopLeftY = (wh - drawH) * 0.5f,
		.Width = drawW,
		.Height = drawH,
		.MaxDepth = 1,
		.MinDepth = 0,
	};

	device_ctx->lpVtbl->RSSetViewports(device_ctx, 1, &vp);

	device_ctx->lpVtbl->OMSetRenderTargets(device_ctx, 1, &render_target, NULL);

	//setup texture and apply to pipeline
	for (int y = 0 ; y < 240; y++)
	{
		for(int x = 0; x < 256; x++)
		{
			framebuffer[y][x] = 0xFF000000;
		}
	}

	D3D11_SUBRESOURCE_DATA texture = {
		.pSysMem = framebuffer,
		.SysMemPitch = sizeof(UINT32) * 256,
	};

	D3D11_TEXTURE2D_DESC td = {
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Width = 256,
		.Height = 240,
		.ArraySize = 1,
		.MipLevels = 1,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE,
		.SampleDesc.Count = 1,
		.SampleDesc.Quality = 0,
		.Usage = D3D11_USAGE_DEFAULT,
		.MiscFlags = 0,
	};

	device->lpVtbl->CreateTexture2D(device, &td, &texture, &framebuffer_texture);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {
		.Format = td.Format,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D.MipLevels = 1,
		.Texture2D.MostDetailedMip = 0,
	};
	device->lpVtbl->CreateShaderResourceView(device, framebuffer_texture, &srvd, &framebuffer_view);


	D3D11_SAMPLER_DESC tsd = { 0 };
	tsd.AddressU = tsd.AddressV = tsd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	tsd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	tsd.MinLOD = 0;
	tsd.MaxLOD = D3D11_FLOAT32_MAX;

	device->lpVtbl->CreateSamplerState(device, &tsd, &sample_state);

	device_ctx->lpVtbl->PSSetSamplers(device_ctx, 0, 1, &sample_state);
	device_ctx->lpVtbl->PSSetShaderResources(device_ctx, 0, 1, &framebuffer_view);
}

const float colour[4] = {0.0f,0.0f,0.0f,1.0f};
void update_window_graphics()
{
	device_ctx->lpVtbl->UpdateSubresource(device_ctx, framebuffer_texture, 0, NULL, framebuffer, 256 * sizeof(UINT32), 0);

	device_ctx->lpVtbl->ClearRenderTargetView(device_ctx, render_target, colour);
	device_ctx->lpVtbl->Draw(device_ctx, 6, 0);
	swapchain->lpVtbl->Present(swapchain, 0, 0);
}

void set_pixel(int x, int y, UINT32 colour)
{
	if ( x >= 0 && y >= 0 && x <= 256 && y <= 240)
	{
		framebuffer[y][x] = 0xFF000000 | (colour & 0xFFFFFF);
	}
}

void delete_graphics()
{
	if (swapchain)
	{
		swapchain->lpVtbl->Release(swapchain);
		swapchain = NULL;
	}
	if (device)
	{
		device->lpVtbl->Release(device);
		device = NULL;
	}
	if (device_ctx)
	{
		device_ctx->lpVtbl->Release(device_ctx);
		device_ctx = NULL;
	}
	if (render_target)
	{
		render_target->lpVtbl->Release(render_target);
		render_target = NULL;
	}
	if (vertex_shader)
	{
		vertex_shader->lpVtbl->Release(vertex_shader);
		vertex_shader = NULL;
	}
	if (pixel_shader)
	{
		pixel_shader->lpVtbl->Release(pixel_shader);
		pixel_shader = NULL;
	}
	if (input_layout)
	{
		input_layout->lpVtbl->Release(input_layout);
		input_layout = NULL;
	}
	if (vertex_buffer)
	{
		vertex_buffer->lpVtbl->Release(vertex_buffer);
		vertex_buffer = NULL;
	}
	if (framebuffer_texture)
	{
		framebuffer_texture->lpVtbl->Release(framebuffer_texture);
		framebuffer_texture = NULL;
	}
	if (framebuffer_view)
	{
		framebuffer_view->lpVtbl->Release(framebuffer_view);
		framebuffer_view = NULL;
	}
	if (sample_state)
	{
		sample_state->lpVtbl->Release(sample_state);
		sample_state = NULL;
	}
}