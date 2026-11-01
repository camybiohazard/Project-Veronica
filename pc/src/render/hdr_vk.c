/* HDR output on Linux (hdr.h): Vulkan presents what OpenGL drew.

   An FP16 Vulkan image the size of the window is exported (opaque fd) and imported into OpenGL
   (GL_EXT_memory_object_fd) as a texture attached to a GL framebuffer; two exported binary
   semaphores order the two APIs: GL signals gl_done after gs_gl.c composed the image into it,
   Vulkan waits for it, blits the image into the swapchain image and signals vk_done, which GL
   waits for before it draws into the image again. The image stays in GENERAL layout and moves
   between the external (GL) queue family and ours by ownership barriers.

   The Vulkan device is the GL driver's (device UUID, GL_EXT_memory_object). The swapchain is HDR10
   (VK_COLOR_SPACE_HDR10_ST2084_EXT, 10-bit or FP16; the image then holds PQ values, hdr_pq) or
   else scRGB (VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, FP16); the Vulkan driver offers these on
   Wayland when the compositor does HDR (color-management-v1) and tells it. The window is an SDL
   Vulkan window; the GL context has none (hdr_gl_headless, host.c). */
#ifdef __linux__
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string.h>
#include "hdr.h"
#include "gl_loader.h"

void host_log(const char* fmt, ...);

/* GL_EXT_memory_object(_fd), GL_EXT_semaphore(_fd) */
#define GL_RGBA16F 0x881A
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#define GL_DEDICATED_MEMORY_OBJECT_EXT 0x9581
#define GL_DEVICE_UUID_EXT 0x9597
#define GL_LAYOUT_GENERAL_EXT 0x958D
typedef void (GLAPIENTRY *PFN_MemObjects)(GLsizei n, GLuint* objs);
typedef void (GLAPIENTRY *PFN_MemObjectParameteriv)(GLuint obj, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY *PFN_ImportMemoryFd)(GLuint obj, uint64_t size, GLenum type, GLint fd);
typedef void (GLAPIENTRY *PFN_TexStorageMem2D)(GLenum target, GLsizei levels, GLenum fmt, GLsizei w, GLsizei h, GLuint mem, uint64_t offset);
typedef void (GLAPIENTRY *PFN_ImportSemaphoreFd)(GLuint sem, GLenum type, GLint fd);
typedef void (GLAPIENTRY *PFN_SemaphoreOp)(GLuint sem, GLuint nbuf, const GLuint* bufs, GLuint ntex, const GLuint* texs, const GLenum* layouts);
typedef void (GLAPIENTRY *PFN_GetUnsignedBytei)(GLenum target, GLuint index, GLubyte* data);
typedef void (GLAPIENTRY *PFN_Flush)(void);
static struct {
    PFN_MemObjects create_mem, delete_mem, gen_sem, delete_sem;
    PFN_MemObjectParameteriv mem_param;
    PFN_ImportMemoryFd import_mem;
    PFN_TexStorageMem2D storage_mem;
    PFN_ImportSemaphoreFd import_sem;
    PFN_SemaphoreOp wait, signal;
    PFN_GetUnsignedBytei get_ubyte;
    PFN_Flush flush;
} gx;

#define VK_INSTANCE_FUNCS(X) \
    X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties2) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceSurfaceSupportKHR) \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) X(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    X(vkGetPhysicalDeviceSurfacePresentModesKHR) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceFormatProperties) X(vkEnumerateDeviceExtensionProperties) X(vkCreateDevice) \
    X(vkDestroySurfaceKHR) \
    X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) \
    X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) X(vkQueueSubmit) \
    X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) \
    X(vkBindImageMemory) X(vkGetMemoryFdKHR) X(vkCreateSemaphore) X(vkDestroySemaphore) X(vkGetSemaphoreFdKHR) \
    X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) X(vkCreateCommandPool) \
    X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
    X(vkCmdPipelineBarrier) X(vkCmdBlitImage) X(vkDeviceWaitIdle)
#define DECL(f) static PFN_##f f;
VK_INSTANCE_FUNCS(DECL)
static PFN_vkCreateInstance vkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;

