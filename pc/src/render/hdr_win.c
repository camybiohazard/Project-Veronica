/* HDR output on Windows (hdr.h): Direct3D 11 presents what OpenGL drew.

   One FP16 D3D11 texture the size of the window is registered with OpenGL (WGL_NV_DX_interop2,
   offered by the NVIDIA, AMD and Intel drivers) and attached to a GL framebuffer. Each image:
   lock it for GL, gs_gl.c draws the composed scRGB image into it, unlock (the interop orders the
   GL work before D3D's), copy it to the swapchain's back buffer, present. The swapchain is
   flip-model FP16 in DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 (scRGB): Windows maps it to the
   display, HDR or not. The window has no OpenGL pixel format (host.c: the GL context lives on a
   hidden window), so nothing else presents to it. No shaders on the D3D side. */
#ifdef _WIN32
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <SDL3/SDL.h>
#include "hdr.h"
#include "gl_loader.h"

void host_log(const char* fmt, ...);

#define GL_VENDOR 0x1F00
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002
typedef HANDLE (WINAPI *PFN_DXOpenDevice)(void* dx_device);
typedef BOOL (WINAPI *PFN_DXCloseDevice)(HANDLE dev);
typedef HANDLE (WINAPI *PFN_DXRegisterObject)(HANDLE dev, void* dx_object, GLuint name, GLenum type, GLenum access);
typedef BOOL (WINAPI *PFN_DXUnregisterObject)(HANDLE dev, HANDLE obj);
typedef BOOL (WINAPI *PFN_DXLockObjects)(HANDLE dev, GLint count, HANDLE* objs);
static struct {
    PFN_DXOpenDevice open; PFN_DXCloseDevice close;
    PFN_DXRegisterObject reg; PFN_DXUnregisterObject unreg;
    PFN_DXLockObjects lock, unlock;
} wgl;

typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(REFIID riid, void** factory);

static int active;
static ID3D11Device* dev;
static ID3D11DeviceContext* ctx;
static IDXGISwapChain1* chain;
static UINT chain_flags;
static int tearing;              /* DXGI_PRESENT_ALLOW_TEARING with vsync off */
static ID3D11Texture2D* tex;     /* the shared image */
static HANDLE gl_dev, gl_obj;    /* interop device, tex registered as gl_tex */
static GLuint gl_tex, gl_fbo;
static int cur_w, cur_h, locked;

/* the D3D adapter of the GL driver's vendor: the interop wants both on the same GPU (two-GPU
   laptops); NULL = the default */
static IDXGIAdapter1* gl_adapter(IDXGIFactory1* f)
{
    const char* v = (const char*)glGetString(GL_VENDOR);
    UINT want = !v ? 0 : SDL_strstr(v, "NVIDIA") ? 0x10DE : (SDL_strstr(v, "ATI") || SDL_strstr(v, "AMD")) ? 0x1002 : SDL_strstr(v, "Intel") ? 0x8086 : 0;
    IDXGIAdapter1* a;
    for (UINT i = 0; want && IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK; i++) {
        DXGI_ADAPTER_DESC1 d;
        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &d)) && d.VendorId == want && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) return a;
        IDXGIAdapter1_Release(a);
    }
    return NULL;
}

static void release_image(void)
{
    if (gl_obj) { wgl.unreg(gl_dev, gl_obj); gl_obj = NULL; }
    if (tex) { ID3D11Texture2D_Release(tex); tex = NULL; }
    cur_w = cur_h = 0;
}

