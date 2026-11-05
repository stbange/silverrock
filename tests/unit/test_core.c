#include "core/sr_fixed.h"
#include "core/sr_log.h"
#include "core/sr_math.h"
#include "core/sr_mem.h"
#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "sr_test.h"

#include <math.h>
#include <stdlib.h>

static void test_vec_ops(void) {
  SrVec3 a = sr_v3(1, 2, 3), b = sr_v3(4, 5, 6);
  SrVec3 c = sr_v3_add(a, b);
  SR_CHECK_NEAR(c.x, 5, 1e-6);
  c = sr_v3_sub(b, a);
  SR_CHECK_NEAR(c.z, 3, 1e-6);
  SR_CHECK_NEAR(sr_v3_dot(a, b), 32, 1e-6);
  c = sr_v3_cross(sr_v3(1, 0, 0), sr_v3(0, 1, 0));
  SR_CHECK_NEAR(c.z, 1, 1e-6);
  SR_CHECK_NEAR(sr_v3_len(sr_v3(3, 4, 0)), 5, 1e-6);
  SR_CHECK_NEAR(sr_v3_len(sr_v3_norm(b)), 1, 1e-6);
  c = sr_v3_norm(sr_v3(0, 0, 0));
  SR_CHECK_NEAR(c.x, 0, 1e-9);
  c = sr_v3_lerp(a, b, 0.5f);
  SR_CHECK_NEAR(c.y, 3.5, 1e-6);
  SR_CHECK_NEAR(sr_v2_len((SrVec2){6, 8}), 10, 1e-6);
  SR_CHECK_NEAR(sr_clampf(5, 0, 1), 1, 0);
  SR_CHECK_NEAR(sr_clampf(-5, 0, 1), 0, 0);
  SR_CHECK_NEAR(sr_lerpf(0, 10, 0.25f), 2.5, 1e-6);
  SR_CHECK_NEAR(sr_wrap_angle(3 * SR_PI), SR_PI, 1e-5);
  SR_CHECK_NEAR(sr_wrap_angle(-3 * SR_PI), -SR_PI, 1e-5);
}

static void test_matrices(void) {
  SrMat4 t = sr_m4_translate(sr_v3(1, 2, 3));
  SrVec4 p = sr_m4_mul_v4(t, (SrVec4){0, 0, 0, 1});
  SR_CHECK_NEAR(p.x, 1, 1e-6);
  SR_CHECK_NEAR(p.z, 3, 1e-6);
  SrMat4 s = sr_m4_scale(sr_v3(2, 2, 2));
  SrMat4 ts = sr_m4_mul(t, s);
  p = sr_m4_mul_v4(ts, (SrVec4){1, 1, 1, 1});
  SR_CHECK_NEAR(p.x, 3, 1e-6);
  SrMat4 r = sr_m4_rotate_y(SR_PI / 2);
  p = sr_m4_mul_v4(r, (SrVec4){1, 0, 0, 0});
  SR_CHECK_NEAR(p.z, -1, 1e-5);
  SrMat4 pr = sr_m4_perspective(SR_PI / 2, 1.0f, 0.1f, 100.0f);
  p = sr_m4_mul_v4(pr, (SrVec4){0, 0, -1, 1});
  SR_CHECK(p.w > 0);
  SrMat4 o = sr_m4_ortho(0, 10, 0, 10, -1, 1);
  p = sr_m4_mul_v4(o, (SrVec4){10, 10, 0, 1});
  SR_CHECK_NEAR(p.x, 1, 1e-6);
  /* Looking along -Z from origin: a point in front ends up at negative view z. */
  SrMat4 v = sr_m4_fps_view(sr_v3(0, 0, 0), 0, 0);
  p = sr_m4_mul_v4(v, (SrVec4){0, 0, -5, 1});
  SR_CHECK_NEAR(p.z, -5, 1e-5);
  SrVec3 d = sr_dir_from_yaw_pitch(0, 0);
  SR_CHECK_NEAR(d.z, -1, 1e-6);
  SrMat4 id = sr_m4_mul(sr_m4_identity(), v);
  SR_CHECK_NEAR(id.m[14], v.m[14], 1e-6);
}

static void test_fixed(void) {
  SR_CHECK_EQ(sr_fix_mul(SR_FIX(3), SR_FIX(4)), SR_FIX(12));
  SR_CHECK_EQ(sr_fix_div(SR_FIX(12), SR_FIX(4)), SR_FIX(3));
  SR_CHECK_EQ(sr_fix_div(SR_FIX(1), 0), INT32_MAX);
  SR_CHECK_EQ(sr_fix_div(SR_FIX(-1), 0), INT32_MIN);
  SR_CHECK_EQ(sr_fix_lerp(SR_FIX(0), SR_FIX(10), SR_FIX_ONE / 2), SR_FIX(5));
  SR_CHECK_EQ(sr_fix_to_int(SR_FIX(-3) + 1), -3);
  SR_CHECK_EQ(sr_isqrt64(0), 0);
  SR_CHECK_EQ(sr_isqrt64(99), 9);
  SR_CHECK_EQ(sr_isqrt64(100), 10);
  SR_CHECK_EQ(sr_isqrt64(1ull << 62), 1u << 31);
  SR_CHECK_EQ(sr_dist_i(0, 0, 300, 400), 500);
  SR_CHECK_EQ(sr_manhattan_i(5, 5, 2, 9), 7);
  SR_CHECK_EQ(sr_manhattan_i(2, 9, 5, 5), 7);
}

