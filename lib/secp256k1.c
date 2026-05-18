#include "secp256k1.h"
#include <string.h>

/* constants _P_WORDS, _GX_WORDS, _GY_WORDS used via P, secp256k1_G */

static const uint256 P = { .v = { 0xFFFFFC2F, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } };
/* N (order) kept for reference: { 0xD0364141, 0xBFD25E8C, 0xAF48A03B, 0xBAAEDCE6, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } */
static const uint256 ONE = { .v = { 1, 0, 0, 0, 0, 0, 0, 0 } };
static const uint256 ZERO = { .v = { 0, 0, 0, 0, 0, 0, 0, 0 } };
static const uint256 INF = { .v = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } };

static inline void addc(uint32_t a, uint32_t b, uint32_t carryIn, uint32_t *sum, uint32_t *carryOut)
{
    uint64_t s = (uint64_t)a + b + carryIn;
    *sum = (uint32_t)s;
    *carryOut = (uint32_t)(s >> 32);
}

static inline void subc(uint32_t a, uint32_t b, uint32_t borrowIn, uint32_t *diff, uint32_t *borrowOut)
{
    uint64_t d = (uint64_t)a - b - borrowIn;
    *diff = (uint32_t)d;
    *borrowOut = (uint32_t)((d >> 32) & 1);
}

static bool less_than_equal(const uint32_t *a, const uint32_t *b, int len)
{
    for (int i = len - 1; i >= 0; i--) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return true;
}

static bool greater_than_equal(const uint32_t *a, const uint32_t *b, int len)
{
    for (int i = len - 1; i >= 0; i--) {
        if (a[i] > b[i]) return true;
        if (a[i] < b[i]) return false;
    }
    return true;
}

static uint32_t add_n(uint32_t *c, const uint32_t *a, const uint32_t *b, int len)
{
    uint32_t carry = 0;
    for (int i = 0; i < len; i++) {
        addc(a[i], b[i], carry, &c[i], &carry);
    }
    return carry;
}

static uint32_t sub_n(uint32_t *c, const uint32_t *a, const uint32_t *b, int len)
{
    uint32_t borrow = 0;
    for (int i = 0; i < len; i++) {
        subc(a[i], b[i], borrow, &c[i], &borrow);
    }
    return borrow;
}

static void multiply(const uint32_t *x, int xLen, const uint32_t *y, int yLen, uint32_t *z)
{
    for (int i = 0; i < xLen + yLen; i++) z[i] = 0;
    for (int i = 0; i < xLen; i++) {
        uint32_t high = 0;
        for (int j = 0; j < yLen; j++) {
            uint64_t product = (uint64_t)x[i] * y[j] + z[i + j] + high;
            z[i + j] = (uint32_t)product;
            high = (uint32_t)(product >> 32);
        }
        z[i + yLen] = high;
    }
}

static bool is_one(const uint256 *x)
{
    if (x->v[0] != 1) return false;
    for (int i = 1; i < 8; i++) if (x->v[i] != 0) return false;
    return true;
}

static bool is_even(const uint256 *x)
{
    return (x->v[0] & 1) == 0;
}

static uint256 div_by_2(const uint256 *x)
{
    uint256 r;
    for (int i = 0; i < 7; i++) r.v[i] = (x->v[i] >> 1) | (x->v[i + 1] << 31);
    r.v[7] = x->v[7] >> 1;
    return r;
}

bool uint256_is_zero(const uint256 *x)
{
    for (int i = 0; i < 8; i++) if (x->v[i] != 0) return false;
    return true;
}

bool uint256_bit(const uint256 *x, int n)
{
    n = n % 256;
    return (x->v[n / 32] & (1u << (n % 32))) != 0;
}

int uint256_cmp(const uint256 *a, const uint256 *b)
{
    for (int i = 7; i >= 0; i--) {
        if (a->v[i] < b->v[i]) return -1;
        if (a->v[i] > b->v[i]) return 1;
    }
    return 0;
}

uint256 uint256_add(const uint256 *a, const uint256 *b)
{
    uint256 r;
    add_n(r.v, a->v, b->v, 8);
    return r;
}

uint256 uint256_sub(const uint256 *a, const uint256 *b)
{
    uint256 r;
    sub_n(r.v, a->v, b->v, 8);
    return r;
}

