/* Vertex lighting options (GRAPHICS "Light falloff", "Soft shading", "Main light", "Ambient light";
   docs/USER_GUIDE.md): the light sum of the chunk models' vertices (ps2_NinjaCnk.c njCnkCvVn, and
   njCnkCvVnPs2 through VU0 CALCPOINT, which sums the same) is computed again after the original
   with pc_rules.h pc_light_vertex. Only fIr/fIg/fIb change; position, fog, normal z and the colour
   modes (native_glue.c vertex_color) stay. Ambient light scales the ambient colour where it is set
   (pc_light_ambient_scale). At the original values nothing is computed again.

   Per-pixel lighting ("Per-pixel lighting", with the native 3D renderer): the vertex chunks also
   keep each vertex's view normal (by its index in the vertex buffer), and pc_native_strip gives
   its vertices the view position, that normal and the light set the strip is lit with
   (pc_light_set: the lights, ambient, material and colour mode), which gs_gl.c fs_src evaluates
   per pixel.

   Flashlight (Flashlight on/off in CONTROLS): a spot light from Claire's gun height (the point
   tps_glue.c aims from), where she faces, a little down, or where she aims; in play only. It goes
   into every light set (texels 22..25) in the view space of the game's lights (cam.mtx) and lights
   per pixel only, so the light sets are made while it shines (with Per-pixel lighting off the
   strips keep their vertex colour and gs_gl.c fs_src adds only the flashlight's share).

   Wet surfaces (Wet surfaces): while it rains (effect 100 runs), the light sets say how wet and
   where up is, and gs_gl.c fs_src gives up-facing surfaces a highlight from every light; per-pixel
   lighting is on for it. Game thread. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_NaMatrix.h"
#include "../../decomp/include/ps2/veronica/prog/ps2_Vu1Strip.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include <math.h>
#include "../src/gs/stream.h"
#include "pc_rules.h"
#include <stdlib.h>
#include <string.h>

int pc_light_falloff(void);   /* the options (host.c): 1 smooth */
int pc_light_soft(void);      /* % */
int pc_light_main(void);      /* % */
int pc_light_ambient(void);   /* % */
int pc_light_pixel(void);     /* per-pixel lighting (with the native 3D renderer) */
int pc_light_wet(void);       /* Wet surfaces, % */
void pc_flashlight_config(int* brightness, int* range, int* beam);
void pc_flashlight_battery_config(int* life, int* recharge);   /* minutes; life 0: no battery */
int pc_flashlight_enabled(void);   /* Flashlight: off, the button does nothing */
int pc_in_play(const BH_PWORK* p, int scope);   /* reload_glue.c */
int pad_flashlight(void);                       /* pad.c */

extern CNK_LIGHTING* pNaCnkCrntLighting;

/* flashlight: switched on, and this frame's light (stream.h layout from texel 22: position and on,
   direction and range, colour and near range^2, cos of the inner and outer beam, spill share and
   range), made at the frame's first light set */
static int flash_on, flash_was, flash_made;
/* wet surfaces (Wet surfaces, in rain): it rains this frame; the light sets carry the amount and
   the view's up (gs_gl.c fs_src) */
static int raining;
int pc_wet_active(void) { return raining && pc_light_wet() > 0; }
/* the beam's world direction, eased towards where it should point by FLASH_EASE of the way each game
   frame (raising or lowering the weapon turns it over about four frames instead of at once);
   flash_aimed 0: none yet, the next one is taken as it is */
#define FLASH_EASE 0.45f
static float flash_dir[3];
static int flash_aimed;
static float flash[16];

/* battery (Battery life, Battery recharge; 0 life: none): 0..1, drained while the light shines in
   play, recharged while it is off in play (menus and events stop both). Empty, the light goes out
   and comes on again from BATT_MIN; below BATT_LOW the beam dims and now and then flickers
   (flicker_left game frames, from its own random numbers, never the game's rand) */
#define BATT_LOW 0.2f
#define BATT_MIN 0.1f
static float battery = 1.0f, batt_k = 1.0f;   /* batt_k: the beam's strength this frame */
static int flicker_left;
static unsigned int flicker_seed = 12345u;