#define MAX_IMAGES 8
static int active, pq;
static SDL_Window* window;
static VkInstance inst;
static VkSurfaceKHR surface;
static VkPhysicalDevice phys;
static VkDevice dev;
static uint32_t family;
static VkQueue queue;
static VkSurfaceFormatKHR sformat;
static VkSwapchainKHR chain;
static VkImage chain_img[MAX_IMAGES];
static VkSemaphore chain_done[MAX_IMAGES];   /* the blit into image i finished: present it */
static uint32_t chain_n;
static VkExtent2D chain_ext;
static int chain_dirty, chain_vsync;
static VkCommandPool pool;
static VkCommandBuffer cmd;
static VkFence fence;
static int fence_used;
static VkSemaphore acquired, gl_done, vk_done;
static GLuint gl_done_gl, vk_done_gl;
static int vk_pending;   /* vk_done signalled, GL has not waited for it yet */
static VkImage img;      /* the shared image */
static VkDeviceMemory img_mem;
static GLuint img_glmem, gl_tex, gl_fbo;
static int img_w, img_h, began;

/* ---------------- the headless OpenGL context (hdr.h hdr_gl_headless) ---------------- */
typedef void* EGLDisplay; typedef void* EGLContext; typedef void* EGLConfig; typedef void* EGLDeviceEXT;
typedef int32_t EGLint; typedef unsigned int EGLBoolean; typedef unsigned int EGLenum;
#define EGL_NONE 0x3038
#define EGL_EXTENSIONS 0x3055
#define EGL_OPENGL_API 0x30A2
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_BIT 0x0008
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x0001
static struct {
    SDL_SharedObject* lib;
    void* (*gpa)(const char* name);
    EGLBoolean (*query_devices)(EGLint max, EGLDeviceEXT* devs, EGLint* n);
    const char* (*device_string)(EGLDeviceEXT dev, EGLint name);
    EGLDisplay (*platform_display)(EGLenum platform, void* native, const EGLint* attribs);
    EGLBoolean (*initialize)(EGLDisplay dpy, EGLint* major, EGLint* minor);
    EGLBoolean (*terminate)(EGLDisplay dpy);
    const char* (*query_string)(EGLDisplay dpy, EGLint name);
    EGLBoolean (*bind_api)(EGLenum api);
    EGLBoolean (*choose_config)(EGLDisplay dpy, const EGLint* attribs, EGLConfig* cfgs, EGLint n, EGLint* got);
    EGLContext (*create_context)(EGLDisplay dpy, EGLConfig cfg, EGLContext share, const EGLint* attribs);
    EGLBoolean (*destroy_context)(EGLDisplay dpy, EGLContext ctx);
    EGLBoolean (*make_current)(EGLDisplay dpy, void* draw, void* read, EGLContext ctx);
    EGLDisplay dpy;
    EGLContext ctx;
} egl;

static int ext_listed(const char* list, const char* name)
{
    size_t n = strlen(name);
    for (const char* p = list; p && (p = strstr(p, name)); p += n)
        if ((p == list || p[-1] == ' ') && (p[n] == ' ' || !p[n])) return 1;
    return 0;
}