static void test_rng_golden(void) {
  /* Golden values: must be identical on every platform (checked in CI on all targets). */
  SrRng r;
  sr_rng_seed(&r, 42u, 54u);
  SR_CHECK_EQ(sr_rng_u32(&r), 0xa15c02b7u);
  SR_CHECK_EQ(sr_rng_u32(&r), 0x7b47f409u);
  SR_CHECK_EQ(sr_mix64(0), 0xe220a8397b1dcdafull);
  SR_CHECK_EQ(sr_hash_str("silverrock"), sr_hash_bytes(0, "silverrock", 10));
}

static void test_rng_streams(void) {
  SrRng a = sr_rng_stream(1, "city", 0, 0);
  SrRng b = sr_rng_stream(1, "city", 0, 0);
  SrRng c = sr_rng_stream(1, "npc", 0, 0);
  uint32_t x = sr_rng_u32(&a);
  SR_CHECK_EQ(x, sr_rng_u32(&b));
  SR_CHECK(x != sr_rng_u32(&c));
  int hist[3] = {0, 0, 0};
  for (int i = 0; i < 3000; i++) {
    int32_t v = sr_rng_range(&a, -1, 1);
    SR_CHECK(v >= -1 && v <= 1);
    hist[v + 1]++;
  }
  SR_CHECK(hist[0] > 800 && hist[1] > 800 && hist[2] > 800);
  SR_CHECK_EQ(sr_rng_range(&a, 5, 5), 5);
  SR_CHECK_EQ(sr_rng_range(&a, 5, 2), 5);
  int yes = 0;
  for (int i = 0; i < 1000; i++) yes += sr_rng_chance(&a, 250);
  SR_CHECK(yes > 180 && yes < 320);
  float f = sr_rng_float(&a);
  SR_CHECK(f >= 0.0f && f < 1.0f);
  int32_t w[3] = {0, 10, 0};
  SR_CHECK_EQ(sr_rng_weighted(&a, w, 3), 1);
  int32_t z[2] = {0, -1};
  SR_CHECK_EQ(sr_rng_weighted(&a, z, 2), 0);
  SR_CHECK(sr_hash_combine(1, 2) != sr_hash_combine(2, 1));
}

static void test_arena(void) {
  unsigned char mem[64];
  SrArena a;
  sr_arena_init(&a, mem, sizeof mem);
  void *p = sr_arena_alloc(&a, 10, 8);
  SR_CHECK(p != NULL);
  void *q = sr_arena_alloc(&a, 10, 8);
  SR_CHECK(((unsigned char *)q - mem) % 8 == 0);
  SR_CHECK(sr_arena_alloc(&a, 100, 1) == NULL);
  SR_CHECK(sr_arena_alloc(&a, 1, 0) != NULL);
  sr_arena_reset(&a);
  SR_CHECK_EQ(a.used, 0);
  a.fail_after = 1;
  SR_CHECK(sr_arena_alloc(&a, 1, 1) != NULL);
  SR_CHECK(sr_arena_alloc(&a, 1, 1) == NULL); /* injected failure */
}