/* the shared texture (and the swapchain's buffers) at w x h */
static int make_image(int w, int h)
{
    D3D11_TEXTURE2D_DESC td = { 0 };
    HRESULT hr;
    release_image();
    hr = IDXGISwapChain1_ResizeBuffers(chain, 0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, chain_flags);
    if (FAILED(hr)) { host_log("hdr: ResizeBuffers %d x %d failed (0x%08lx)\n", w, h, (unsigned long)hr); return 0; }
    td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    hr = ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex);
    if (FAILED(hr)) { host_log("hdr: CreateTexture2D %d x %d failed (0x%08lx)\n", w, h, (unsigned long)hr); return 0; }
    if (gl_tex) glDeleteTextures(1, &gl_tex);
    glGenTextures(1, &gl_tex);
    gl_obj = wgl.reg(gl_dev, tex, gl_tex, GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
    if (!gl_obj) { host_log("hdr: wglDXRegisterObjectNV failed (%lu)\n", GetLastError()); release_image(); return 0; }
    if (!gl_fbo) glGenFramebuffers(1, &gl_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    cur_w = w; cur_h = h;
    return 1;
}

static void release_all(void)
{
    release_image();
    if (gl_fbo) { glDeleteFramebuffers(1, &gl_fbo); gl_fbo = 0; }
    if (gl_tex) { glDeleteTextures(1, &gl_tex); gl_tex = 0; }
    if (gl_dev) { wgl.close(gl_dev); gl_dev = NULL; }
    if (chain) { IDXGISwapChain1_Release(chain); chain = NULL; }
    if (ctx) { ID3D11DeviceContext_Release(ctx); ctx = NULL; }
    if (dev) { ID3D11Device_Release(dev); dev = NULL; }
    active = 0;
}

int hdr_init(void* sdl_window)
{
    SDL_Window* win = (SDL_Window*)sdl_window;
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(win), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    SDL_SharedObject* d3d11 = SDL_LoadObject("d3d11.dll");
    SDL_SharedObject* dxgi = SDL_LoadObject("dxgi.dll");
    PFN_D3D11_CREATE_DEVICE create = d3d11 ? (PFN_D3D11_CREATE_DEVICE)SDL_LoadFunction(d3d11, "D3D11CreateDevice") : NULL;
    PFN_CreateDXGIFactory1 factory1 = dxgi ? (PFN_CreateDXGIFactory1)SDL_LoadFunction(dxgi, "CreateDXGIFactory1") : NULL;
    IDXGIFactory1* f1 = NULL;
    IDXGIFactory2* f2 = NULL;
    IDXGIFactory5* f5 = NULL;
    IDXGIAdapter1* adapter = NULL;
    IDXGIDevice1* xdev = NULL;
    IDXGISwapChain3* chain3 = NULL;
    DXGI_SWAP_CHAIN_DESC1 sd = { 0 };
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr;
    int w, h;
    const char* why = NULL;

    wgl.open = (PFN_DXOpenDevice)SDL_GL_GetProcAddress("wglDXOpenDeviceNV");
    wgl.close = (PFN_DXCloseDevice)SDL_GL_GetProcAddress("wglDXCloseDeviceNV");
    wgl.reg = (PFN_DXRegisterObject)SDL_GL_GetProcAddress("wglDXRegisterObjectNV");
    wgl.unreg = (PFN_DXUnregisterObject)SDL_GL_GetProcAddress("wglDXUnregisterObjectNV");
    wgl.lock = (PFN_DXLockObjects)SDL_GL_GetProcAddress("wglDXLockObjectsNV");
    wgl.unlock = (PFN_DXLockObjects)SDL_GL_GetProcAddress("wglDXUnlockObjectsNV");
    if (!hwnd) why = "no window handle";
    else if (!wgl.open || !wgl.close || !wgl.reg || !wgl.unreg || !wgl.lock || !wgl.unlock) why = "the OpenGL driver has no WGL_NV_DX_interop2";
    else if (!create || !factory1) why = "no d3d11.dll / dxgi.dll";
    else if (FAILED(factory1(&IID_IDXGIFactory1, (void**)&f1))) why = "CreateDXGIFactory1 failed";
    if (why) goto fail;

    adapter = gl_adapter(f1);
    hr = create((IDXGIAdapter*)adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &fl, 1, D3D11_SDK_VERSION, &dev, NULL, &ctx);
    if (FAILED(hr)) { why = "D3D11CreateDevice failed"; goto fail; }
    if (SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice1, (void**)&xdev))) {
        IDXGIDevice1_SetMaximumFrameLatency(xdev, 1);   /* as the GL path: at most one image queued */
        IDXGIDevice1_Release(xdev);
    }
    if (FAILED(IDXGIFactory1_QueryInterface(f1, &IID_IDXGIFactory2, (void**)&f2))) { why = "no DXGI 1.2"; goto fail; }
    if (SUCCEEDED(IDXGIFactory1_QueryInterface(f1, &IID_IDXGIFactory5, (void**)&f5))) {
        BOOL allow = FALSE;
        tearing = SUCCEEDED(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))) && allow;
        IDXGIFactory5_Release(f5);
    }
    chain_flags = tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    sd.Width = (UINT)(w > 0 ? w : 1); sd.Height = (UINT)(h > 0 ? h : 1);
    sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    sd.Flags = chain_flags;
    hr = IDXGIFactory2_CreateSwapChainForHwnd(f2, (IUnknown*)dev, hwnd, &sd, NULL, NULL, &chain);
    if (FAILED(hr)) { why = "CreateSwapChainForHwnd failed"; goto fail; }
    IDXGIFactory2_MakeWindowAssociation(f2, hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);   /* fullscreen stays settings.c's */
    if (FAILED(IDXGISwapChain1_QueryInterface(chain, &IID_IDXGISwapChain3, (void**)&chain3))) { why = "no DXGI 1.4"; goto fail; }
    {
        UINT support = 0;
        hr = IDXGISwapChain3_CheckColorSpaceSupport(chain3, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, &support);
        if (FAILED(hr) || !(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) ||
            FAILED(IDXGISwapChain3_SetColorSpace1(chain3, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709))) { why = "no scRGB swapchain"; goto fail; }
    }
    IDXGISwapChain3_Release(chain3); chain3 = NULL;
    gl_dev = wgl.open(dev);
    if (!gl_dev) { why = "wglDXOpenDeviceNV failed"; goto fail; }
    if (!make_image((int)sd.Width, (int)sd.Height)) { why = "the shared texture failed"; goto fail; }
    {
        DXGI_ADAPTER_DESC1 d;
        char name[128] = "default adapter";
        if (adapter && SUCCEEDED(IDXGIAdapter1_GetDesc1(adapter, &d))) WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof(name), NULL, NULL);
        host_log("hdr: Direct3D 11 scRGB presentation on %s%s\n", name, tearing ? " (tearing allowed with vsync off)" : "");
    }
    if (adapter) IDXGIAdapter1_Release(adapter);
    IDXGIFactory2_Release(f2);
    IDXGIFactory1_Release(f1);
    active = 1;
    return 1;