/* a GL 3.3 core context current without a surface on display dpy, or 0 */
static int headless_on(EGLDisplay dpy)
{
    static const EGLint ctx_attr[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                                       EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
    static const EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
    EGLConfig cfg = NULL;
    EGLint got = 0;
    const char* ext;
    if (!egl.initialize(dpy, NULL, NULL)) return 0;
    ext = egl.query_string(dpy, EGL_EXTENSIONS);
    if (!ext_listed(ext, "EGL_KHR_surfaceless_context") ||
        (!ext_listed(ext, "EGL_KHR_no_config_context") && (!egl.choose_config(dpy, cfg_attr, &cfg, 1, &got) || !got)) ||
        !egl.bind_api(EGL_OPENGL_API) || !(egl.ctx = egl.create_context(dpy, cfg, NULL, ctx_attr))) {
        egl.terminate(dpy);
        return 0;
    }
    if (!egl.make_current(dpy, NULL, NULL, egl.ctx)) { egl.destroy_context(dpy, egl.ctx); egl.ctx = NULL; egl.terminate(dpy); return 0; }
    egl.dpy = dpy;
    return 1;
}

int hdr_gl_headless(void)
{
    EGLDeviceEXT devs[16];
    EGLint n = 0;
    if (!(egl.lib = SDL_LoadObject("libEGL.so.1")) || !(egl.gpa = (void* (*)(const char*))SDL_LoadFunction(egl.lib, "eglGetProcAddress"))) {
        host_log("hdr: no libEGL.so.1\n");
        return 0;
    }
#define EGLP(field, name) *(void**)&egl.field = egl.gpa(name);
    EGLP(query_devices, "eglQueryDevicesEXT") EGLP(device_string, "eglQueryDeviceStringEXT") EGLP(platform_display, "eglGetPlatformDisplayEXT")
    EGLP(initialize, "eglInitialize") EGLP(terminate, "eglTerminate") EGLP(query_string, "eglQueryString") EGLP(bind_api, "eglBindAPI")
    EGLP(choose_config, "eglChooseConfig") EGLP(create_context, "eglCreateContext") EGLP(destroy_context, "eglDestroyContext")
    EGLP(make_current, "eglMakeCurrent")
#undef EGLP
    if (!egl.query_devices || !egl.device_string || !egl.platform_display || !egl.initialize || !egl.terminate || !egl.query_string ||
        !egl.bind_api || !egl.choose_config || !egl.create_context || !egl.destroy_context || !egl.make_current) {
        host_log("hdr: EGL lacks the device platform (EGL_EXT_device_enumeration, EGL_EXT_platform_device)\n");
        return 0;
    }
    if (!egl.query_devices(16, devs, &n)) n = 0;
    for (int pass = 0; pass < 2; pass++)   /* GPUs first, then software rasterizers */
        for (EGLint i = 0; i < n; i++) {
            EGLDisplay dpy;
            if (ext_listed(egl.device_string(devs[i], EGL_EXTENSIONS), "EGL_MESA_device_software") != pass) continue;
            if ((dpy = egl.platform_display(EGL_PLATFORM_DEVICE_EXT, devs[i], NULL)) && headless_on(dpy)) return 1;
        }
    host_log("hdr: no EGL device gives a surfaceless OpenGL 3.3 context\n");
    return 0;
}

void hdr_gl_headless_release(void)
{
    if (egl.ctx) {
        egl.make_current(egl.dpy, NULL, NULL, NULL);
        egl.destroy_context(egl.dpy, egl.ctx);
        egl.terminate(egl.dpy);
        egl.ctx = NULL; egl.dpy = NULL;
    }
}

void* hdr_gl_proc(const char* name) { return egl.ctx ? egl.gpa(name) : (void*)SDL_GL_GetProcAddress(name); }

/* an extension of the current GL context */
static int gl_has(const char* name)
{
    const GLubyte* (GLAPIENTRY *get_stringi)(GLenum, GLuint) = (const GLubyte* (GLAPIENTRY *)(GLenum, GLuint))hdr_gl_proc("glGetStringi");
    void (GLAPIENTRY *get_integerv)(GLenum, GLint*) = (void (GLAPIENTRY *)(GLenum, GLint*))hdr_gl_proc("glGetIntegerv");
    GLint n = 0;
    if (!get_stringi || !get_integerv) return 0;
    get_integerv(0x821D /* GL_NUM_EXTENSIONS */, &n);
    for (GLint i = 0; i < n; i++) {
        const char* e = (const char*)get_stringi(0x1F03 /* GL_EXTENSIONS */, (GLuint)i);
        if (e && !strcmp(e, name)) return 1;
    }
    return 0;
}

static const char* vk_err(VkResult r)
{
    static char b[16];
    switch (r) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "out of host memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "out of device memory";
    case VK_ERROR_INITIALIZATION_FAILED: return "initialization failed";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "extension not present";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "feature not present";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "incompatible driver";
    case VK_ERROR_SURFACE_LOST_KHR: return "surface lost";
    case VK_ERROR_DEVICE_LOST: return "device lost";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "invalid external handle";
    default: SDL_snprintf(b, sizeof(b), "%d", (int)r); return b;
    }
}

