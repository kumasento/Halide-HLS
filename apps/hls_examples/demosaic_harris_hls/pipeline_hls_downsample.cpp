#include <iostream>
#include <math.h>
#include <float.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <hls_stream.h>
#include "Stencil.h"
#include "hls_target_downsample.h"
#ifndef HALIDE_ATTRIBUTE_ALIGN
  #ifdef _MSC_VER
    #define HALIDE_ATTRIBUTE_ALIGN(x) __declspec(align(x))
  #else
    #define HALIDE_ATTRIBUTE_ALIGN(x) __attribute__((aligned(x)))
  #endif
#endif
#ifndef BUFFER_T_DEFINED
#define BUFFER_T_DEFINED
#include <stdbool.h>
#include <stdint.h>
typedef struct buffer_t {
    uint64_t dev;
    uint8_t* host;
    int32_t extent[4];
    int32_t stride[4];
    int32_t min[4];
    int32_t elem_size;
    HALIDE_ATTRIBUTE_ALIGN(1) bool host_dirty;
    HALIDE_ATTRIBUTE_ALIGN(1) bool dev_dirty;
    HALIDE_ATTRIBUTE_ALIGN(1) uint8_t _padding[10 - sizeof(void *)];
} buffer_t;
#endif
struct halide_filter_metadata_t;
extern "C" {
void *halide_malloc(void *ctx, size_t);
void halide_free(void *ctx, void *ptr);
void *halide_print(void *ctx, const void *str);
void *halide_error(void *ctx, const void *str);
int halide_debug_to_file(void *ctx, const char *filename, void *data, int, int, int, int, int, int);
int halide_start_clock(void *ctx);
int64_t halide_current_time_ns(void *ctx);
void halide_profiler_pipeline_end(void *, void *);
}

#ifdef _WIN32
float roundf(float);
double round(double);
#else
inline float asinh_f32(float x) {return asinhf(x);}
inline float acosh_f32(float x) {return acoshf(x);}
inline float atanh_f32(float x) {return atanhf(x);}
inline double asinh_f64(double x) {return asinh(x);}
inline double acosh_f64(double x) {return acosh(x);}
inline double atanh_f64(double x) {return atanh(x);}
#endif
inline float sqrt_f32(float x) {return sqrtf(x);}
inline float sin_f32(float x) {return sinf(x);}
inline float asin_f32(float x) {return asinf(x);}
inline float cos_f32(float x) {return cosf(x);}
inline float acos_f32(float x) {return acosf(x);}
inline float tan_f32(float x) {return tanf(x);}
inline float atan_f32(float x) {return atanf(x);}
inline float sinh_f32(float x) {return sinhf(x);}
inline float cosh_f32(float x) {return coshf(x);}
inline float tanh_f32(float x) {return tanhf(x);}
inline float hypot_f32(float x, float y) {return hypotf(x, y);}
inline float exp_f32(float x) {return expf(x);}
inline float log_f32(float x) {return logf(x);}
inline float pow_f32(float x, float y) {return powf(x, y);}
inline float floor_f32(float x) {return floorf(x);}
inline float ceil_f32(float x) {return ceilf(x);}
inline float round_f32(float x) {return roundf(x);}

inline double sqrt_f64(double x) {return sqrt(x);}
inline double sin_f64(double x) {return sin(x);}
inline double asin_f64(double x) {return asin(x);}
inline double cos_f64(double x) {return cos(x);}
inline double acos_f64(double x) {return acos(x);}
inline double tan_f64(double x) {return tan(x);}
inline double atan_f64(double x) {return atan(x);}
inline double sinh_f64(double x) {return sinh(x);}
inline double cosh_f64(double x) {return cosh(x);}
inline double tanh_f64(double x) {return tanh(x);}
inline double hypot_f64(double x, double y) {return hypot(x, y);}
inline double exp_f64(double x) {return exp(x);}
inline double log_f64(double x) {return log(x);}
inline double pow_f64(double x, double y) {return pow(x, y);}
inline double floor_f64(double x) {return floor(x);}
inline double ceil_f64(double x) {return ceil(x);}
inline double round_f64(double x) {return round(x);}

