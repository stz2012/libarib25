#pragma once

#if defined(__SSE2__)

#include <utility>

#if defined(_WIN32)
# include <intrin.h>
#else
# include <x86intrin.h>
#endif

#include "portable.h"

#include "multi2_block.h"

namespace multi2 {

namespace x86 {

class xmm {
private:
	__m128i v;

public:
	inline xmm() {
#if !defined(NO_MM_UNDEFINED)
		v = _mm_undefined_si128();
#endif
	}
	inline xmm(uint32_t n) { v = _mm_set1_epi32(n); }
	inline xmm(const __m128i &r) { v = r; }

	inline xmm &operator=(const xmm &other) {
		v = other.v;
		return *this;
	}

	inline xmm operator+(const xmm &other) const { return _mm_add_epi32(v, other.v); }
	inline xmm operator-(const xmm &other) const { return _mm_sub_epi32(v, other.v); }
	inline xmm operator^(const xmm &other) const { return _mm_xor_si128(v, other.v); }
	inline xmm operator|(const xmm &other) const { return _mm_or_si128(v, other.v); }
	inline xmm operator<<(int n) const { return _mm_slli_epi32(v, n); }
	inline xmm operator>>(int n) const { return _mm_srli_epi32(v, n); }

	inline const __m128i &value() const { return v; }
};

}

#if defined(__SSSE3__)

template<>
inline void block<x86::xmm>::load(const uint8_t *p) {
	const __m128i *q = reinterpret_cast<const __m128i *>(p);

	__m128i a0 = _mm_loadu_si128(q);     // 3 2 1 0 - DCBA
	__m128i a1 = _mm_loadu_si128(q + 1); // 7 6 5 4 - DCBA

	__m128i s = _mm_set_epi8(12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3);
	__m128i b0 = _mm_shuffle_epi8(a0, s); // 3 1 2 0 - ABCD
	__m128i b1 = _mm_shuffle_epi8(a1, s); // 7 5 6 4 - ABCD

	left  = _mm_unpacklo_epi32(b0, b1); // 6 2 4 0 - ABCD
	right = _mm_unpackhi_epi32(b0, b1); // 7 3 5 1 - ABCD
}

template<>
inline void block<x86::xmm>::store(uint8_t *p) const {
	__m128i a0 = left.value();  // 6 2 4 0 - ABCD
	__m128i a1 = right.value(); // 7 3 5 1 - ABCD

	__m128i s = _mm_set_epi8(12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3);
	__m128i b0 = _mm_shuffle_epi8(a0, s); // 6 4 2 0 - DCBA
	__m128i b1 = _mm_shuffle_epi8(a1, s); // 7 5 3 1 - DCBA

	__m128i c0 = _mm_unpacklo_epi32(b0, b1); // 3 2 1 0 - DCBA
	__m128i c1 = _mm_unpackhi_epi32(b0, b1); // 7 6 5 4 - DCBA

	__m128i *q = reinterpret_cast<__m128i *>(p);
	_mm_storeu_si128(q,     c0);
	_mm_storeu_si128(q + 1, c1);
}

template<>
inline x86::xmm rot<8, x86::xmm>(const x86::xmm &v) {
	__m128i s = _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
	return _mm_shuffle_epi8(v.value(), s);
}

template<>
inline x86::xmm rot<16, x86::xmm>(const x86::xmm &v) {
	__m128i s = _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
	return _mm_shuffle_epi8(v.value(), s);
}

#else /* __SSSE3__ */

template<>
inline void block<x86::xmm>::load(const uint8_t *p) {
	const __m128i *q = reinterpret_cast<const __m128i *>(p);

	__m128i a0 = _mm_loadu_si128(q);     // 3 2 1 0
	__m128i a1 = _mm_loadu_si128(q + 1); // 7 6 5 4

	__m128i b0 = _mm_unpacklo_epi32(a0, a1); // 5 1 4 0
	__m128i b1 = _mm_unpackhi_epi32(a0, a1); // 7 3 6 2

	__m128i c0 = _mm_unpacklo_epi32(b0, b1); // 6 4 2 0 - DCBA
	__m128i c1 = _mm_unpackhi_epi32(b0, b1); // 7 5 3 1 - DCBA

	__m128i d0 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(c0, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1)); // BADC
	__m128i d1 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(c1, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1)); // BADC

	left  = _mm_or_si128(_mm_srli_epi16(d0, 8), _mm_slli_epi16(d0, 8)); // ABCD
	right = _mm_or_si128(_mm_srli_epi16(d1, 8), _mm_slli_epi16(d1, 8)); // ABCD
}