static int has_ext(const VkExtensionProperties* e, uint32_t n, const char* name)
{
    for (uint32_t i = 0; i < n; i++) if (!strcmp(e[i].extensionName, name)) return 1;
    return 0;
}

/* an exportable binary semaphore and its GL import */
static int make_semaphore(VkSemaphore* s, GLuint* g)
{
    VkExportSemaphoreCreateInfo es = { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, NULL, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT };
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &es, 0 };
    VkSemaphoreGetFdInfoKHR gi = { VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, NULL, VK_NULL_HANDLE, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT };
    int fd = -1;
    if (vkCreateSemaphore(dev, &sci, NULL, s) != VK_SUCCESS) return 0;
    gi.semaphore = *s;
    if (vkGetSemaphoreFdKHR(dev, &gi, &fd) != VK_SUCCESS) return 0;
    gx.gen_sem(1, g);
    gx.import_sem(*g, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);   /* GL owns the fd now */
    return 1;
}

static void release_image(void)
{
    if (gl_tex) { glDeleteTextures(1, &gl_tex); gl_tex = 0; }
    if (img_glmem) { gx.delete_mem(1, &img_glmem); img_glmem = 0; }
    if (img) { vkDestroyImage(dev, img, NULL); img = VK_NULL_HANDLE; }
    if (img_mem) { vkFreeMemory(dev, img_mem, NULL); img_mem = VK_NULL_HANDLE; }
    img_w = img_h = 0;
}

/* the shared image at w x h (both APIs idle) */
static int make_image(int w, int h)
{
    VkExternalMemoryImageCreateInfo ext = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, NULL, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT };
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &ext };
    VkMemoryRequirements req;
    VkPhysicalDeviceMemoryProperties mp;
    VkMemoryDedicatedAllocateInfo ded = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
    VkExportMemoryAllocateInfo exp = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, &ded, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT };
    VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &exp };
    VkMemoryGetFdInfoKHR gi = { VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, NULL, VK_NULL_HANDLE, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT };
    GLint dedicated = 1;
    int fd = -1;
    VkResult r;
    release_image();
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    ici.extent = (VkExtent3D){ (uint32_t)w, (uint32_t)h, 1 };
    ici.mipLevels = 1; ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if ((r = vkCreateImage(dev, &ici, NULL, &img)) != VK_SUCCESS) { host_log("hdr: vkCreateImage %d x %d: %s\n", w, h, vk_err(r)); return 0; }
    vkGetImageMemoryRequirements(dev, img, &req);
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    mai.memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount && mai.memoryTypeIndex == UINT32_MAX; i++)
        if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) mai.memoryTypeIndex = i;
    for (uint32_t i = 0; i < mp.memoryTypeCount && mai.memoryTypeIndex == UINT32_MAX; i++)
        if (req.memoryTypeBits & (1u << i)) mai.memoryTypeIndex = i;
    mai.allocationSize = req.size;
    ded.image = img;
    if ((r = vkAllocateMemory(dev, &mai, NULL, &img_mem)) != VK_SUCCESS) { host_log("hdr: vkAllocateMemory: %s\n", vk_err(r)); release_image(); return 0; }
    if ((r = vkBindImageMemory(dev, img, img_mem, 0)) != VK_SUCCESS) { host_log("hdr: vkBindImageMemory: %s\n", vk_err(r)); release_image(); return 0; }
    gi.memory = img_mem;
    if ((r = vkGetMemoryFdKHR(dev, &gi, &fd)) != VK_SUCCESS) { host_log("hdr: vkGetMemoryFdKHR: %s\n", vk_err(r)); release_image(); return 0; }
    gx.create_mem(1, &img_glmem);
    gx.mem_param(img_glmem, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated);
    gx.import_mem(img_glmem, req.size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);   /* GL owns the fd now */
    glGenTextures(1, &gl_tex);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    gx.storage_mem(GL_TEXTURE_2D, 1, GL_RGBA16F, w, h, img_glmem, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!gl_fbo) glGenFramebuffers(1, &gl_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        host_log("hdr: the imported texture is not renderable in OpenGL\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        release_image();
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    img_w = w; img_h = h;
    return 1;
}

/* (re)create the swapchain for the window's size and the vsync setting */
static int make_chain(int vsync)
{
    VkSurfaceCapabilitiesKHR caps;
    VkPresentModeKHR modes[16], mode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t nmodes = 16;
    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkSwapchainKHR old = chain;
    VkResult r;
    int w, h;
    vkDeviceWaitIdle(dev);
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps) != VK_SUCCESS) return 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (caps.currentExtent.width != UINT32_MAX) { w = (int)caps.currentExtent.width; h = (int)caps.currentExtent.height; }
    if (w <= 0 || h <= 0) return 0;   /* minimised */
    if (w < (int)caps.minImageExtent.width) w = (int)caps.minImageExtent.width;
    if (h < (int)caps.minImageExtent.height) h = (int)caps.minImageExtent.height;
    if (w > (int)caps.maxImageExtent.width) w = (int)caps.maxImageExtent.width;
    if (h > (int)caps.maxImageExtent.height) h = (int)caps.maxImageExtent.height;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &nmodes, modes) < 0) nmodes = 0;
    if (!vsync)   /* as the GL path's swap interval 0: tearing, else the newest image */
        for (int pass = 0; pass < 2 && mode == VK_PRESENT_MODE_FIFO_KHR; pass++)
            for (uint32_t i = 0; i < nmodes; i++)
                if (modes[i] == (pass ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)) mode = modes[i];
    ci.surface = surface;
    ci.minImageCount = caps.minImageCount < 2 ? 2 : caps.minImageCount;
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount) ci.minImageCount = caps.maxImageCount;
    ci.imageFormat = sformat.format;
    ci.imageColorSpace = sformat.colorSpace;
    ci.imageExtent = (VkExtent2D){ (uint32_t)w, (uint32_t)h };
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                      : (VkCompositeAlphaFlagBitsKHR)(caps.supportedCompositeAlpha & -caps.supportedCompositeAlpha);
    ci.presentMode = mode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = old;
    r = vkCreateSwapchainKHR(dev, &ci, NULL, &chain);
    if (old) vkDestroySwapchainKHR(dev, old, NULL);
    if (r != VK_SUCCESS) { chain = VK_NULL_HANDLE; host_log("hdr: vkCreateSwapchainKHR: %s\n", vk_err(r)); return 0; }
    chain_n = MAX_IMAGES;
    if (vkGetSwapchainImagesKHR(dev, chain, &chain_n, chain_img) < 0) { chain_n = 0; return 0; }
    chain_ext = ci.imageExtent;
    chain_vsync = vsync;
    chain_dirty = 0;
    return 1;
}

