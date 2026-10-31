/* Modern controls: the direction math (pc/src/host/modern.h), pure; the game side is
   pc/glue/modern_glue.c */
#include "../host/modern.h"
#include <math.h>

int g_modern_on = 0;   /* first-start default: classic (tank); recvx.ini overrides */

#define UNITS (32768.0f / 3.14159265f)

float modern_wrap(float a)
{
    a = fmodf(a, 65536.0f);
    return a > 32768.0f ? a - 65536.0f : a < -32768.0f ? a + 65536.0f : a;
}

/* ponytail: by the camera's orientation, as the HD remasters, not the exact on-screen direction
   at the player's place: far off-centre in a wide shot, "up" runs a few degrees off screen-up
   under perspective; project the ground axes at the player if that ever matters */
float modern_target(float rx, float rz, float sx, float sy)
{
    float n = sqrtf(rx * rx + rz * rz), dx, dz;
    if (n < 1e-6f) rx = 1, rz = 0, n = 1;   /* no camera: as one looking along -z */
    rx /= n; rz /= n;
    /* screen up on the ground: the right axis turned a quarter towards the view, (rz, -rx) */
    dx = sx * rx + sy * rz;
    dz = sx * rz - sy * rx;
    return atan2f(-dx, -dz) * UNITS;
}

void modern_hold(ModernHold* h, float stick, int cut, float* rx, float* rz)
{
    if (!h->held || h->cut == cut || fabsf(modern_wrap(stick - h->ref)) > MODERN_RELEASE) {
        h->held = 1; h->cut = cut; h->rx = *rx; h->rz = *rz; h->ref = stick;
        return;
    }
    *rx = h->rx; *rz = h->rz;
}

float modern_turn(float ay, float target)
{
    float d = modern_wrap(target - ay) * 0.4f, max = MODERN_DEG(30.0f);
    return d > max ? max : d < -max ? -max : d;
}

float modern_stick(float* x, float* y, float dead, float curve)
{
    float m = sqrtf(*x * *x + *y * *y), k;
    if (m <= dead || m < 1e-6f) { *x = *y = 0; return 0; }
    k = ((m > 1 ? 1 : m) - dead) / (1 - dead);
    k = powf(k, curve);
    *x *= k / m; *y *= k / m;
    return k;
}

void modern_assist(float off_yaw, float off_pitch, float* dyaw, float* dpitch)
{
    if (off_yaw * off_yaw + off_pitch * off_pitch > MODERN_ASSIST_CONE * MODERN_ASSIST_CONE) return;
    *dyaw *= 0.5f; *dpitch *= 0.5f;
    if ((*dyaw != 0 || *dpitch != 0) && *dyaw * off_yaw + *dpitch * off_pitch >= 0) {
        *dyaw += off_yaw * 0.1f; *dpitch += off_pitch * 0.1f;
    }
}