template<>
inline void block<x86::xmm>::store(uint8_t *p) const {
	__m128i a0 = left.value();  // ABCD
	__m128i a1 = right.value(); // ABCD

	__m128i b0 = _mm_or_si128(_mm_srli_epi16(a0, 8), _mm_slli_epi16(a0, 8)); // BADC
	__m128i b1 = _mm_or_si128(_mm_srli_epi16(a1, 8), _mm_slli_epi16(a1, 8)); // BADC

	__m128i c0 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(b0, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1)); // 6 4 2 0 - DCBA
	__m128i c1 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(b1, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1)); // 7 5 3 1 - DCBA

	__m128i d0 = _mm_unpacklo_epi32(c0, c1); // 3 2 1 0
	__m128i d1 = _mm_unpackhi_epi32(c0, c1); // 7 6 5 4

	__m128i *q = reinterpret_cast<__m128i *>(p);
	_mm_storeu_si128(q,     d0);
	_mm_storeu_si128(q + 1, d1);
}

#endif /* __SSSE3__ */

#if defined(__SSE4_1__)

template<>
inline std::pair<block<x86::xmm>, cbc_state> block<x86::xmm>::cbc_post_decrypt(const block<x86::xmm> &c, const cbc_state &state) const {
	__m128i c0 = c.left.value(); // 3 1 2 0
	__m128i c1 = c.right.value();

	uint32_t s0 = _mm_extract_epi32(c0, 3); // 3
	uint32_t s1 = _mm_extract_epi32(c1, 3);

	__m128i b0 = _mm_shuffle_epi32(c0, _MM_SHUFFLE(1, 0, 2, 3)); // 2 0 1 3
	__m128i b1 = _mm_shuffle_epi32(c1, _MM_SHUFFLE(1, 0, 2, 3));

	__m128i x0 = _mm_insert_epi32(b0, state.left,  0); // 2 0 1 s
	__m128i x1 = _mm_insert_epi32(b1, state.right, 0);

	__m128i d0 = left.value();  // 3 1 2 0
	__m128i d1 = right.value();

	__m128i p0 = _mm_xor_si128(d0, x0);
	__m128i p1 = _mm_xor_si128(d1, x1);

	return std::make_pair(block<x86::xmm>(p0, p1), cbc_state(s0, s1));
}

#else /* __SSE4_1__ */

template<>
inline std::pair<block<x86::xmm>, cbc_state> block<x86::xmm>::cbc_post_decrypt(const block<x86::xmm> &c, const cbc_state &state) const {
	__m128i c0 = c.left.value(); // 3 1 2 0 / 3 2 1 0
	__m128i c1 = c.right.value();

#if defined(__SSSE3__)
	const int s = _MM_SHUFFLE(1, 0, 2, 3);
#else
	const int s = _MM_SHUFFLE(2, 1, 0, 3);
#endif
	__m128i b0 = _mm_shuffle_epi32(c0, s); // 2 0 1 3 / 2 1 0 3
	__m128i b1 = _mm_shuffle_epi32(c1, s);

	uint32_t s0 = _mm_cvtsi128_si32(b0); // 3
	uint32_t s1 = _mm_cvtsi128_si32(b1);

	__m128i i0 = _mm_cvtsi32_si128(state.left); // - - - s
	__m128i i1 = _mm_cvtsi32_si128(state.right);

	__m128i x0 = _mm_castps_si128(_mm_move_ss(_mm_castsi128_ps(b0), _mm_castsi128_ps(i0))); // 2 0 1 s / 2 1 0 s
	__m128i x1 = _mm_castps_si128(_mm_move_ss(_mm_castsi128_ps(b1), _mm_castsi128_ps(i1)));

	__m128i d0 = left.value();  // 3 1 2 0 / 3 2 1 0
	__m128i d1 = right.value();

	__m128i p0 = _mm_xor_si128(d0, x0);
	__m128i p1 = _mm_xor_si128(d1, x1);

	return std::make_pair(block<x86::xmm>(p0, p1), cbc_state(s0, s1));
}

#endif /* __SSE4_1__ */

template<>
inline x86::xmm rot1_add_dec<x86::xmm>(const x86::xmm &v) {
	__m128i d = _mm_cmpgt_epi32(v.value(), _mm_set1_epi32(-1));
	return v + v + v + x86::xmm(d);
}

}

#endif /* __SSE2__ */