inline float nan_f32() {return NAN;}
inline float neg_inf_f32() {return -INFINITY;}
inline float inf_f32() {return INFINITY;}
inline bool is_nan_f32(float x) {return x != x;}
inline bool is_nan_f64(double x) {return x != x;}
inline float float_from_bits(uint32_t bits) {
 union {
  uint32_t as_uint;
  float as_float;
 } u;
 u.as_uint = bits;
 return u.as_float;
}
inline int64_t make_int64(int32_t hi, int32_t lo) {
    return (((int64_t)hi) << 32) | (uint32_t)lo;
}
inline double make_float64(int32_t i0, int32_t i1) {
    union {
        int32_t as_int32[2];
        double as_double;
    } u;
    u.as_int32[0] = i0;
    u.as_int32[1] = i1;
    return u.as_double;
}

template<typename T> T max(T a, T b) {if (a > b) return a; return b;}
template<typename T> T min(T a, T b) {if (a < b) return a; return b;}
template<typename A, typename B> A reinterpret(B b) {A a; memcpy(&a, &b, sizeof(a)); return a;}

static bool halide_rewrite_buffer(buffer_t *b, int32_t elem_size,
                           int32_t min0, int32_t extent0, int32_t stride0,
                           int32_t min1, int32_t extent1, int32_t stride1,
                           int32_t min2, int32_t extent2, int32_t stride2,
                           int32_t min3, int32_t extent3, int32_t stride3) {
 b->min[0] = min0;
 b->min[1] = min1;
 b->min[2] = min2;
 b->min[3] = min3;
 b->extent[0] = extent0;
 b->extent[1] = extent1;
 b->extent[2] = extent2;
 b->extent[3] = extent3;
 b->stride[0] = stride0;
 b->stride[1] = stride1;
 b->stride[2] = stride2;
 b->stride[3] = stride3;
 return true;
}
#ifndef HALIDE_FUNCTION_ATTRS
#define HALIDE_FUNCTION_ATTRS
#endif
#ifdef __cplusplus
extern "C" {
#endif

int32_t halide_error_bad_elem_size(void *, const char *, const char *, int32_t, int32_t);
int32_t halide_error_access_out_of_bounds(void *, const char *, int32_t, int32_t, int32_t, int32_t, int32_t);
int32_t halide_error_constraint_violated(void *, const char *, int32_t, const char *, int32_t);
int32_t halide_error_buffer_allocation_too_large(void *, const char *, int64_t, int64_t);
int32_t halide_error_buffer_extents_too_large(void *, const char *, int64_t, int64_t);
int32_t halide_error_explicit_bounds_too_small(void *, const char *, const char *, int32_t, int32_t, int32_t, int32_t);

static int __pipeline_hls(buffer_t *_p2_input_buffer, buffer_t *_output_2_buffer) HALIDE_FUNCTION_ATTRS {
 uint8_t *_p2_input = (uint8_t *)(_p2_input_buffer->host);
 (void)_p2_input;
 const bool _p2_input_host_and_dev_are_null = (_p2_input_buffer->host == NULL) && (_p2_input_buffer->dev == 0);
 (void)_p2_input_host_and_dev_are_null;
 const int32_t _p2_input_min_0 = _p2_input_buffer->min[0];
 (void)_p2_input_min_0;
 const int32_t _p2_input_min_1 = _p2_input_buffer->min[1];
 (void)_p2_input_min_1;
 const int32_t _p2_input_min_2 = _p2_input_buffer->min[2];
 (void)_p2_input_min_2;
 const int32_t _p2_input_min_3 = _p2_input_buffer->min[3];
 (void)_p2_input_min_3;
 const int32_t _p2_input_extent_0 = _p2_input_buffer->extent[0];
 (void)_p2_input_extent_0;
 const int32_t _p2_input_extent_1 = _p2_input_buffer->extent[1];
 (void)_p2_input_extent_1;
 const int32_t _p2_input_extent_2 = _p2_input_buffer->extent[2];
 (void)_p2_input_extent_2;
 const int32_t _p2_input_extent_3 = _p2_input_buffer->extent[3];
 (void)_p2_input_extent_3;
 const int32_t _p2_input_stride_0 = _p2_input_buffer->stride[0];
 (void)_p2_input_stride_0;
 const int32_t _p2_input_stride_1 = _p2_input_buffer->stride[1];
 (void)_p2_input_stride_1;
 const int32_t _p2_input_stride_2 = _p2_input_buffer->stride[2];
 (void)_p2_input_stride_2;
 const int32_t _p2_input_stride_3 = _p2_input_buffer->stride[3];
 (void)_p2_input_stride_3;
 const int32_t _p2_input_elem_size = _p2_input_buffer->elem_size;
 (void)_p2_input_elem_size;
 uint8_t *_output_2 = (uint8_t *)(_output_2_buffer->host);
 (void)_output_2;
 const bool _output_2_host_and_dev_are_null = (_output_2_buffer->host == NULL) && (_output_2_buffer->dev == 0);
 (void)_output_2_host_and_dev_are_null;
 const int32_t _output_2_min_0 = _output_2_buffer->min[0];
 (void)_output_2_min_0;
 const int32_t _output_2_min_1 = _output_2_buffer->min[1];
 (void)_output_2_min_1;
 const int32_t _output_2_min_2 = _output_2_buffer->min[2];
 (void)_output_2_min_2;
 const int32_t _output_2_min_3 = _output_2_buffer->min[3];
 (void)_output_2_min_3;
 const int32_t _output_2_extent_0 = _output_2_buffer->extent[0];
 (void)_output_2_extent_0;
 const int32_t _output_2_extent_1 = _output_2_buffer->extent[1];
 (void)_output_2_extent_1;
 const int32_t _output_2_extent_2 = _output_2_buffer->extent[2];
 (void)_output_2_extent_2;
 const int32_t _output_2_extent_3 = _output_2_buffer->extent[3];
 (void)_output_2_extent_3;
 const int32_t _output_2_stride_0 = _output_2_buffer->stride[0];
 (void)_output_2_stride_0;
 const int32_t _output_2_stride_1 = _output_2_buffer->stride[1];
 (void)_output_2_stride_1;
 const int32_t _output_2_stride_2 = _output_2_buffer->stride[2];
 (void)_output_2_stride_2;
 const int32_t _output_2_stride_3 = _output_2_buffer->stride[3];
 (void)_output_2_stride_3;
 const int32_t _output_2_elem_size = _output_2_buffer->elem_size;
 (void)_output_2_elem_size;
 if (_output_2_host_and_dev_are_null)
 {
  bool _0 = halide_rewrite_buffer(_output_2_buffer, 1, 0, 720, 1, 0, 480, 720, 0, 0, 0, 0, 0, 0);
  (void)_0;
 } // if _output_2_host_and_dev_are_null
 if (_p2_input_host_and_dev_are_null)
 {
  bool _1 = halide_rewrite_buffer(_p2_input_buffer, 1, 193, 1454, 1, 33, 974, 1454, 0, 0, 0, 0, 0, 0);
  (void)_1;
 } // if _p2_input_host_and_dev_are_null
 bool _2 = _output_2_host_and_dev_are_null || _p2_input_host_and_dev_are_null;
 bool _3 = !(_2);
 if (_3)
 {
  bool _4 = _output_2_elem_size == 1;
  if (!_4)   {
   int32_t _5 = halide_error_bad_elem_size(NULL, "Output buffer output$2", "uint8", _output_2_elem_size, 1);
   return _5;
  }
  bool _6 = _p2_input_elem_size == 1;
  if (!_6)   {
   int32_t _7 = halide_error_bad_elem_size(NULL, "Input buffer p2:input", "uint8", _p2_input_elem_size, 1);
   return _7;
  }
  bool _8 = _output_2_min_0 <= 0;
  int32_t _9 = 720 - _output_2_extent_0;
  bool _10 = _9 <= _output_2_min_0;
  bool _11 = _8 && _10;
  if (!_11)   {
   int32_t _12 = _output_2_min_0 + _output_2_extent_0;
   int32_t _13 = _12 + -1;
   int32_t _14 = halide_error_access_out_of_bounds(NULL, "Output buffer output$2", 0, 0, 719, _output_2_min_0, _13);
   return _14;
  }
  bool _15 = _output_2_min_1 <= 0;
  int32_t _16 = 480 - _output_2_extent_1;
  bool _17 = _16 <= _output_2_min_1;
  bool _18 = _15 && _17;
  if (!_18)   {
   int32_t _19 = _output_2_min_1 + _output_2_extent_1;
   int32_t _20 = _19 + -1;
   int32_t _21 = halide_error_access_out_of_bounds(NULL, "Output buffer output$2", 1, 0, 479, _output_2_min_1, _20);
   return _21;
  }
  bool _22 = _p2_input_min_0 <= 193;
  int32_t _23 = 1647 - _p2_input_extent_0;
  bool _24 = _23 <= _p2_input_min_0;
  bool _25 = _22 && _24;
  if (!_25)   {
   int32_t _26 = _p2_input_min_0 + _p2_input_extent_0;
   int32_t _27 = _26 + -1;
   int32_t _28 = halide_error_access_out_of_bounds(NULL, "Input buffer p2:input", 0, 193, 1646, _p2_input_min_0, _27);
   return _28;
  }
  bool _29 = _p2_input_min_1 <= 33;
  int32_t _30 = 1007 - _p2_input_extent_1;
  bool _31 = _30 <= _p2_input_min_1;
  bool _32 = _29 && _31;
  if (!_32)   {
   int32_t _33 = _p2_input_min_1 + _p2_input_extent_1;
   int32_t _34 = _33 + -1;
   int32_t _35 = halide_error_access_out_of_bounds(NULL, "Input buffer p2:input", 1, 33, 1006, _p2_input_min_1, _34);
   return _35;
  }
  bool _36 = _output_2_stride_0 == 1;
  if (!_36)   {
   int32_t _37 = halide_error_constraint_violated(NULL, "output$2.stride.0", _output_2_stride_0, "1", 1);
   return _37;
  }
  bool _38 = _p2_input_stride_0 == 1;
  if (!_38)   {
   int32_t _39 = halide_error_constraint_violated(NULL, "p2:input.stride.0", _p2_input_stride_0, "1", 1);
   return _39;
  }
  int64_t _40 = (int64_t)(_output_2_extent_1);
  int64_t _41 = (int64_t)(_output_2_extent_0);
  int64_t _42 = _40 * _41;
  int64_t _43 = (int64_t)(_p2_input_extent_1);
  int64_t _44 = (int64_t)(_p2_input_extent_0);
  int64_t _45 = _43 * _44;
  int64_t _46 = (int64_t)(2147483647);
  bool _47 = _41 <= _46;
  if (!_47)   {
   int64_t _48 = (int64_t)(_output_2_extent_0);
   int64_t _49 = (int64_t)(2147483647);
   int32_t _50 = halide_error_buffer_allocation_too_large(NULL, "output$2", _48, _49);
   return _50;
  }
  int64_t _51 = (int64_t)(_output_2_extent_1);
  int64_t _52 = (int64_t)(_output_2_stride_1);
  int64_t _53 = _51 * _52;
  int64_t _54 = (int64_t)(2147483647);
  bool _55 = _53 <= _54;
  if (!_55)   {
   int64_t _56 = (int64_t)(_output_2_extent_1);
   int64_t _57 = (int64_t)(_output_2_stride_1);
   int64_t _58 = _56 * _57;
   int64_t _59 = (int64_t)(2147483647);
   int32_t _60 = halide_error_buffer_allocation_too_large(NULL, "output$2", _58, _59);
   return _60;
  }
  int64_t _61 = (int64_t)(2147483647);
  bool _62 = _42 <= _61;
  if (!_62)   {
   int64_t _63 = (int64_t)(2147483647);
   int32_t _64 = halide_error_buffer_extents_too_large(NULL, "output$2", _42, _63);
   return _64;
  }
  int64_t _65 = (int64_t)(_p2_input_extent_0);
  int64_t _66 = (int64_t)(2147483647);
  bool _67 = _65 <= _66;
  if (!_67)   {
   int64_t _68 = (int64_t)(_p2_input_extent_0);
   int64_t _69 = (int64_t)(2147483647);
   int32_t _70 = halide_error_buffer_allocation_too_large(NULL, "p2:input", _68, _69);
   return _70;
  }
  int64_t _71 = (int64_t)(_p2_input_extent_1);
  int64_t _72 = (int64_t)(_p2_input_stride_1);
  int64_t _73 = _71 * _72;
  int64_t _74 = (int64_t)(2147483647);
  bool _75 = _73 <= _74;
  if (!_75)   {
   int64_t _76 = (int64_t)(_p2_input_extent_1);
   int64_t _77 = (int64_t)(_p2_input_stride_1);
   int64_t _78 = _76 * _77;
   int64_t _79 = (int64_t)(2147483647);
   int32_t _80 = halide_error_buffer_allocation_too_large(NULL, "p2:input", _78, _79);
   return _80;
  }
  int64_t _81 = (int64_t)(2147483647);
  bool _82 = _45 <= _81;
  if (!_82)   {
   int64_t _83 = (int64_t)(2147483647);
   int32_t _84 = halide_error_buffer_extents_too_large(NULL, "p2:input", _45, _83);
   return _84;
  }
  {
   int64_t _85 = 1416196;
   uint8_t *__auto_insert__padded_2 = (uint8_t *)halide_malloc(NULL, sizeof(uint8_t)*_85);
   // produce __auto_insert__padded$2
   for (int __auto_insert__padded_2_s0_y = -7; __auto_insert__padded_2_s0_y < -7 + 974; __auto_insert__padded_2_s0_y++)
   {
    for (int __auto_insert__padded_2_s0_x = -7; __auto_insert__padded_2_s0_x < -7 + 1454; __auto_insert__padded_2_s0_x++)
    {
     int32_t _86 = __auto_insert__padded_2_s0_y * 1454;
     int32_t _87 = __auto_insert__padded_2_s0_x + _86;
     int32_t _88 = _87 + 10185;
     int32_t _89 = __auto_insert__padded_2_s0_y + 40;
     int32_t _90 = _89 * _p2_input_stride_1;
     int32_t _91 = __auto_insert__padded_2_s0_x + _90;
     int32_t _92 = _p2_input_min_1 * _p2_input_stride_1;
     int32_t _93 = _p2_input_min_0 + _92;
     int32_t _94 = _91 - _93;
     int32_t _95 = _94 + 200;
     uint8_t _96 = _p2_input[_95];
     __auto_insert__padded_2[_88] = _96;
    } // for __auto_insert__padded_2_s0_x
   } // for __auto_insert__padded_2_s0_y
   // consume __auto_insert__padded$2
   {
    int64_t _97 = 345600;
    uint8_t *_hw_output_2 = (uint8_t *)halide_malloc(NULL, sizeof(uint8_t)*_97);
    // produce hw_output$2
    hls::stream<AxiPackedStencil<uint8_t, 2, 1> > _padded_2_stencil_update_stream;
    (void)0;
    (void)0;
    // produce padded$2.stencil_update.stream
    for (int _padded_2_scan_update_y = 0; _padded_2_scan_update_y < 0 + 487*2; _padded_2_scan_update_y++)
    {
     for (int _padded_2_scan_update_x = 0; _padded_2_scan_update_x < 0 + 727; _padded_2_scan_update_x++)
     {
      Stencil<uint8_t, 2, 1> _padded_2_stencil;
      // produce padded$2.stencil
       for (int _padded_2_stencil_s0_x = 0; _padded_2_stencil_s0_x < 0 + 2; _padded_2_stencil_s0_x++)
       {
        int32_t _98 = _padded_2_scan_update_y;
        int32_t _99 = 0 + _98;
        int32_t _100 = _padded_2_scan_update_x * 2;
        int32_t _101 = _padded_2_stencil_s0_x + _100;
        int32_t _102 = _99 * 1454;
        int32_t _103 = _101 + _102;
        uint8_t _104 = __auto_insert__padded_2[_103];
        _padded_2_stencil(_padded_2_stencil_s0_x, 0) = _104;
       } // for _padded_2_stencil_s0_x
      // consume padded$2.stencil
      _padded_2_stencil_update_stream.write(_padded_2_stencil);
      (void)0;
     } // for _padded_2_scan_update_x
    } // for _padded_2_scan_update_y
    // consume padded$2.stencil_update.stream
    hls::stream<AxiPackedStencil<uint8_t, 1, 1> > __auto_insert__hw_output_2_stencil_stream;
    (void)0;
    (void)0;
    // produce _hls_target.__auto_insert__hw_output$2.stencil.stream
    hls_target(__auto_insert__hw_output_2_stencil_stream, _padded_2_stencil_update_stream);
    // consume _hls_target.__auto_insert__hw_output$2.stencil.stream
    for (int _hw_output_2_s0_y_yi = 0; _hw_output_2_s0_y_yi < 0 + 480; _hw_output_2_s0_y_yi++)
    {
     for (int _hw_output_2_s0_x_xi = 0; _hw_output_2_s0_x_xi < 0 + 720; _hw_output_2_s0_x_xi++)
     {
      Stencil<uint8_t, 1, 1> __auto_insert__hw_output_2_stencil;
      // produce __auto_insert__hw_output$2.stencil
      __auto_insert__hw_output_2_stencil = __auto_insert__hw_output_2_stencil_stream.read();
      (void)0;
      // consume __auto_insert__hw_output$2.stencil
      int32_t _398 = _hw_output_2_s0_y_yi * 720;
      int32_t _399 = _hw_output_2_s0_x_xi + _398;
      uint8_t _400 = __auto_insert__hw_output_2_stencil(0, 0);
      _hw_output_2[_399] = _400;
     } // for _hw_output_2_s0_x_xi
    } // for _hw_output_2_s0_y_yi
    // consume hw_output$2
    bool _401 = 0 <= _output_2_min_1;
    int32_t _402 = _output_2_min_1 + _output_2_extent_1;
    bool _403 = _402 <= 480;
    bool _404 = _401 && _403;
    if (!_404)     {
     int32_t _405 = _output_2_min_1 + _output_2_extent_1;
     int32_t _406 = _405 + -1;
     int32_t _407 = halide_error_explicit_bounds_too_small(NULL, "y", "output$2", 0, 479, _output_2_min_1, _406);
     return _407;
    }
    bool _408 = 0 <= _output_2_min_0;
    int32_t _409 = _output_2_min_0 + _output_2_extent_0;
    bool _410 = _409 <= 720;
    bool _411 = _408 && _410;
    if (!_411)     {
     int32_t _412 = _output_2_min_0 + _output_2_extent_0;
     int32_t _413 = _412 + -1;
     int32_t _414 = halide_error_explicit_bounds_too_small(NULL, "x", "output$2", 0, 719, _output_2_min_0, _413);
     return _414;
    }
    // produce output$2
    for (int _output_2_s0_y = 0; _output_2_s0_y < 0 + 480; _output_2_s0_y++)
    {
     for (int _output_2_s0_x = 0; _output_2_s0_x < 0 + 720; _output_2_s0_x++)
     {
      int32_t _415 = _output_2_s0_y * _output_2_stride_1;
      int32_t _416 = _output_2_s0_x + _415;
      int32_t _417 = _output_2_min_1 * _output_2_stride_1;
      int32_t _418 = _output_2_min_0 + _417;
      int32_t _419 = _416 - _418;
      int32_t _420 = _output_2_s0_y * 720;
      int32_t _421 = _output_2_s0_x + _420;
      uint8_t _422 = _hw_output_2[_421];
      _output_2[_419] = _422;
     } // for _output_2_s0_x
    } // for _output_2_s0_y
    // consume output$2
    halide_free(NULL, _hw_output_2);
   } // alloc _hw_output_2
   halide_free(NULL, __auto_insert__padded_2);
  } // alloc __auto_insert__padded_2
 } // if _3
 return 0;
}


int pipeline_hls(buffer_t *_p2_input_buffer, buffer_t *_output_2_buffer) HALIDE_FUNCTION_ATTRS {
 uint8_t *_p2_input = (uint8_t *)(_p2_input_buffer->host);
 (void)_p2_input;
 const bool _p2_input_host_and_dev_are_null = (_p2_input_buffer->host == NULL) && (_p2_input_buffer->dev == 0);
 (void)_p2_input_host_and_dev_are_null;
 const int32_t _p2_input_min_0 = _p2_input_buffer->min[0];
 (void)_p2_input_min_0;
 const int32_t _p2_input_min_1 = _p2_input_buffer->min[1];
 (void)_p2_input_min_1;
 const int32_t _p2_input_min_2 = _p2_input_buffer->min[2];
 (void)_p2_input_min_2;
 const int32_t _p2_input_min_3 = _p2_input_buffer->min[3];
 (void)_p2_input_min_3;
 const int32_t _p2_input_extent_0 = _p2_input_buffer->extent[0];
 (void)_p2_input_extent_0;
 const int32_t _p2_input_extent_1 = _p2_input_buffer->extent[1];
 (void)_p2_input_extent_1;
 const int32_t _p2_input_extent_2 = _p2_input_buffer->extent[2];
 (void)_p2_input_extent_2;
 const int32_t _p2_input_extent_3 = _p2_input_buffer->extent[3];
 (void)_p2_input_extent_3;
 const int32_t _p2_input_stride_0 = _p2_input_buffer->stride[0];
 (void)_p2_input_stride_0;
 const int32_t _p2_input_stride_1 = _p2_input_buffer->stride[1];
 (void)_p2_input_stride_1;
 const int32_t _p2_input_stride_2 = _p2_input_buffer->stride[2];
 (void)_p2_input_stride_2;
 const int32_t _p2_input_stride_3 = _p2_input_buffer->stride[3];
 (void)_p2_input_stride_3;
 const int32_t _p2_input_elem_size = _p2_input_buffer->elem_size;
 (void)_p2_input_elem_size;
 uint8_t *_output_2 = (uint8_t *)(_output_2_buffer->host);
 (void)_output_2;
 const bool _output_2_host_and_dev_are_null = (_output_2_buffer->host == NULL) && (_output_2_buffer->dev == 0);
 (void)_output_2_host_and_dev_are_null;
 const int32_t _output_2_min_0 = _output_2_buffer->min[0];
 (void)_output_2_min_0;
 const int32_t _output_2_min_1 = _output_2_buffer->min[1];
 (void)_output_2_min_1;
 const int32_t _output_2_min_2 = _output_2_buffer->min[2];
 (void)_output_2_min_2;
 const int32_t _output_2_min_3 = _output_2_buffer->min[3];
 (void)_output_2_min_3;
 const int32_t _output_2_extent_0 = _output_2_buffer->extent[0];
 (void)_output_2_extent_0;
 const int32_t _output_2_extent_1 = _output_2_buffer->extent[1];
 (void)_output_2_extent_1;
 const int32_t _output_2_extent_2 = _output_2_buffer->extent[2];
 (void)_output_2_extent_2;
 const int32_t _output_2_extent_3 = _output_2_buffer->extent[3];
 (void)_output_2_extent_3;
 const int32_t _output_2_stride_0 = _output_2_buffer->stride[0];
 (void)_output_2_stride_0;
 const int32_t _output_2_stride_1 = _output_2_buffer->stride[1];
 (void)_output_2_stride_1;
 const int32_t _output_2_stride_2 = _output_2_buffer->stride[2];
 (void)_output_2_stride_2;
 const int32_t _output_2_stride_3 = _output_2_buffer->stride[3];
 (void)_output_2_stride_3;
 const int32_t _output_2_elem_size = _output_2_buffer->elem_size;
 (void)_output_2_elem_size;
 int32_t _423 = __pipeline_hls(_p2_input_buffer, _output_2_buffer);
 bool _424 = _423 == 0;
 if (!_424)  {
  return _423;
 }
 return 0;
}
#ifdef __cplusplus
}  // extern "C"
#endif
