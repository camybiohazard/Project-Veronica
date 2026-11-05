/* GS command stream: game thread -> render thread, and frame interpolation (stream.c). */
#pragma once
#include <stdint.h>

typedef struct Frame Frame;

void     stream_init(void);
/* game thread */
void     stream_reg(int reg, uint64_t v);
void     stream_image(const uint8_t* data, uint32_t len);
void     stream_disp(uint32_t fbp, uint32_t fbw, uint32_t psm);
void     stream_tag(uint32_t mesh);                 /* following GS output belongs to this mesh (0: none) */
void     stream_strip(uint32_t strip);              /* ... and within it to this strip */
void     stream_tag_last(uint32_t mesh, uint32_t strip);   /* the primitive group just drawn belongs to this mesh and strip */
void     stream_tex_def_begin(uint32_t id, uint32_t tbp, uint32_t cbp, uint32_t image_pages, int flags);   /* flags: NTEX_FONT, NTEX_KEEP (textures.h) */
void     stream_tex_def_end(void);
void     stream_tex_bind(uint32_t id, uint32_t tbp, uint32_t cbp);
void     stream_tex_forget(uint32_t id);
void     stream_rtex_capture(uint32_t id, uint32_t fbp, uint32_t fbw, uint32_t w, uint32_t h, int wide);
void     stream_movie(uint32_t tbp, int index);    /* movie frame index is the texture at tbp */
void     stream_submit(void);                       /* end of frame (buffer swap) */
/* per-pixel lighting: a light set, STREAM_LSET floats (layout: gs_gl.c fs_src light_sum), into the
   frame; returns its number for NativeVertex.light (0: the table is full) */
#define  STREAM_LSET 104
#define  STREAM_LSET_MAX 4095
uint32_t stream_light_set(const float* set);
/* Enhanced fog: a fog set, STREAM_FSET floats (the game's fog table, 128, then its density and 3 unused;
   gs_gl.c fs_src fog_power), into the frame; returns its number for NativeVertex.fogset (0: the table is full) */
#define  STREAM_FSET 132
#define  STREAM_FSET_MAX 15
uint32_t stream_fog_set(const float* set);
/* ... and the frame's projection and lights for the glow: STREAM_FX floats (gs_gl.c fs_src FX: row 0
   the light count (the render thread adds the options and the scale), row 1 the projection centre and
   kx, ky, row 2 the depth's a, b, then per light 8: view position, far range^2, colour, near range^2) */
#define  STREAM_FX 76
#define  STREAM_FX_LIGHTS 8
void     stream_fog_ao(const float* set);       /* STREAM_FSET floats: the frame's first fog (ambient occlusion fades in it), any fog style */
void     stream_fog_view(const float* proj);     /* 6 floats: centre x, y, kx, ky, a, b */
void     stream_fog_light(const float* light);   /* 8 floats; the same position again: skipped */
#define  STREAM_CAM 20                              /* view_glue.c pc_frame_view: the frame's camera and projection */
void     stream_download(void* dst, uint32_t len);  /* StoreImage: waits for the pixels */
uint64_t stream_game_frames(void);
/* render thread */
Frame*   stream_pop(int timeout_ms);
void     stream_release(Frame* f);
int      stream_is_partial(const Frame* f);
int      stream_queued(void);      /* render thread: a game frame waits to be executed */
void     stream_exec(Frame* f);
void     stream_note_upload(const uint64_t* pages);
int      stream_match(const Frame* prev, const Frame* cur);
void     stream_replay(const Frame* f, float t);
