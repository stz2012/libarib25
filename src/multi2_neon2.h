#pragma once

#if defined(__ARM_NEON__) || defined(__ARM_NEON)

#include <utility>
#include <arm_neon.h>

#include "portable.h"

#include "multi2_block.h"

namespace multi2 {

namespace arm {

template<size_t S>
class neon2 {
private:
	uint32x4_t v0;
	uint32x4_t v1;

public:
	inline neon2() { }
	inline neon2(uint32_t n) { v0 = v1 = vdupq_n_u32(n); }
	inline neon2(const uint32x4_t &r0, const uint32x4_t &r1) {
		v0 = r0;
		v1 = r1;
	}

	inline neon2 &operator=(const neon2 &other) {
		v0 = other.v0;
		v1 = other.v1;
		return *this;
	}

	inline neon2 operator+(const neon2 &other) const {
		return neon2(vaddq_u32(v0, other.v0), vaddq_u32(v1, other.v1));
	}
	inline neon2 operator-(const neon2 &other) const {
		return neon2(vsubq_u32(v0, other.v0), vsubq_u32(v1, other.v1));
	}
	inline neon2 operator^(const neon2 &other) const {
		return neon2(veorq_u32(v0, other.v0), veorq_u32(v1, other.v1));
	}
	inline neon2 operator|(const neon2 &other) const {
		return neon2(vorrq_u32(v0, other.v0), vorrq_u32(v1, other.v1));
	}

	inline const uint32x4_t &value0() const { return v0; }
	inline const uint32x4_t &value1() const { return v1; }

	static inline void load_block(block<neon2> &b, const uint8_t *p) {
		size_t d = 8 - (8 - S) * 2;
		const uint32_t *q = reinterpret_cast<const uint32_t *>(p);
		uint32x4x2_t a0 = vld2q_u32(q);
		uint32x4x2_t a1 = vld2q_u32(q + d);

		uint32x4_t b0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a0.val[0])));
		uint32x4_t b1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a0.val[1])));
		uint32x4_t b2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a1.val[0])));
		uint32x4_t b3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a1.val[1])));

		b.left  = neon2(b0, b2);
		b.right = neon2(b1, b3);
	}

	static inline void store_block(uint8_t *p, const block<neon2> &b) {
		uint32x4_t a0 = b.left.value0();
		uint32x4_t a1 = b.right.value0();
		uint32x4_t a2 = b.left.value1();
		uint32x4_t a3 = b.right.value1();

		uint32x4_t b0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a0)));
		uint32x4_t b1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a1)));
		uint32x4_t b2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a2)));
		uint32x4_t b3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(a3)));

		size_t d = 8 - (8 - S) * 2;
		uint32_t *q = reinterpret_cast<uint32_t *>(p);
		uint32x4x2_t e0 = { b0, b1 };
		uint32x4x2_t e1 = { b2, b3 };
		vst2q_u32(q + d, e1);
		vst2q_u32(q,     e0);
	}

	static inline std::pair<block<neon2>, cbc_state> cbc_post_decrypt(const block<neon2> &d, const block<neon2> &c, const cbc_state &state) {
		uint32x4_t c0 = c.left.value0(); // 3 2 1 0
		uint32x4_t c1 = c.right.value0();
		uint32x4_t c2 = c.left.value1(); // d c b a
		uint32x4_t c3 = c.right.value1();

		uint32_t s2 = vgetq_lane_u32(c2, 3); // d
		uint32_t s3 = vgetq_lane_u32(c3, 3);

		uint32x4_t b0 = vextq_u32(c0, c0, 3); // 2 1 0 3
		uint32x4_t b1 = vextq_u32(c1, c1, 3);
		uint32x4_t b2 = vextq_u32(c0, c2, 3); // c b a 3
		uint32x4_t b3 = vextq_u32(c1, c3, 3);

		uint32x4_t x0 = vsetq_lane_u32(state.left,  b0, 0); // 2 1 0 s
		uint32x4_t x1 = vsetq_lane_u32(state.right, b1, 0);
		uint32x4_t x2 = b2;                                 // c b a 3
		uint32x4_t x3 = b3;

		uint32x4_t d0 = d.left.value0();  // 3 2 1 0
		uint32x4_t d1 = d.right.value0();
		uint32x4_t d2 = d.left.value1();  // d c b a
		uint32x4_t d3 = d.right.value1();

		uint32x4_t p0 = veorq_u32(d0, x0); // 3 2 1 0
		uint32x4_t p1 = veorq_u32(d1, x1);
		uint32x4_t p2 = veorq_u32(d2, x2); // d c b a?
		uint32x4_t p3 = veorq_u32(d3, x3);

		return std::make_pair(block<neon2>(neon2(p0, p2), neon2(p1, p3)), cbc_state(s2, s3));
	}
};

}

