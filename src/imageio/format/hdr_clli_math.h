/*
   This file is part of darktable,
   Copyright (C) 2024 darktable developers.

   darktable is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   darktable is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*
 * SMPTE ST 2084 (PQ) EOTF: map a normalized PQ-encoded value in [0, 1] to
 * absolute luminance in cd/m^2 (nits), peak white = 10000 nits.
 *
 * Shared by avif.c and heif.c (HDR10 export) and by the unit tests, so the
 * single source of truth is covered directly. Intentionally free of darktable
 * dependencies (only libc math) so the tests link without lib_darktable.
 */
static inline float _pq_to_nits(const float e)
{
  const float m1 = 0.1593017578125f;   /* 2610 / 16384      */
  const float m2 = 78.84375f;          /* 2523 / 4096 * 128 */
  const float c1 = 0.8359375f;         /* 3424 / 4096       */
  const float c2 = 18.8515625f;        /* 2413 / 4096 * 32  */
  const float c3 = 18.6875f;           /* 2392 / 4096 * 32  */

  const float ec = fmaxf(0.0f, fminf(e, 1.0f));
  const float ep = powf(ec, 1.0f / m2);
  const float num = fmaxf(ep - c1, 0.0f);
  const float den = c2 - c3 * ep;
  if(den <= 0.0f) return 10000.0f;
  return 10000.0f * powf(num / den, 1.0f / m1);
}

/*
 * Compute HDR10 content light levels (CLLI) from an interleaved RGBA float
 * buffer of PQ-encoded, absolute-luminance samples (4 floats per pixel):
 *   *out_maxcll  = brightest single sample (max RGB component), in nits,
 *   *out_maxfall = mean over all pixels of that per-pixel peak, in nits.
 * Non-finite samples are ignored (treated as 0) so they cannot poison the
 * MaxFALL sum or make the final 16-bit cast undefined.
 */
static inline void _hdr_compute_clli(const float *const rgba,
                                     const size_t npixels,
                                     uint16_t *const out_maxcll,
                                     uint16_t *const out_maxfall)
{
  float max_cll = 0.0f;
  double sum_fall = 0.0;

#ifdef _OPENMP
#pragma omp parallel for simd \
    reduction(max : max_cll) reduction(+ : sum_fall) schedule(static)
#endif
  for(size_t k = 0; k < npixels; k++)
  {
    const float *const px = &rgba[4 * k];
    const float e_max = fmaxf(fmaxf(px[0], px[1]), px[2]);
    const float nits_raw = _pq_to_nits(e_max);
    const float nits = isfinite(nits_raw) ? nits_raw : 0.0f;
    max_cll = fmaxf(max_cll, nits);
    sum_fall += nits;
  }
  const float max_fall = npixels ? (float)(sum_fall / (double)npixels) : 0.0f;

  const float cll_c  = fmaxf(0.0f, fminf(roundf(max_cll),  65535.0f));
  const float fall_c = fmaxf(0.0f, fminf(roundf(max_fall), 65535.0f));
  *out_maxcll  = (uint16_t)cll_c;
  *out_maxfall = (uint16_t)fall_c;
}
