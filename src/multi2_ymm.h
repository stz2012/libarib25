#pragma once

#if defined(__AVX2__)

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

class ymm {
private:
	__m256i v;

public:
	inline ymm() {
#if !defined(NO_MM_UNDEFINED)
		v = _mm256_undefined_si256();
#endif
	}
	inline ymm(uint32_t n) { v = _mm256_set1_epi32(n); }
	inline ymm(const __m256i &r) { v = r; }

	inline ymm &operator=(const ymm &other) {
		v = other.v;
		return *this;
	}

	inline ymm operator+(const ymm &other) const { return _mm256_add_epi32(v, other.v); }
	inline ymm operator-(const ymm &other) const { return _mm256_sub_epi32(v, other.v); }
	inline ymm operator^(const ymm &other) const { return _mm256_xor_si256(v, other.v); }
	inline ymm operator|(const ymm &other) const { return _mm256_or_si256(v, other.v); }
	inline ymm operator<<(int n) const { return _mm256_slli_epi32(v, n); }
	inline ymm operator>>(int n) const { return _mm256_srli_epi32(v, n); }

	inline const __m256i &value() const { return v; }
};

}

template<>
inline void block<x86::ymm>::load(const uint8_t *p) {
	const __m256i *q = reinterpret_cast<const __m256i *>(p);

	__m256i a0 = _mm256_loadu_si256(q);     // 7 6 5 4 3 2 1 0 - DCBA
	__m256i a1 = _mm256_loadu_si256(q + 1); // f e d c b a 9 8 - DCBA

	__m256i s = _mm256_set_epi8(
		12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3,
		12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3
		);
	__m256i b0 = _mm256_shuffle_epi8(a0, s); // 7 5 6 4 3 1 2 0 - ABCD
	__m256i b1 = _mm256_shuffle_epi8(a1, s); // f d e c b 9 a 8 - ABCD

	left  = _mm256_unpacklo_epi32(b0, b1); // e 6 c 4 a 2 8 0 - ABCD
	right = _mm256_unpackhi_epi32(b0, b1); // f 7 d 5 b 3 9 1 - ABCD
}

template<>
inline void block<x86::ymm>::store(uint8_t *p) const {
	__m256i a0 = left.value();  // e 6 c 4 a 2 8 0 - ABCD
	__m256i a1 = right.value(); // f 7 d 5 b 3 9 1 - ABCD

	__m256i s = _mm256_set_epi8(
		12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3,
		12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3
	);
	__m256i b0 = _mm256_shuffle_epi8(a0, s); // e c 6 4 a 8 2 0 - DCBA
	__m256i b1 = _mm256_shuffle_epi8(a1, s); // f d 7 5 b 9 3 1 - DCBA

	__m256i c0 = _mm256_unpacklo_epi32(b0, b1); // 7 6 5 4 3 2 1 0 - DCBA
	__m256i c1 = _mm256_unpackhi_epi32(b0, b1); // f e d c 7 6 5 4 - DCBA

	__m256i *q = reinterpret_cast<__m256i *>(p);
	_mm256_storeu_si256(q,     c0);
	_mm256_storeu_si256(q + 1, c1);
}

template<>
inline std::pair<block<x86::ymm>, cbc_state> block<x86::ymm>::cbc_post_decrypt(const block<x86::ymm> &c, const cbc_state &state) const {
	__m256i c0 = c.left.value();  // 7 3 6 2 5 1 4 0
	__m256i c1 = c.right.value();

	uint32_t s0 = _mm256_extract_epi32(c0, 7); // 7
	uint32_t s1 = _mm256_extract_epi32(c1, 7);

	__m256i s  = _mm256_set_epi32(5, 4, 3, 2, 1, 0, 6, 7);
	__m256i a0 = _mm256_permutevar8x32_epi32(c0, s); // 6 2 5 1 4 0 3 7
	__m256i a1 = _mm256_permutevar8x32_epi32(c1, s);

	__m256i x0 = _mm256_insert_epi32(a0, state.left,  0); // 6 2 5 1 4 0 3 s
	__m256i x1 = _mm256_insert_epi32(a1, state.right, 0);

	__m256i d0 = left.value();  // 7 3 6 2 5 1 4 0
	__m256i d1 = right.value();

	__m256i p0 = _mm256_xor_si256(d0, x0);
	__m256i p1 = _mm256_xor_si256(d1, x1);

	return std::make_pair(block<x86::ymm>(p0, p1), cbc_state(s0, s1));
}


template<>
inline x86::ymm rot<8, x86::ymm>(const x86::ymm &v) {
	__m256i s = _mm256_set_epi8(
		14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3,
		14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3
	);
	return _mm256_shuffle_epi8(v.value(), s);
}

template<>
inline x86::ymm rot<16, x86::ymm>(const x86::ymm &v) {
	__m256i s = _mm256_set_epi8(
		13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2,
		13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2
	);
	return _mm256_shuffle_epi8(v.value(), s);
}

template<>
inline x86::ymm rot1_add_dec<x86::ymm>(const x86::ymm &v) {
	__m256i d = _mm256_cmpgt_epi32(v.value(), _mm256_set1_epi32(-1));
	return v + v + v + x86::ymm(d);
}

}

#endif /* __AVX2__ */
