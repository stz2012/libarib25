#pragma once

#if defined(__ARM_NEON__) || defined(__ARM_NEON)

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) || (__BYTE_ORDER__) != (__ORDER_LITTLE_ENDIAN__)
	#error "Currently, USE_NEON is only for little-endian."
#endif

#include <utility>
#include <arm_neon.h>

#include "portable.h"

#include "multi2_block.h"

namespace multi2 {

namespace arm {

class neon {
private:
	uint32x4_t v;

public:
	inline neon() { }
	inline neon(uint32_t n) { v = vdupq_n_u32(n); }
	inline neon(const uint32x4_t &r) { v = r; }

	inline neon &operator=(const neon &other) {
		v = other.v;
		return *this;
	}

	inline neon operator+(const neon &other) const { return vaddq_u32(v, other.v); }
	inline neon operator-(const neon &other) const { return vsubq_u32(v, other.v); }
	inline neon operator^(const neon &other) const { return veorq_u32(v, other.v); }
	inline neon operator|(const neon &other) const { return vorrq_u32(v, other.v); }

	inline const uint32x4_t &value() const { return v; }
};

}

template<>
inline void block<arm::neon>::load(const uint8_t *p) {
	const uint32_t *q = reinterpret_cast<const uint32_t *>(p);
	uint32x4x2_t a = vld2q_u32(q);

	left  = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a.val[0])));
	right = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a.val[1])));
}

template<>
inline void block<arm::neon>::store(uint8_t *p) const {
	uint32x4_t a0 = left.value();
	uint32x4_t a1 = right.value();

	uint32x4_t b0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a0)));
	uint32x4_t b1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a1)));

	uint32_t *q = reinterpret_cast<uint32_t *>(p);
	uint32x4x2_t d = { b0, b1 };
	vst2q_u32(q, d);
}

template<>
inline std::pair<block<arm::neon>, cbc_state> block<arm::neon>::cbc_post_decrypt(const block<arm::neon> &c, const cbc_state &state) const {
	uint32x4_t c0 = c.left.value(); // 3 2 1 0
	uint32x4_t c1 = c.right.value();

	uint32_t s0 = vgetq_lane_u32(c0, 3); // 3
	uint32_t s1 = vgetq_lane_u32(c1, 3);

	uint32x4_t b0 = vextq_u32(c0, c0, 3); // 2 1 0 3
	uint32x4_t b1 = vextq_u32(c1, c1, 3);

	uint32x4_t x0 = vsetq_lane_u32(state.left,  b0, 0); // 2 1 0 s
	uint32x4_t x1 = vsetq_lane_u32(state.right, b1, 0);

	uint32x4_t d0 = left.value();  // 3 2 1 0
	uint32x4_t d1 = right.value();

	uint32x4_t p0 = veorq_u32(d0, x0);
	uint32x4_t p1 = veorq_u32(d1, x1);

	return std::make_pair(block<arm::neon>(p0, p1), cbc_state(s0, s1));
}

template<int N>
inline arm::neon rot(const arm::neon &v) {
	uint32x4_t a = v.value();
	if (N == 16) {
		return vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(a)));
	} else {
		return vsliq_n_u32(vshrq_n_u32(a, 32 - N), a, N);
	}
}

template<>
inline arm::neon rot1_sub<arm::neon>(const arm::neon &v) {
	uint32x4_t a = v.value();
	return vsraq_n_u32(a, a, 31);
}

template<>
inline arm::neon rot1_add_dec<arm::neon>(const arm::neon &v) {
	uint32x4_t d = vcgeq_s32(vreinterpretq_s32_u32(v.value()), vdupq_n_s32(0));
	return v + v + v + arm::neon(d);
}

}

#endif /* __ARM_NEON__ || __ARM_NEON */