static void release_all(void)
{
    if (dev) vkDeviceWaitIdle(dev);
    if (gx.flush) glFinish();
    release_image();
    if (gl_fbo) { glDeleteFramebuffers(1, &gl_fbo); gl_fbo = 0; }
    if (gl_done_gl) { gx.delete_sem(1, &gl_done_gl); gl_done_gl = 0; }
    if (vk_done_gl) { gx.delete_sem(1, &vk_done_gl); vk_done_gl = 0; }
    if (dev) {
        for (int i = 0; i < MAX_IMAGES; i++) if (chain_done[i]) { vkDestroySemaphore(dev, chain_done[i], NULL); chain_done[i] = VK_NULL_HANDLE; }
        if (acquired) vkDestroySemaphore(dev, acquired, NULL);
        if (gl_done) vkDestroySemaphore(dev, gl_done, NULL);
        if (vk_done) vkDestroySemaphore(dev, vk_done, NULL);
        if (fence) vkDestroyFence(dev, fence, NULL);
        if (pool) vkDestroyCommandPool(dev, pool, NULL);
        if (chain) vkDestroySwapchainKHR(dev, chain, NULL);
        vkDestroyDevice(dev, NULL);
    }
    if (surface) vkDestroySurfaceKHR(inst, surface, NULL);
    if (inst && vkDestroyInstance) vkDestroyInstance(inst, NULL);
    acquired = gl_done = vk_done = VK_NULL_HANDLE; fence = VK_NULL_HANDLE; pool = VK_NULL_HANDLE; chain = VK_NULL_HANDLE;
    dev = VK_NULL_HANDLE; surface = VK_NULL_HANDLE; inst = VK_NULL_HANDLE;
    active = 0;
}