static float flicker_rand(void) { flicker_seed = flicker_seed * 1664525u + 1013904223u; return (flicker_seed >> 8) / 16777216.0f; }

/* stream_submit, once per game frame: the button toggles it in play; the battery */
void pc_light_frame(void)
{
    int held = pad_flashlight(), life, recharge, play = pc_in_play(&ply, 0);
    pc_flashlight_battery_config(&life, &recharge);
    if (held && !flash_was && play) flash_on = !flash_on && (life <= 0 || battery >= BATT_MIN);
    if (!pc_flashlight_enabled()) flash_on = 0;
    flash_was = held;
    flash_made = 0;
    raining = 0;   /* effect 100 (effsub1.c bhEff100): the rain round the camera */
    for (int i = 0; i < 512 && !raining; i++) raining = (eff[i].flg & 1) && eff[i].id == 100;
    batt_k = 1.0f;
    if (life <= 0) { battery = 1.0f; return; }
    if (play && flash_on) {
        battery -= 1.0f / (life * 60.0f * 30.0f);   /* game frames: 30 a second */
        if (battery <= 0.0f) { battery = 0.0f; flash_on = 0; }
        if (battery < BATT_LOW) {
            batt_k = 0.45f + 0.55f * battery / BATT_LOW;
            if (flicker_left > 0) { flicker_left--; batt_k *= 0.25f + 0.5f * flicker_rand(); }
            else if (flicker_rand() < 0.02f + 0.08f * (1.0f - battery / BATT_LOW)) flicker_left = 2 + (int)(flicker_rand() * 5.0f);
        }
    } else if (play && battery < 1.0f) {
        battery += 1.0f / (recharge * 60.0f * 30.0f);
        if (battery > 1.0f) battery = 1.0f;
    }
}

/* hud_glue.c: the battery (0..1) and whether to show it (the battery in use: on, or not full);
   on: the light shines */
int pc_flashlight_battery(float* level, int* on)
{
    int life, recharge;
    pc_flashlight_battery_config(&life, &recharge);
    *level = battery; *on = flash_on;
    return life > 0 && pc_flashlight_enabled() && (flash_on || battery < 1.0f);
}

int pc_flashlight_lit(void) { return flash_on && plp == &ply && pc_in_play(&ply, 0); }

static void flash_make(void)
{
    const float* m = (const float*)cam.mtx;   /* world -> view, as bhSetLight's lights (row r, column c: m[4 * r + c]) */
    int brightness, range, beam;
    float a, s, c, o[3], d[3], k;
    flash_made = 1;
    memset(flash, 0, sizeof(flash));
    if (!pc_flashlight_lit()) { flash_aimed = 0; return; }
    pc_flashlight_config(&brightness, &range, &beam);
    a = plp->ay * (3.14159265f / 32768.0f); s = sinf(a); c = cosf(a);
    o[0] = plp->px - s * 4.0f; o[1] = plp->py + 12.0f; o[2] = plp->pz - c * 4.0f;
    if (plp->mode1 == 1) {   /* aiming: along the aim (as tps_glue.c's muzzle ray) */
        NJS_MATRIX mx;
        NJS_VECTOR v = { 0, 0, -1.0f }, vv;
        njUnitMatrix(&mx);
        njRotateXYZ(&mx, plp->wax, plp->ay, 0);
        njCalcVector(&mx, &v, &vv);
        d[0] = vv.x; d[1] = vv.y; d[2] = vv.z;
    } else {   /* facing, 20 degrees down: the floor about 3 m ahead, in view of the fixed cameras */
        d[0] = -s * 0.940f; d[1] = -0.342f; d[2] = -c * 0.940f;
    }
    if (flash_aimed) {
        float l;
        for (int i = 0; i < 3; i++) d[i] = flash_dir[i] + (d[i] - flash_dir[i]) * FLASH_EASE;
        l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l > 1e-4f) for (int i = 0; i < 3; i++) d[i] /= l;
    }
    memcpy(flash_dir, d, sizeof(flash_dir));
    flash_aimed = 1;
    for (int i = 0; i < 3; i++) {
        flash[i] = m[i] * o[0] + m[4 + i] * o[1] + m[8 + i] * o[2] + m[12 + i];
        flash[4 + i] = m[i] * d[0] + m[4 + i] * d[1] + m[8 + i] * d[2];
    }
    k = 1.0f / sqrtf(flash[4] * flash[4] + flash[5] * flash[5] + flash[6] * flash[6]);
    flash[4] *= k; flash[5] *= k; flash[6] *= k;
    flash[3] = 1.0f;
    flash[7] = (float)range;
    k = brightness / 100.0f * 1.6f * batt_k;   /* warm white; dimmer on a low battery */
    flash[8] = 1.0f * k; flash[9] = 0.95f * k; flash[10] = 0.85f * k;
    flash[11] = (0.35f * range) * (0.35f * range);
    flash[12] = cosf(beam * 0.6f * 3.14159265f / 180.0f); flash[13] = cosf(beam * 3.14159265f / 180.0f);
    flash[14] = 0.15f; flash[15] = 60.0f;   /* spill: a faint glow all round, out to 6 m, so the light shows when the beam points at the camera */
}