fail:
    host_log("hdr: unavailable (%s), SDR output\n", why);
    if (chain3) IDXGISwapChain3_Release(chain3);
    if (adapter) IDXGIAdapter1_Release(adapter);
    if (f2) IDXGIFactory2_Release(f2);
    if (f1) IDXGIFactory1_Release(f1);
    release_all();
    return 0;
}

int hdr_active(void) { return active; }
int hdr_pq(void) { return 0; }
void* hdr_gl_proc(const char* name) { return (void*)SDL_GL_GetProcAddress(name); }

unsigned int hdr_begin(int w, int h)
{
    if (!active || w <= 0 || h <= 0) return 0;
    if ((w != cur_w || h != cur_h) && !make_image(w, h)) return 0;
    if (!wgl.lock(gl_dev, 1, &gl_obj)) { host_log("hdr: wglDXLockObjectsNV failed (%lu)\n", GetLastError()); return 0; }
    locked = 1;
    return gl_fbo;
}

uint64_t hdr_present(int vsync)
{
    ID3D11Texture2D* back = NULL;
    uint64_t t;
    HRESULT hr;
    if (!active) return 0;
    if (locked) { wgl.unlock(gl_dev, 1, &gl_obj); locked = 0; }
    if (tex && SUCCEEDED(IDXGISwapChain1_GetBuffer(chain, 0, &IID_ID3D11Texture2D, (void**)&back))) {
        ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource*)back, (ID3D11Resource*)tex);
        ID3D11Texture2D_Release(back);
    }
    t = SDL_GetTicksNS();
    hr = IDXGISwapChain1_Present(chain, vsync ? 1 : 0, !vsync && tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
    t = SDL_GetTicksNS() - t;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        static int logged;
        if (!logged++) host_log("hdr: the Direct3D device was lost (0x%08lx, reason 0x%08lx)\n", (unsigned long)hr, (unsigned long)ID3D11Device_GetDeviceRemovedReason(dev));
    }
    return t;
}
#endif