/* the Vulkan device whose UUID the GL driver reports */
static VkPhysicalDevice gl_device(void)
{
    VkPhysicalDevice pd[16];
    uint32_t n = 16;
    GLubyte uuid[VK_UUID_SIZE] = { 0 };
    gx.get_ubyte(GL_DEVICE_UUID_EXT, 0, uuid);
    if (vkEnumeratePhysicalDevices(inst, &n, pd) < 0) return VK_NULL_HANDLE;
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceIDProperties id = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
        VkPhysicalDeviceProperties2 p = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &id };
        vkGetPhysicalDeviceProperties2(pd[i], &p);
        if (!memcmp(id.deviceUUID, uuid, VK_UUID_SIZE)) {
            host_log("hdr: Vulkan device %s\n", p.properties.deviceName);
            return pd[i];
        }
    }
    return VK_NULL_HANDLE;
}

/* the surface format: HDR10 (10-bit or FP16) first, else scRGB; one the blit can write */
static int pick_format(void)
{
    VkSurfaceFormatKHR f[64];
    uint32_t n = 64;
    int best = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &n, f) < 0) return 0;
    for (uint32_t i = 0; i < n; i++) {
        VkFormatProperties fp;
        int score = 0;
        if (f[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT &&
            (f[i].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || f[i].format == VK_FORMAT_A2R10G10B10_UNORM_PACK32)) score = 4;
        else if (f[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT && f[i].format == VK_FORMAT_R16G16B16A16_SFLOAT) score = 3;
        else if (f[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT && f[i].format == VK_FORMAT_R16G16B16A16_SFLOAT) score = 2;
        if (score <= best) continue;
        vkGetPhysicalDeviceFormatProperties(phys, f[i].format, &fp);
        if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) continue;
        best = score; sformat = f[i];
    }
    pq = best >= 3;
    return best > 0;
}

int hdr_init(void* sdl_window)
{
    static const char* const dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME };
    PFN_vkGetInstanceProcAddr gipa;
    VkExtensionProperties ext[256];
    uint32_t next = 256, nsdl = 0, n;
    const char* const* sdl_exts;
    const char* inst_exts[32];
    uint32_t ninst = 0;
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "recvx", 1, "recvx", 1, VK_API_VERSION_1_1 };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app };
    VkQueueFamilyProperties qf[16];
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 3, dev_exts };
    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, VK_NULL_HANDLE, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkResult r;
    const char* why = NULL;
    int w, h;

    window = (SDL_Window*)sdl_window;
    if (!gl_has("GL_EXT_memory_object") || !gl_has("GL_EXT_memory_object_fd") || !gl_has("GL_EXT_semaphore") || !gl_has("GL_EXT_semaphore_fd")) {
        host_log("hdr: unavailable (the OpenGL driver has no GL_EXT_memory_object_fd / GL_EXT_semaphore_fd), SDR output\n");
        return 0;
    }
#define GLP(field, type, name) if (!(gx.field = (type)hdr_gl_proc(name))) why = "an OpenGL interop function is missing";
    GLP(create_mem, PFN_MemObjects, "glCreateMemoryObjectsEXT") GLP(delete_mem, PFN_MemObjects, "glDeleteMemoryObjectsEXT")
    GLP(gen_sem, PFN_MemObjects, "glGenSemaphoresEXT") GLP(delete_sem, PFN_MemObjects, "glDeleteSemaphoresEXT")
    GLP(mem_param, PFN_MemObjectParameteriv, "glMemoryObjectParameterivEXT") GLP(import_mem, PFN_ImportMemoryFd, "glImportMemoryFdEXT")
    GLP(storage_mem, PFN_TexStorageMem2D, "glTexStorageMem2DEXT") GLP(import_sem, PFN_ImportSemaphoreFd, "glImportSemaphoreFdEXT")
    GLP(wait, PFN_SemaphoreOp, "glWaitSemaphoreEXT") GLP(signal, PFN_SemaphoreOp, "glSignalSemaphoreEXT")
    GLP(get_ubyte, PFN_GetUnsignedBytei, "glGetUnsignedBytei_vEXT") GLP(flush, PFN_Flush, "glFlush")
