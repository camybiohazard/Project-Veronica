/* HDR output: the OpenGL renderer stays; its final image (composed by gs_gl.c gs_present) is drawn
   into an FP16 texture shared with another API, which presents it on an HDR swapchain. OpenGL
   alone cannot say a window holds HDR.
   - Windows (hdr_win.c): Direct3D 11 (WGL_NV_DX_interop2), FP16 flip-model swapchain in scRGB
     (linear BT.709, 1.0 = 80 nits).
   - Linux (hdr_vk.c): Vulkan (GL_EXT_memory_object_fd / GL_EXT_semaphore_fd), a swapchain in
     HDR10 (PQ, BT.2020; the texture then holds PQ values) or else scRGB; the Vulkan driver tells
     the Wayland compositor (color-management-v1).
   Elsewhere: never active. */
#pragma once
#include <stdint.h>

#if defined(_WIN32) || defined(__linux__)
int  hdr_init(void* sdl_window);   /* main thread, GL context current; 1: active (else everything is released) */
int  hdr_active(void);
int  hdr_pq(void);                 /* the shared texture wants PQ (HDR10), else scRGB */
unsigned int hdr_begin(int w, int h);   /* the GL framebuffer of the shared texture (w x h), 0 = none this image */
uint64_t hdr_present(int vsync);   /* hand it over, present; ns the present blocked */
void* hdr_gl_proc(const char* name);   /* a GL entry point of the current context (SDL's, or the headless one) */
#else
static inline int hdr_init(void* sdl_window) { (void)sdl_window; return 0; }
static inline int hdr_active(void) { return 0; }
static inline int hdr_pq(void) { return 0; }
static inline unsigned int hdr_begin(int w, int h) { (void)w; (void)h; return 0; }
static inline uint64_t hdr_present(int vsync) { (void)vsync; return 0; }
static inline void* hdr_gl_proc(const char* name) { return (void*)SDL_GL_GetProcAddress(name); }
#endif
#ifdef __linux__
/* Linux: an OpenGL 3.3 context without a window (EGL device platform, surfaceless), made current:
   HDR output draws only into framebuffers, and some drivers (NVIDIA's 32-bit EGL) cannot make
   Wayland window surfaces; 1 = done */
int  hdr_gl_headless(void);
void hdr_gl_headless_release(void);
#endif

/* SDL's window SDR white level is relative to this: Windows reports its SDR content brightness
   over 80 nits; Wayland compositors give only the headroom over their reference white, taken as
   BT.2408's 203 nits (where a PQ compositor puts SDR white) */
#ifdef _WIN32
#define HDR_WHITE_NITS 80.0f
#else
#define HDR_WHITE_NITS 203.0f
#endif
/* Auto peak brightness (nits) from SDL's window properties; beyond 4000 nits it is PQ's nominal
   range rather than the display's (GNOME reports 10000): taken as 1000 */
static inline float hdr_auto_peak(float sdr_white_level, float headroom)
{
    float p = HDR_WHITE_NITS * sdr_white_level * headroom;
    return p > 4000.0f ? 1000.0f : p;
}