void uint256_from_be_bytes(uint256 *k, const uint8_t bytes[32])
{
    for (int i = 0; i < 8; i++) {
        k->v[7 - i] = ((uint32_t)bytes[i*4] << 24)
                     | ((uint32_t)bytes[i*4+1] << 16)
                     | ((uint32_t)bytes[i*4+2] << 8)
                     | (uint32_t)bytes[i*4+3];
    }
}

void uint256_to_be_bytes(const uint256 *k, uint8_t bytes[32])
{
    for (int i = 0; i < 8; i++) {
        bytes[i*4]   = (uint8_t)(k->v[7 - i] >> 24);
        bytes[i*4+1] = (uint8_t)(k->v[7 - i] >> 16);
        bytes[i*4+2] = (uint8_t)(k->v[7 - i] >> 8);
        bytes[i*4+3] = (uint8_t)(k->v[7 - i]);
    }
}

static uint256 add_mod_p(const uint256 *a, const uint256 *b)
{
    uint256 sum;
    uint32_t overflow = add_n(sum.v, a->v, b->v, 8);
    if (overflow || greater_than_equal(sum.v, P.v, 8)) {
        sub_n(sum.v, sum.v, P.v, 8);
    }
    return sum;
}

static uint256 sub_mod_p(const uint256 *a, const uint256 *b)
{
    uint256 diff;
    if (sub_n(diff.v, a->v, b->v, 8)) {
        add_n(diff.v, diff.v, P.v, 8);
    }
    return diff;
}

static uint256 mul_mod_p(const uint256 *a, const uint256 *b)
{
    uint32_t product[16];
    multiply(a->v, 8, b->v, 8, product);

    uint32_t tmp[10] = {0};
    uint32_t tmp2[10] = {0};
    uint32_t s = 977;

    for (int i = 0; i < 8; i++) tmp2[1 + i] = product[8 + i];
    multiply(&s, 1, &product[8], 8, tmp);
    add_n(tmp, tmp, tmp2, 10);

    for (int i = 8; i < 16; i++) product[i] = 0;
    add_n(product, product, tmp, 10);

    for (int i = 0; i < 8; i++) tmp2[1 + i] = product[8 + i];
    multiply(&s, 1, &product[8], 8, tmp);
    add_n(tmp, tmp, tmp2, 10);

    uint32_t overflow = add_n(product, product, tmp, 8);
    if (overflow || greater_than_equal(product, P.v, 8)) {
        sub_n(product, product, P.v, 8);
    }

    uint256 result;
    for (int i = 0; i < 8; i++) result.v[i] = product[i];
    return result;
}

static uint256 inv_mod_p(const uint256 *x)
{
    uint256 u = *x;
    uint256 v = P;
    uint256 x1 = ONE;
    uint256 x2 = ZERO;
    int32_t x1_signed = 0;
    int32_t x2_signed = 0;

    while (!is_one(&u) && !is_one(&v)) {
        while (is_even(&u)) {
            u = div_by_2(&u);
            if (is_even(&x1)) {
                x1 = div_by_2(&x1);
                x1.v[7] |= ((uint32_t)(uint32_t)x1_signed & 1) << 31;
                x1_signed >>= 1;
            } else {
                uint32_t carry = add_n(x1.v, x1.v, P.v, 8);
                x1 = div_by_2(&x1);
                x1_signed += (int32_t)carry;
                x1.v[7] |= ((uint32_t)(uint32_t)x1_signed & 1) << 31;
                x1_signed >>= 1;
            }
        }
        while (is_even(&v)) {
            v = div_by_2(&v);
            if (is_even(&x2)) {
                x2 = div_by_2(&x2);
                x2.v[7] |= ((uint32_t)(uint32_t)x2_signed & 1) << 31;
                x2_signed >>= 1;
            } else {
                uint32_t carry = add_n(x2.v, x2.v, P.v, 8);
                x2 = div_by_2(&x2);
                x2_signed += (int32_t)carry;
                x2.v[7] |= ((uint32_t)(uint32_t)x2_signed & 1) << 31;
                x2_signed >>= 1;
            }
        }
        if (less_than_equal(v.v, u.v, 8)) {
            sub_n(u.v, u.v, v.v, 8);
            uint32_t borrow = sub_n(x1.v, x1.v, x2.v, 8);
            x1_signed -= x2_signed;
            x1_signed -= (int32_t)borrow;
        } else {
            sub_n(v.v, v.v, u.v, 8);
            uint32_t borrow = sub_n(x2.v, x2.v, x1.v, 8);
            x2_signed -= x1_signed;
            x2_signed -= (int32_t)borrow;
        }
    }

    uint256 out;
    if (is_one(&u)) {
        while (x1_signed < 0) { x1_signed += (int32_t)add_n(x1.v, x1.v, P.v, 8); }
        while (x1_signed > 0) { x1_signed -= (int32_t)sub_n(x1.v, x1.v, P.v, 8); }
        for (int i = 0; i < 8; i++) out.v[i] = x1.v[i];
    } else {
        while (x2_signed < 0) { x2_signed += (int32_t)add_n(x2.v, x2.v, P.v, 8); }
        while (x2_signed > 0) { x2_signed -= (int32_t)sub_n(x2.v, x2.v, P.v, 8); }
        for (int i = 0; i < 8; i++) out.v[i] = x2.v[i];
    }
    return out;
}