#undef GLP
    if (why) goto fail;

    if (!SDL_Vulkan_LoadLibrary(NULL) || !(gipa = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr())) { why = "no Vulkan loader (libvulkan.so.1)"; goto fail; }
    vkCreateInstance = (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");
    vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)gipa(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!vkCreateInstance || !vkEnumerateInstanceExtensionProperties) { why = "no Vulkan loader entry points"; goto fail; }
    sdl_exts = SDL_Vulkan_GetInstanceExtensions(&nsdl);
    if (!sdl_exts) { why = "SDL has no Vulkan surface for this window"; goto fail; }
    for (uint32_t i = 0; i < nsdl && ninst < 30; i++) inst_exts[ninst++] = sdl_exts[i];
    if (vkEnumerateInstanceExtensionProperties(NULL, &next, ext) < 0) next = 0;
    if (!has_ext(ext, next, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME)) { why = "the Vulkan driver has no HDR colour spaces (VK_EXT_swapchain_colorspace)"; goto fail; }
    inst_exts[ninst++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
    ici.enabledExtensionCount = ninst;
    ici.ppEnabledExtensionNames = inst_exts;
    if ((r = vkCreateInstance(&ici, NULL, &inst)) != VK_SUCCESS) { why = vk_err(r); goto fail; }
#define LOAD(f) if (!(f = (PFN_##f)gipa(inst, #f))) why = "a Vulkan function is missing: " #f;
    VK_INSTANCE_FUNCS(LOAD)
#undef LOAD
    if (why) goto fail;
    if (!SDL_Vulkan_CreateSurface(window, inst, NULL, &surface)) { why = "SDL_Vulkan_CreateSurface failed"; goto fail; }
    if (!(phys = gl_device())) { why = "no Vulkan device is the OpenGL driver's (32-bit Vulkan driver installed?)"; goto fail; }
    next = 256;
    if (vkEnumerateDeviceExtensionProperties(phys, NULL, &next, ext) < 0) next = 0;
    for (int i = 0; i < 3; i++) if (!has_ext(ext, next, dev_exts[i])) { why = "the Vulkan device lacks external memory / semaphore fds or swapchains"; goto fail; }
    n = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, qf);
    family = UINT32_MAX;
    for (uint32_t i = 0; i < n && family == UINT32_MAX; i++) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &present);
        if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) family = i;
    }
    if (family == UINT32_MAX) { why = "no Vulkan queue can present to the window"; goto fail; }
    if (!pick_format()) { why = "the display or compositor offers no HDR swapchain (HDR off, X11, or no colour management)"; goto fail; }
    qci.queueFamilyIndex = family;
    if ((r = vkCreateDevice(phys, &dci, NULL, &dev)) != VK_SUCCESS) { why = vk_err(r); goto fail; }
    vkGetDeviceQueue(dev, family, 0, &queue);
    pci.queueFamilyIndex = family;
    if (vkCreateCommandPool(dev, &pci, NULL, &pool) != VK_SUCCESS) { why = "vkCreateCommandPool failed"; goto fail; }
    cai.commandPool = pool;
    if (vkAllocateCommandBuffers(dev, &cai, &cmd) != VK_SUCCESS || vkCreateFence(dev, &fci, NULL, &fence) != VK_SUCCESS ||
        vkCreateSemaphore(dev, &sci, NULL, &acquired) != VK_SUCCESS) { why = "Vulkan objects failed"; goto fail; }
    for (int i = 0; i < MAX_IMAGES; i++) if (vkCreateSemaphore(dev, &sci, NULL, &chain_done[i]) != VK_SUCCESS) { why = "Vulkan objects failed"; goto fail; }
    if (!make_semaphore(&gl_done, &gl_done_gl) || !make_semaphore(&vk_done, &vk_done_gl)) { why = "exported semaphores failed"; goto fail; }
    if (!make_chain(1)) chain_dirty = 1;   /* the window, not shown yet, may have no size: at the first image */
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (!make_image(w > 0 ? w : 640, h > 0 ? h : 480)) { why = "the shared image failed"; goto fail; }
    host_log("hdr: Vulkan %s presentation (format %d)\n", pq ? "HDR10 (PQ)" : "scRGB", (int)sformat.format);
    active = 1;
    return 1;

