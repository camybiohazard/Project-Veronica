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