static bool is_point_at_infinity(const ecpoint *p)
{
    for (int i = 0; i < 8; i++) {
        if (p->x.v[i] != 0xFFFFFFFF) return false;
        if (p->y.v[i] != 0xFFFFFFFF) return false;
    }
    return true;
}

static ecpoint point_at_infinity(void)
{
    ecpoint p = { .x = INF, .y = INF };
    return p;
}

static ecpoint double_point(const ecpoint *p)
{
    uint256 y2 = add_mod_p(&p->y, &p->y);
    uint256 y_inv = inv_mod_p(&y2);
    uint256 x3 = mul_mod_p(&p->x, &p->x);
    uint256 x3_2 = add_mod_p(&x3, &x3);
    uint256 three_x2 = add_mod_p(&x3_2, &x3);
    uint256 s = mul_mod_p(&three_x2, &y_inv);
    uint256 s2 = mul_mod_p(&s, &s);
    uint256 t = sub_mod_p(&s2, &p->x);
    uint256 rx = sub_mod_p(&t, &p->x);
    uint256 px_minus_rx = sub_mod_p(&p->x, &rx);
    uint256 t2 = mul_mod_p(&s, &px_minus_rx);
    uint256 ry = sub_mod_p(&t2, &p->y);
    ecpoint result = { .x = rx, .y = ry };
    return result;
}

static ecpoint add_points(const ecpoint *p1, const ecpoint *p2)
{
    if (is_point_at_infinity(p1)) return *p2;
    if (is_point_at_infinity(p2)) return *p1;

    if (uint256_cmp(&p1->x, &p2->x) == 0) {
        if (uint256_cmp(&p1->y, &p2->y) == 0) return double_point(p1);
        return point_at_infinity();
    }

    uint256 rise = sub_mod_p(&p1->y, &p2->y);
    uint256 run = sub_mod_p(&p1->x, &p2->x);
    uint256 run_inv = inv_mod_p(&run);
    uint256 s = mul_mod_p(&rise, &run_inv);
    uint256 s2 = mul_mod_p(&s, &s);
    uint256 t = sub_mod_p(&s2, &p1->x);
    uint256 rx = sub_mod_p(&t, &p2->x);
    uint256 px_minus_rx = sub_mod_p(&p1->x, &rx);
    uint256 t2 = mul_mod_p(&s, &px_minus_rx);
    uint256 ry = sub_mod_p(&t2, &p1->y);
    ecpoint result = { .x = rx, .y = ry };
    return result;
}

ecpoint secp256k1_G(void)
{
    ecpoint g = {
        .x = { .v = { 0x16F81798, 0x59F2815B, 0x2DCE28D9, 0x029BFCDB, 0xCE870B07, 0x55A06295, 0xF9DCBBAC, 0x79BE667E } },
        .y = { .v = { 0xFB10D4B8, 0x9C47D08F, 0xA6855419, 0xFD17B448, 0x0E1108A8, 0x5DA4FBFC, 0x26A3C465, 0x483ADA77 } }
    };
    return g;
}

ecpoint secp256k1_multiply(const uint256 *k, const ecpoint *p)
{
    ecpoint sum = point_at_infinity();
    ecpoint d = *p;

    for (int i = 0; i < 256; i++) {
        if (uint256_bit(k, i)) {
            sum = add_points(&sum, &d);
        }
        d = double_point(&d);
    }
    return sum;
}