fail:
    host_log("hdr: unavailable (%s), SDR output\n", why ? why : "?");
    release_all();
    return 0;
}

int hdr_active(void) { return active; }
int hdr_pq(void) { return pq; }

unsigned int hdr_begin(int w, int h)
{
    static const GLenum general = GL_LAYOUT_GENERAL_EXT;
    began = 0;
    if (!active || w <= 0 || h <= 0) return 0;
    if (w != img_w || h != img_h) {
        vkDeviceWaitIdle(dev);
        glFinish();
        if (!make_image(w, h)) return 0;
        chain_dirty = 1;
    }
    if (vk_pending) { gx.wait(vk_done_gl, 0, NULL, 1, &gl_tex, &general); vk_pending = 0; }
    began = 1;
    return gl_fbo;
}

static void image_barrier(VkImage im, VkImageLayout from, VkImageLayout to, uint32_t src_family, uint32_t dst_family, VkAccessFlags src, VkAccessFlags dst)
{
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, src, dst, from, to, src_family, dst_family, im, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}

uint64_t hdr_present(int vsync)
{
    static const GLenum general = GL_LAYOUT_GENERAL_EXT;
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VkSemaphore waits[2], signals[2];
    VkPipelineStageFlags stages[2] = { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    uint32_t idx = 0;
    int got = 0;
    uint64_t t;
    VkResult r;
    if (!active || !began) return 0;
    began = 0;
    gx.signal(gl_done_gl, 0, NULL, 1, &gl_tex, &general);
    gx.flush();
    t = SDL_GetTicksNS();
    if (fence_used) { vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX); vkResetFences(dev, 1, &fence); }
    if ((chain_dirty || !chain || vsync != chain_vsync) && !make_chain(vsync)) chain_dirty = 1;
    if (chain && !chain_dirty) {
        r = vkAcquireNextImageKHR(dev, chain, UINT64_MAX, acquired, VK_NULL_HANDLE, &idx);
        if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) got = 1;
        if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) chain_dirty = 1;
    }
    /* always submitted: it consumes gl_done and signals vk_done even without a swapchain image */
    vkBeginCommandBuffer(cmd, &bi);
    image_barrier(img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_EXTERNAL, family, 0, VK_ACCESS_TRANSFER_READ_BIT);
    if (got) {
        VkImageBlit bl = { { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { { 0, 0, 0 }, { img_w, img_h, 1 } },
                           { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { { 0, 0, 0 }, { (int32_t)chain_ext.width, (int32_t)chain_ext.height, 1 } } };
        int same = img_w == (int)chain_ext.width && img_h == (int)chain_ext.height;
        image_barrier(chain_img[idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        vkCmdBlitImage(cmd, img, VK_IMAGE_LAYOUT_GENERAL, chain_img[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bl, same ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
        image_barrier(chain_img[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, VK_ACCESS_TRANSFER_WRITE_BIT, 0);
    }
    image_barrier(img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, family, VK_QUEUE_FAMILY_EXTERNAL, VK_ACCESS_TRANSFER_READ_BIT, 0);
    vkEndCommandBuffer(cmd);
    waits[0] = gl_done; signals[0] = vk_done;
    if (got) { waits[1] = acquired; signals[1] = chain_done[idx]; }
    si.waitSemaphoreCount = got ? 2 : 1; si.pWaitSemaphores = waits; si.pWaitDstStageMask = stages;
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = got ? 2 : 1; si.pSignalSemaphores = signals;
    r = vkQueueSubmit(queue, 1, &si, fence);
    fence_used = r == VK_SUCCESS;
    vk_pending = fence_used;
    if (r != VK_SUCCESS) {
        static int logged;
        if (!logged++) host_log("hdr: vkQueueSubmit: %s\n", vk_err(r));
    } else if (got) {
        VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, &chain_done[idx], 1, &chain, &idx };
        r = vkQueuePresentKHR(queue, &pi);
        if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) chain_dirty = 1;
    }
    return SDL_GetTicksNS() - t;
}
#endif
