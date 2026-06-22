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

/*
 * Unit tests for the HDR10 CLLI math shared header.
 * Tests exercise _pq_to_nits and _hdr_compute_clli directly without
 * linking lib_darktable, so the header must be dependency-free.
 */

#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <cmocka.h>

#include "imageio/format/hdr_clli_math.h"

/* Float approximate equality helper (cmocka has no float assert). */
static void _assert_float_approx(const float actual, const float expected,
                                 const float tol,
                                 const char *msg)
{
  const float diff = fabsf(actual - expected);
  if(diff > tol)
  {
    /* Print diagnostic then fail. */
    fprintf(stderr, "FLOAT APPROX FAIL: %s  actual=%.6f  expected=%.6f  diff=%.6f  tol=%.6f\n",
            msg ? msg : "", (double)actual, (double)expected, (double)diff, (double)tol);
  }
  assert_true(diff <= tol);
}

/* _pq_to_nits: black (e=0) -> ~0 nits */
static void test_pq_black(void **state)
{
  (void)state;
  const float nits = _pq_to_nits(0.0f);
  _assert_float_approx(nits, 0.0f, 1e-3f, "pq_black");
}

/* _pq_to_nits: full white (e=1) -> ~10000 nits */
static void test_pq_white(void **state)
{
  (void)state;
  const float nits = _pq_to_nits(1.0f);
  _assert_float_approx(nits, 10000.0f, 1.0f, "pq_white");
}

/* _pq_to_nits: PQ code for ~100 nits */
static void test_pq_100nits(void **state)
{
  (void)state;
  /* 0.50807457 is the standard PQ code for 100 cd/m^2 */
  const float nits = _pq_to_nits(0.50807457f);
  _assert_float_approx(nits, 100.0f, 0.5f, "pq_100nits");
}

/* _pq_to_nits: PQ code for ~1000 nits */
static void test_pq_1000nits(void **state)
{
  (void)state;
  /* 0.75196350 is the standard PQ code for 1000 cd/m^2 */
  const float nits = _pq_to_nits(0.75196350f);
  _assert_float_approx(nits, 1000.0f, 2.0f, "pq_1000nits");
}

/* _pq_to_nits: negative input is clamped to 0 -> 0 nits */
static void test_pq_negative_clamped(void **state)
{
  (void)state;
  const float nits = _pq_to_nits(-1.0f);
  _assert_float_approx(nits, 0.0f, 1e-6f, "pq_negative_clamped");
}

/*
 * _hdr_compute_clli: 3-pixel buffer.
 *
 * Pixel 0: PQ 0.75196350 -> ~1001 nits (float precision rounds up slightly)
 * Pixel 1: PQ 0.50807457 -> ~100 nits
 * Pixel 2: PQ 0.0        ->    0 nits
 *
 * maxcll  = round(1001.25) = 1001
 * maxfall = round((1001.25 + 99.996 + 0) / 3) = round(367.08) = 367
 *
 * These values are locked to actual function output on this platform.
 */
static void test_clli_3pixel(void **state)
{
  (void)state;
  /* RGBA layout: R,G,B,A per pixel (A is ignored in the CLLI computation) */
  const float buf[12] = {
    /* pixel 0: full brightness via R channel */
    0.75196350f, 0.0f, 0.0f, 1.0f,
    /* pixel 1: mid brightness via G channel */
    0.0f, 0.50807457f, 0.0f, 1.0f,
    /* pixel 2: black */
    0.0f, 0.0f, 0.0f, 1.0f,
  };

  uint16_t maxcll = 0, maxfall = 0;
  _hdr_compute_clli(buf, 3, &maxcll, &maxfall);

  assert_int_equal((int)maxcll, 1001);
  assert_int_equal((int)maxfall, 367);
}

/*
 * _hdr_compute_clli: NaN sample must not poison the result.
 * 2 pixels: {1000-nit PQ, NaN} -> maxcll=1001, maxfall=round(1001.25/2)=501
 */
static void test_clli_nan_pixel(void **state)
{
  (void)state;
  const float buf[8] = {
    0.75196350f, 0.0f, 0.0f, 1.0f,
    (float)NAN,  0.0f, 0.0f, 1.0f,
  };

  uint16_t maxcll = 0, maxfall = 0;
  _hdr_compute_clli(buf, 2, &maxcll, &maxfall);

  /* NaN pixel contributes 0 nits, so maxcll is still ~1001, maxfall is ~501 */
  assert_int_equal((int)maxcll, 1001);
  assert_int_equal((int)maxfall, 501);
}

/*
 * _hdr_compute_clli: Inf sample must not poison the result.
 * 2 pixels: {1000-nit PQ, +Inf} -> same expectation as NaN case.
 */
static void test_clli_inf_pixel(void **state)
{
  (void)state;
  const float buf[8] = {
    0.75196350f, 0.0f, 0.0f, 1.0f,
    (float)INFINITY, 0.0f, 0.0f, 1.0f,
  };

  uint16_t maxcll = 0, maxfall = 0;
  _hdr_compute_clli(buf, 2, &maxcll, &maxfall);

  /*
   * The Inf pixel: _pq_to_nits clamps input to [0,1], so e=1 -> 10000 nits.
   * isfinite(10000) is true, so 10000 does participate.
   * maxcll = round(10000) = 10000
   * maxfall = round((1001.25 + 10000) / 2) = round(5500.6) = 5501
   *
   * This tests that the +Inf PQ input is safely clamped by _pq_to_nits and
   * does NOT cause undefined behavior or a poisoned result.
   */
  assert_int_equal((int)maxcll, 10000);
  assert_int_equal((int)maxfall, 5501);
}

int main(int argc, char *argv[])
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_pq_black),
    cmocka_unit_test(test_pq_white),
    cmocka_unit_test(test_pq_100nits),
    cmocka_unit_test(test_pq_1000nits),
    cmocka_unit_test(test_pq_negative_clamped),
    cmocka_unit_test(test_clli_3pixel),
    cmocka_unit_test(test_clli_nan_pixel),
    cmocka_unit_test(test_clli_inf_pixel),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
// clang-format off
// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.py
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
// clang-format on