static int options(void) { return pc_light_falloff() || pc_light_soft() > 0 || pc_light_main() != 100; }

int pc_light_on(void) { return options() || pc_light_pixel(); }

/* ps2_Vu1Strip.c vu1SetAmbient: the ambient colour x the Ambient light option, into out (the
   VU1 programs, the CPU strips and native_glue.c take it from there) */
void pc_light_ambient_scale(const VU1_COLOR* in, VU1_COLOR* out)
{
    const float k = pc_light_ambient() / 100.0f;
    *out = *in;
    out->fR *= k; out->fG *= k; out->fB *= k;
}

/* per-pixel lighting: view normal and whether it is valid, by vertex buffer index; the lights the
   last vertex chunk took */
static float (*vnorm)[4];
static int nvnorm, chunk_lights;

static void vnorm_grow(int n)
{
    if (n <= nvnorm) return;
    vnorm = realloc(vnorm, (size_t)n * sizeof(*vnorm));
    memset(vnorm + nvnorm, 0, (size_t)(n - nvnorm) * sizeof(*vnorm));
    nvnorm = n;
}

/* n vertices of a vertex chunk: in, stride floats apart, position then normal (model space; normal
   at in + nofs), out their buffer entries (index first on) with the view position already in
   fVx..fVz. max_lights: the lights the original takes (njCnkCvVnPs2: the first four) */
void pc_light_relight(const float* in, int stride, int nofs, VU1_STRIP_BUF* out, int first, int n, int max_lights)
{
    const float* m = (const float*)pNaMatMatrixStuckPtr;   /* row r, column c: m[4 * r + c] */
    const float wrap = pc_light_soft() / 100.0f;
    const float main_k = pc_light_main() / 100.0f;
    const int smooth = pc_light_falloff(), relight = options(), pixel = pc_light_pixel();
    const int nl = pNaCnkCrntLighting->lLightMax < max_lights ? pNaCnkCrntLighting->lLightMax : max_lights;
    chunk_lights = nl;
    if (pixel) vnorm_grow(first + n);
    for (int i = 0; i < n; i++, in += stride, out++) {
        const float* s = in + nofs;
        const float nv[3] = { m[0] * s[0] + m[4] * s[1] + m[8] * s[2], m[1] * s[0] + m[5] * s[1] + m[9] * s[2], m[2] * s[0] + m[6] * s[1] + m[10] * s[2] };
        if (relight) {
            const float p[3] = { out->fVx, out->fVy, out->fVz };
            float rgb[3];
            pc_light_vertex(pNaCnkCrntLighting->pLightTop, nl, p, nv, wrap, smooth, main_k, rgb);
            out->fIr = rgb[0]; out->fIg = rgb[1]; out->fIb = rgb[2];
        }
        if (pixel) { float* v = vnorm[first + i]; v[0] = nv[0]; v[1] = nv[1]; v[2] = nv[2]; v[3] = 1.0f; }
    }
}

