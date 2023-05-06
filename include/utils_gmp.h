#include <gmp.h>
#include "settings.h"

inline void gmp_add_assign(double &x, double y)
{
#ifdef ARGUS_HIGH_PRECISION
    mpf_t mpf_x, mpf_y;
    mpf_init_set_d(mpf_x, x);
    mpf_init_set_d(mpf_y, y);
    mpf_add(mpf_x, mpf_x, mpf_y);

    x = mpf_get_d(mpf_x);

    mpf_clear(mpf_x);
    mpf_clear(mpf_y);
#else 
    x += y;
#endif
}

inline void gmp_sub_assign(double &x, double y)
{
#ifdef ARGUS_HIGH_PRECISION
    mpf_t mpf_x, mpf_y;
    mpf_init_set_d(mpf_x, x);
    mpf_init_set_d(mpf_y, y);
    mpf_sub(mpf_x, mpf_x, mpf_y);

    x = mpf_get_d(mpf_x);

    mpf_clear(mpf_x);
    mpf_clear(mpf_y);
#else
    x -= y;
#endif
}

inline double gmp_mult(double x, double y)
{
#ifdef ARGUS_HIGH_PRECISION
    mpf_t mpf_x, mpf_y, mpf_res;
    mpf_init(mpf_res);
    mpf_init_set_d(mpf_x, x);
    mpf_init_set_d(mpf_y, y);
    mpf_mul(mpf_res, mpf_x, mpf_y);

    auto res_double = mpf_get_d(mpf_res);

    mpf_clear(mpf_x);
    mpf_clear(mpf_y);
    mpf_clear(mpf_res);

    return res_double;
#else
    return x * y;
#endif
}

inline double gmp_sub(double x, double y)
{
#ifdef ARGUS_HIGH_PRECISION
    mpf_t mpf_x, mpf_y, mpf_res;
    mpf_init(mpf_res);
    mpf_init_set_d(mpf_x, x);
    mpf_init_set_d(mpf_y, y);
    mpf_sub(mpf_res, mpf_x, mpf_y);

    auto res_double = mpf_get_d(mpf_res);

    mpf_clear(mpf_x);
    mpf_clear(mpf_y);
    mpf_clear(mpf_res);
    return res_double;
#else
    return x - y;
#endif
}