template<>
inline size_t block_size<arm::neon2<7> >() {
	return 56;
}

template<>
inline size_t block_size<arm::neon2<8> >() {
	return 64;
}

template<>
inline void block<arm::neon2<7> >::load(const uint8_t *p) {
	arm::neon2<7>::load_block(*this, p);
}

template<>
inline void block<arm::neon2<8> >::load(const uint8_t *p) {
	arm::neon2<8>::load_block(*this, p);
}

template<>
inline void block<arm::neon2<7> >::store(uint8_t *p) const {
	arm::neon2<7>::store_block(p, *this);
}

template<>
inline void block<arm::neon2<8> >::store(uint8_t *p) const {
	arm::neon2<8>::store_block(p, *this);
}

template<>
inline std::pair<block<arm::neon2<7> >, cbc_state> block<arm::neon2<7> >::cbc_post_decrypt(const block<arm::neon2<7> > &c, const cbc_state &state) const {
	return arm::neon2<7>::cbc_post_decrypt(*this, c, state);
}

template<>
inline std::pair<block<arm::neon2<8> >, cbc_state> block<arm::neon2<8> >::cbc_post_decrypt(const block<arm::neon2<8> > &c, const cbc_state &state) const {
	return arm::neon2<8>::cbc_post_decrypt(*this, c, state);
}

template<int N, size_t S>
inline arm::neon2<S> rot(const arm::neon2<S> &v) {
	uint32x4_t a0 = v.value0();
	uint32x4_t a1 = v.value1();
	if (N == 16) {
		uint32x4_t b0 = vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(a0)));
		uint32x4_t b1 = vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(a1)));
		return arm::neon2<S>(b0, b1);
	} else {
		uint32x4_t b0 = vshrq_n_u32(a0, 32 - N);
		uint32x4_t b1 = vshrq_n_u32(a1, 32 - N);

		uint32x4_t c0 = vsliq_n_u32(b0, a0, N);
		uint32x4_t c1 = vsliq_n_u32(b1, a1, N);
		return arm::neon2<S>(c0, c1);
	}
}

template<size_t S>
inline arm::neon2<S> rot1_sub(const arm::neon2<S> &v) {
	uint32x4_t a0 = v.value0();
	uint32x4_t a1 = v.value1();

	uint32x4_t b0 = vsraq_n_u32(a0, a0, 31);
	uint32x4_t b1 = vsraq_n_u32(a1, a1, 31);
	return arm::neon2<S>(b0, b1);
}

template<size_t S>
inline arm::neon2<S> rot1_add_dec(const arm::neon2<S> &v) {
	uint32x4_t a0 = v.value0();
	uint32x4_t a1 = v.value1();

	uint32x4_t d0 = vcgeq_s32(vreinterpretq_s32_u32(a0), vdupq_n_s32(0));
	uint32x4_t d1 = vcgeq_s32(vreinterpretq_s32_u32(a1), vdupq_n_s32(0));
	return v + v + v + arm::neon2<S>(d0, d1);
}

}

#endif /* __ARM_NEON__ || __ARM_NEON */
