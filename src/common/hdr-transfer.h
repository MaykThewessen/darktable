/*
 * HDR transfer functions for the CPU color pipeline. The OpenCL counterpart
 * is data/kernels/hdr_transfer.h; keep both files in sync.
 * Copyright (C) 2026 darktable developers.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * PQ (SMPTE ST 2084) and HLG (ITU-R BT.2100) transfer functions.
 *
 * Linear 1.0 is the BT.2408 reference white of 203 nits. The functions are
 * odd: f(-x) = -f(x), so negative channels survive a round trip. They do
 * not clip at signal 1.0; consumers that need [0, 1] clamp themselves.
 * NaN gives 0.
 */
#pragma once

#include <float.h>

#include "common/math.h"

#define DT_HDR_NONE 0
#define DT_HDR_PQ 1
#define DT_HDR_HLG_REC2020 2
#define DT_HDR_HLG_P3 3

/* BT.2408 reference white; BT.2100 nominal HLG display, zero black. */
#define DT_HDR_REFERENCE_WHITE_NITS 203.0f
#define DT_HDR_PQ_PEAK_NITS 10000.0f
#define DT_HDR_HLG_PEAK_NITS 1000.0f
#define DT_HDR_HLG_SYSTEM_GAMMA 1.2f
#define DT_HDR_PQ_M1 (2610.0f / 16384.0f)
#define DT_HDR_PQ_M2 (2523.0f / 32.0f)
#define DT_HDR_PQ_C1 (3424.0f / 4096.0f)
#define DT_HDR_PQ_C2 (2413.0f / 128.0f)
#define DT_HDR_PQ_C3 (2392.0f / 128.0f)
#define DT_HDR_HLG_A 0.17883277f
#define DT_HDR_HLG_B 0.28466892f
#define DT_HDR_HLG_C 0.55991073f

/*
 * The PQ decode has a pole where C2 - C3 * signal^(1/M2) reaches 0, at
 * signal (C2 / C3)^M2 = 1.992, which the encode approaches as linear goes to
 * infinity. The decode clamps the signal to 1.98, short of the pole: there
 * it returns 5.3e14 times reference white, stays finite and monotonic, and
 * float still resolves the denominator to better than 1e-5.
 */
#define DT_HDR_PQ_MAX_SIGNAL 1.98f

/* 1 / C1^M2 as a float pair; C1^M2 = 7.31e-7 is the signal of PQ black */
#define DT_HDR_PQ_BLACK_SCALE 1368071.625f
#define DT_HDR_PQ_BLACK_SCALE_LO -0.040281184f
/* below this signal, 7.1e-7 nits, the decode works relative to black */
#define DT_HDR_PQ_DARK_SIGNAL 1e-4f

static inline float dt_hdr_pq_decode(const float signal)
{
  // fmaxf() maps NaN to 0
  const float e = fminf(fmaxf(fabsf(signal), 0.0f), DT_HDR_PQ_MAX_SIGNAL);
  /*
   * p = e^(1/M2) lies in [C1, 1.0087] and a float holds it to 6e-8. Both
   * p - C1 (0 at black) and C2 - C3 p (0.16 at signal 1, from C3 p = 18.7)
   * cancel, and the final power 1/M1 = 6.3 amplifies what is left. Take
   * x = p / s - 1 from expm1() instead: s = 1 in general, s = C1 near black,
   * where e / C1^M2 - 1 comes from an fma with the split scale. With
   * C2 - C3 = 1 - C1:
   *   s = 1:   n = (1 - C1) + x   d = (1 - C1) - C3 x
   *   s = C1:  n = C1 x           d = (C2 - C3 C1) - C3 C1 x
   * The -0.5 keeps log1p() finite; signals below half of black decode to 0.
   */
  const int dark = e < DT_HDR_PQ_DARK_SIGNAL;
  const float l = dark
    ? log1pf(fmaxf(fmaf(e, DT_HDR_PQ_BLACK_SCALE, -1.0f) + e * DT_HDR_PQ_BLACK_SCALE_LO, -0.5f))
    : logf(e);
  const float x = expm1f(l * (1.0f / DT_HDR_PQ_M2));
  const float n = fmaxf(dark ? DT_HDR_PQ_C1 * x : (1.0f - DT_HDR_PQ_C1) + x, 0.0f);
  const float d = dark ? (DT_HDR_PQ_C2 - DT_HDR_PQ_C3 * DT_HDR_PQ_C1) - (DT_HDR_PQ_C3 * DT_HDR_PQ_C1) * x
                       : (1.0f - DT_HDR_PQ_C1) - DT_HDR_PQ_C3 * x;
  return copysignf((DT_HDR_PQ_PEAK_NITS / DT_HDR_REFERENCE_WHITE_NITS)
                   * powf(n / d, 1.0f / DT_HDR_PQ_M1), signal);
}

