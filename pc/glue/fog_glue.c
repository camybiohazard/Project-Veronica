/* Enhanced fog (docs/ARCHITECTURE.md "Fog"; g_config.fog_style 1): the native strips carry the
   game's fog as it is when they are drawn (ps2_NaFog.c: the 128-entry table njGenerateFogTable3
   made from the cut's range, the density, and the Fog intensity option, pc_fog_scale), so the
   shader works out the same fog power per pixel from the pixel's view depth (gs_gl.c fs_src
   fog_power) instead of between the vertices, and reshapes it there. Game thread. */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../src/gs/stream.h"
#include <string.h>

extern float fNaFogTbl[128];
extern float fNaFogDensity;
extern unsigned int ulNaFogState;
float pc_fog_scale(void);   /* host.c */
float njCalcFogPower(float fVSZ);   /* ps2_NaFog.c */
int pc_light_points(float* out, int max);   /* light_glue.c */
int pc_fog_enhanced(void);  /* host.c: the option */

extern float fVu1AspectW, fVu1AspectH, fVu1Projection, fVu1OffsetX, fVu1OffsetY, Ps2_zbuff_a, Ps2_zbuff_b;

/* for the glow: the projection (the shader finds any fogged pixel's view position from its depth and
   place, so every fogged draw glows alike, the stains on a wall as the wall) and the point lights the
   strip about to be drawn is lit by, into the frame's */
static void fog_lights(void)
{
    const float proj[6] = { fVu1OffsetX, fVu1OffsetY, fVu1AspectW * fVu1Projection, fVu1AspectH * fVu1Projection, Ps2_zbuff_a, Ps2_zbuff_b };
    float lights[8 * 8];
    int n = pc_light_points(lights, 8);
    stream_fog_view(proj);
    for (int k = 0; k < n; k++) stream_fog_light(lights + 8 * k);
}

/* the fog set of the strip about to be drawn (stream_fog_set), 0 none: the fog is off, or the
   option, or the frame's table of them is full (the vertex fog then); also the frame's fog for the
   ambient occlusion with any fog style */
int pc_fog_set(void)
{
    float set[STREAM_FSET];
    if (!ulNaFogState) return 0;
    memcpy(set, fNaFogTbl, sizeof(fNaFogTbl));
    set[128] = fNaFogDensity * pc_fog_scale();   /* as njCalcFogPower / njCalcFogPowerEx */
    set[129] = set[130] = set[131] = 0.0f;
    stream_fog_ao(set);   /* the AO fades in the frame's fog, whatever the fog style (the first one counts) */
    if (!pc_fog_enhanced()) return 0;
    fog_lights();
    return (int)stream_fog_set(set);
}

/* a vertex's fog (view depth vz) is the table's (njCalcFogPower): the CPU-built strips (pc_native_prim3d:
   the ordering-table models, Claire's hair) take a fog set only then, others keep their own fog */
int pc_fog_is_table(float vz, float fog)
{
    float d = njCalcFogPower(vz) - fog;
    return d > -0.5f && d < 0.5f;
}