static void test_writer_reader(void) {
  SrWriter w;
  sr_writer_init(&w);
  sr_put_u8(&w, 7);
  sr_put_u16(&w, 0xBEEF);
  sr_put_u32(&w, 0xDEADBEEFu);
  sr_put_u64(&w, 0x0123456789ABCDEFull);
  sr_put_i32(&w, -5);
  sr_put_i64(&w, -6);
  sr_put_f32(&w, 1.5f);
  sr_put_str(&w, "hello");
  size_t off = w.len;
  sr_put_u32(&w, 0);
  sr_patch_u32(&w, off, 99);
  sr_patch_u32(&w, w.len, 1);                              /* out of range: ignored */
  for (int i = 0; i < 300; i++) sr_put_u8(&w, (uint8_t)i); /* forces growth */
  SR_CHECK(!w.failed);

  SrReader r;
  sr_reader_init(&r, w.data, w.len);
  SR_CHECK_EQ(sr_get_u8(&r), 7);
  SR_CHECK_EQ(sr_get_u16(&r), 0xBEEF);
  SR_CHECK_EQ(sr_get_u32(&r), 0xDEADBEEFu);
  SR_CHECK(sr_get_u64(&r) == 0x0123456789ABCDEFull);
  SR_CHECK_EQ(sr_get_i32(&r), -5);
  SR_CHECK_EQ(sr_get_i64(&r), -6);
  SR_CHECK_NEAR(sr_get_f32(&r), 1.5, 0);
  char s[8];
  SR_CHECK(sr_get_str(&r, s, sizeof s));
  SR_CHECK_STR(s, "hello");
  SR_CHECK_EQ(sr_get_u32(&r), 99);
  SR_CHECK(sr_get_bytes(&r, NULL, 300));
  SR_CHECK_EQ(sr_get_u8(&r), 0); /* overrun */
  SR_CHECK(r.failed);

  /* string too long for the output buffer */
  sr_reader_init(&r, w.data + 1 + 2 + 4 + 8 + 4 + 8 + 4, 7);
  char tiny[3];
  SR_CHECK(!sr_get_str(&r, tiny, sizeof tiny));
  /* string length present but payload truncated */
  uint8_t trunc[3] = {5, 0, 'a'};
  sr_reader_init(&r, trunc, 3);
  SR_CHECK(!sr_get_str(&r, s, sizeof s));
  sr_writer_free(&w);

  /* injected allocation failure */
  sr_writer_init(&w);
  w.limit = 256;
  for (int i = 0; i < 300; i++) sr_put_u8(&w, 1);
  SR_CHECK(w.failed);
  sr_put_u8(&w, 1); /* ignored after failure */
  sr_writer_free(&w);
}

static void test_crc32(void) {
  SR_CHECK_EQ(sr_crc32("123456789", 9), 0xCBF43926u);
  SR_CHECK_EQ(sr_crc32("", 0), 0);
}

static void test_strings(void) {
  char buf[8];
  SR_CHECK_EQ(sr_strlcpy(buf, "abcdefghij", sizeof buf), 10);
  SR_CHECK_STR(buf, "abcdefg");
  SR_CHECK_EQ(sr_strlcpy(buf, "x", 0), 1);
  char n[32];
  sr_str_normalize("Sony-Corp. 2!", n, sizeof n);
  SR_CHECK_STR(n, "sonycorp2");
  SR_CHECK(sr_str_starts_with("hello", "he"));
  SR_CHECK(!sr_str_starts_with("hello", "lo"));
  int64_t v;
  SR_CHECK(sr_parse_i64("-42", &v) && v == -42);
  SR_CHECK(sr_parse_i64("+7", &v) && v == 7);
  SR_CHECK(!sr_parse_i64("", &v));
  SR_CHECK(!sr_parse_i64("-", &v));
  SR_CHECK(!sr_parse_i64("4x", &v));
  char line[] = "  set  player.cash   =  0 ";
  char *tok[8];
  SR_CHECK_EQ(sr_str_tokenize(line, tok, 8), 4);
  SR_CHECK_STR(tok[1], "player.cash");
  char t[] = "  trim me  ";
  sr_str_trim(t);
  SR_CHECK_STR(t, "trim me");
}

static int g_logged;
static char g_last[128];
static void sink(void *user, SrLogLevel level, const char *subsystem, const char *msg) {
  (void)user;
  g_logged++;
  sr_log_format(g_last, sizeof g_last, 1.0, level, subsystem, msg);
}

static void test_logging(void) {
  SR_SPEC("platform-runtime", "Recoverable error is logged");
  sr_log(SR_LOG_ERROR, "save", "no sink yet");
  sr_log_set_sink(sink, NULL);
  sr_log_set_level(SR_LOG_INFO);
  g_logged = 0;
  sr_log(SR_LOG_ERROR, "save", "write failed: %d", 3);
  SR_CHECK_EQ(g_logged, 1);
  SR_CHECK(strstr(g_last, "error") && strstr(g_last, "save") && strstr(g_last, "write failed: 3"));
  sr_log(SR_LOG_DEBUG, "x", "filtered");
  SR_CHECK_EQ(g_logged, 1);
  SR_CHECK_STR(sr_log_level_name(SR_LOG_WARN), "warning");
  SR_CHECK_STR(sr_log_level_name(SR_LOG_DEBUG), "debug");
  SR_CHECK_STR(sr_log_level_name(SR_LOG_INFO), "info");
  SR_CHECK_STR(sr_log_level_name((SrLogLevel)99), "?");
  sr_log_set_sink(NULL, NULL);
  for (int i = 0; i <= 8; i++) SR_CHECK(sr_result_name((SrResult)i)[0] != '\0');
  SR_CHECK_STR(sr_result_name((SrResult)99), "unknown");
}

void suite_core(void) {
  SR_RUN(test_vec_ops);
  SR_RUN(test_matrices);
  SR_RUN(test_fixed);
  SR_RUN(test_rng_golden);
  SR_RUN(test_rng_streams);
  SR_RUN(test_arena);
  SR_RUN(test_writer_reader);
  SR_RUN(test_crc32);
  SR_RUN(test_strings);
  SR_RUN(test_logging);
}