/* ps2_NinjaCnk.c njCnkCvVMod: vertices made without a normal; their strips keep the vertex colour */
void pc_light_forget(int first, int n)
{
    if (!pc_light_pixel()) return;
    vnorm_grow(first + n);
    for (int i = 0; i < n; i++) vnorm[first + i][3] = 0.0f;
}

/* the view normal of vertex idx, 0 if none was kept */
int pc_light_normal(int idx, float n[3])
{
    if (idx < 0 || idx >= nvnorm || vnorm[idx][3] == 0.0f) return 0;
    n[0] = vnorm[idx][0]; n[1] = vnorm[idx][1]; n[2] = vnorm[idx][2];
    return 1;
}

/* the light set of a strip in colour mode (native_glue.c vertex_color) as stream.h lays it out
   (gs_gl.c fs_src light_sum): the frame's number for it, 0 for the modes that take no light */
uint32_t pc_light_set(int color_mode)
{
    float s[STREAM_LSET];
    const CNK_LIGHT* l = pNaCnkCrntLighting->pLightTop;
    int nl = chunk_lights > 6 ? 6 : chunk_lights;
    if (!pc_light_pixel() || color_mode == 1 || color_mode == 2) return 0;
    memset(s, 0, sizeof(s));
    s[0] = (float)color_mode; s[1] = (float)nl;
    s[4] = vu1Ambient.fR; s[5] = vu1Ambient.fG; s[6] = vu1Ambient.fB;
    s[8] = vu1Diffuse.fR; s[9] = vu1Diffuse.fG; s[10] = vu1Diffuse.fB;
    s[12] = vu1Specula.fR; s[13] = vu1Specula.fG; s[14] = vu1Specula.fB;
    if (pc_wet_active()) {   /* the amount, and the world's up in view space (cam.mtx's second row) */
        const float* m = (const float*)cam.mtx;
        float k = 1.0f / sqrtf(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
        s[2] = pc_light_wet() / 100.0f;
        s[7] = m[4] * k; s[11] = m[5] * k; s[15] = m[6] * k;
    }
    for (int k = 0; k < nl; k++, l++) {
        float* o = s + 16 + 12 * k;
        o[0] = l->fCx; o[1] = l->fCy; o[2] = l->fCz; o[3] = !l->ulState ? -1.0f : l->ulMode == 0 ? 0.0f : 1.0f;
        o[4] = l->fNearRR; o[5] = l->fFarRR; o[6] = l->fI;
        o[8] = l->fR; o[9] = l->fG; o[10] = l->fB;
    }
    if (!flash_made) flash_make();
    memcpy(s + 88, flash, sizeof(flash));
    return stream_light_set(s);
}

/* Enhanced fog's flashlight beam (fog_glue.c): this frame's flashlight (16 floats, as in the light
   sets, from texel 22); returns whether it shines */
int pc_flash_light(float* out)
{
    if (!flash_made) flash_make();
    memcpy(out, flash, sizeof(flash));
    return flash[3] > 0.0f;
}

/* Enhanced fog's light glow (fog_glue.c): the current lighting's point lights that are on, 8 floats
   each (view position, far range^2, colour, near range^2), at most max; returns the count */
int pc_light_points(float* out, int max)
{
    const CNK_LIGHT* l = pNaCnkCrntLighting->pLightTop;
    int n = 0;
    for (int k = 0; k < pNaCnkCrntLighting->lLightMax && n < max; k++, l++) {
        if (!l->ulState || l->ulMode == 0 || !(l->fFarRR > 0.0f)) continue;
        float* o = out + 8 * n++;
        o[0] = l->fCx; o[1] = l->fCy; o[2] = l->fCz; o[3] = l->fFarRR;
        o[4] = l->fR; o[5] = l->fG; o[6] = l->fB; o[7] = l->fNearRR;
    }
    return n;
}