static inline float dt_hdr_pq_encode(const float linear)
{
  // fmaxf() maps NaN to 0, fminf() keeps p finite for infinite linear
  const float l = fminf(fmaxf(fabsf(linear), 0.0f), FLT_MAX)
    * (DT_HDR_REFERENCE_WHITE_NITS / DT_HDR_PQ_PEAK_NITS);
  const float p = powf(l, DT_HDR_PQ_M1);
  /*
   * (C1 + C2 p) / (1 + C3 p) - 1 = (C1 - 1)(1 - p) / (1 + C3 p), since
   * C2 - C3 = 1 - C1. Raising the ratio itself to M2 = 78.8 amplifies its
   * rounding; log1p() of the difference does not, which keeps the encode
   * monotonic.
   */
  const float q = (DT_HDR_PQ_C1 - 1.0f)
    * ((1.0f + 1.0f / DT_HDR_PQ_C3) / (1.0f + DT_HDR_PQ_C3 * p) - 1.0f / DT_HDR_PQ_C3);
  return copysignf(expf(DT_HDR_PQ_M2 * log1pf(q)), linear);
}

static inline float dt_hdr_hlg_decode(const float signal)
{
  const float e = fmaxf(fabsf(signal), 0.0f);
  return copysignf(e <= 0.5f ? e * e / 3.0f
    : (expf((e - DT_HDR_HLG_C) / DT_HDR_HLG_A) + DT_HDR_HLG_B) / 12.0f, signal);
}

static inline float dt_hdr_hlg_encode(const float scene)
{
  const float e = fmaxf(fabsf(scene), 0.0f);
  return copysignf(e <= 1.0f / 12.0f ? sqrtf(3.0f * e)
    : DT_HDR_HLG_A * logf(12.0f * e - DT_HDR_HLG_B) + DT_HDR_HLG_C, scene);
}

static inline float dt_hdr_hlg_luminance(const float rgb[3], const int transfer)
{
  /* D65 luminance coefficients, before chromatic adaptation to D50. */
  return transfer == DT_HDR_HLG_P3
    ? 0.22897456f * rgb[0] + 0.69173852f * rgb[1] + 0.07928691f * rgb[2]
    : 0.2627f * rgb[0] + 0.6780f * rgb[1] + 0.0593f * rgb[2];
}

/* takes display light of luminance y to HLG scene light: the inverse of
   the BT.2100 OOTF. the gain diverges at y = 0; that pixel, and NaN, go
   to black */
static inline float dt_hdr_hlg_scene_scale(const float y)
{
  return y > 0.0f
    ? powf(DT_HDR_REFERENCE_WHITE_NITS / DT_HDR_HLG_PEAK_NITS,
                 1.0f / DT_HDR_HLG_SYSTEM_GAMMA)
      * powf(y, 1.0f / DT_HDR_HLG_SYSTEM_GAMMA - 1.0f)
    : 0.0f;
}

/*
 * The HLG OOTF scales all channels by a gain driven by the luminance Y. The
 * gain uses |Y|, so rgb and -rgb map to opposite values and pixels with
 * negative channels round trip as long as Y is not 0. A pixel with Y = 0
 * (only possible with negative channels) maps to black both ways.
 */
static inline void dt_hdr_decode(dt_aligned_pixel_t rgb, const int transfer)
{
  if(transfer == DT_HDR_PQ)
  {
    for_three_channels(c) rgb[c] = dt_hdr_pq_decode(rgb[c]);
  }
  else if(transfer != DT_HDR_NONE)
  {
    for_three_channels(c) rgb[c] = dt_hdr_hlg_decode(rgb[c]);
    const float y = fabsf(dt_hdr_hlg_luminance(rgb, transfer));
    const float scale = (DT_HDR_HLG_PEAK_NITS / DT_HDR_REFERENCE_WHITE_NITS)
      * powf(y, DT_HDR_HLG_SYSTEM_GAMMA - 1.0f);
    for_three_channels(c) rgb[c] *= scale;
  }
}

static inline void dt_hdr_encode(dt_aligned_pixel_t rgb, const int transfer)
{
  if(transfer == DT_HDR_PQ)
  {
    for_three_channels(c) rgb[c] = dt_hdr_pq_encode(rgb[c]);
  }
  else if(transfer != DT_HDR_NONE)
  {
    const float scale = dt_hdr_hlg_scene_scale(fabsf(dt_hdr_hlg_luminance(rgb, transfer)));
    for_three_channels(c) rgb[c] = dt_hdr_hlg_encode(rgb[c] * scale);
  }
}
