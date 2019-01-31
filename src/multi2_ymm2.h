#pragma once

#if defined(__AVX2__)

#include <cstdint>
#include <utility>

#if defined(_WIN32)
# include <intrin.h>
#else
# include <x86intrin.h>
#endif

#include "multi2_block.h"

namespace multi2 {

namespace x86 {

class ymm2 {
private:
	__m256i v0;
	__m256i v1;

public:
	inline ymm2() { v0 = v1 = _mm256_undefined_si256(); }
	inline ymm2(uint32_t n) { v0 = v1 = _mm256_set1_epi32(n); }
	inline ymm2(const __m256i &r0, const __m256i &r1) {
		v0 = r0;
		v1 = r1;
	}

	inline ymm2 &operator=(const ymm2 &other) {
		v0 = other.v0;
		v1 = other.v1;
		return *this;
	}

	inline ymm2 operator+(const ymm2 &other) const {
		return { _mm256_add_epi32(v0, other.v0), _mm256_add_epi32(v1, other.v1) };
	}
	inline ymm2 operator-(const ymm2 &other) const {
		return { _mm256_sub_epi32(v0, other.v0), _mm256_sub_epi32(v1, other.v1) };
	}
	inline ymm2 operator^(const ymm2 &other) const {
		return { _mm256_xor_si256(v0, other.v0), _mm256_xor_si256(v1, other.v1) };
	}
	inline ymm2 operator|(const ymm2 &other) const {
		return { _mm256_or_si256(v0, other.v0), _mm256_or_si256(v1, other.v1) };
	}
	inline ymm2 operator<<(int n) const {
		return { _mm256_slli_epi32(v0, n), _mm256_slli_epi32(v1, n) };
	}
	inline ymm2 operator>>(int n) const {
		return { _mm256_srli_epi32(v0, n), _mm256_srli_epi32(v1, n) };
	}

	inline const __m256i &value0() const { return v0; }
	inline const __m256i &value1() const { return v1; }
};

}

template<>
constexpr size_t block_size<x86::ymm2>() {
	return 120;
}

template<>
inline void block<x86::ymm2>::load(const uint8_t *p) {
	auto *q0 = reinterpret_cast<const __m256i *>(p);
	auto *q1 = reinterpret_cast<const __m256i *>(p + 56);

	auto a0 = _mm256_loadu_si256(q0);     // 07 06 05 04 03 02 01 00 - DCBA
	auto a1 = _mm256_loadu_si256(q0 + 1); // 0f 0e 0d 0c 0b 0a 09 08 - DCBA
	auto a2 = _mm256_loadu_si256(q1);     // 15 14 13 12 11 10 0f 0e - DCBA
	auto a3 = _mm256_loadu_si256(q1 + 1); // 1d 1c 1b 1a 19 18 17 16 - DCBA

	auto s = _mm256_set_epi8(12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3,
				 12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3);
	auto b0 = _mm256_shuffle_epi8(a0, s); // 07 05 06 04 03 01 02 00 - ABCD
	auto b1 = _mm256_shuffle_epi8(a1, s); // 0f 0d 0e 0c 0b 09 0a 08 - ABCD
	auto b2 = _mm256_shuffle_epi8(a2, s); // 15 13 14 12 11 0f 10 0e - ABCD
	auto b3 = _mm256_shuffle_epi8(a3, s); // 1d 1b 1c 1a 19 17 18 16 - ABCD

	auto c0 = _mm256_unpacklo_epi32(b0, b1); // 0e 06 0c 04 0a 02 08 00 - ABCD
	auto c1 = _mm256_unpackhi_epi32(b0, b1); // 0f 07 0d 05 0b 03 09 01 - ABCD
	auto c2 = _mm256_unpacklo_epi32(b2, b3); // 1c 14 1a 12 18 10 16 0e - ABCD
	auto c3 = _mm256_unpackhi_epi32(b2, b3); // 1d 15 1b 13 19 11 17 0f - ABCD

	left  = { c0, c2 }; // 0e 06 0c 04 0a 02 08 00 - 1c 14 1a 12 18 10 16 0e
	right = { c1, c3 }; // 0f 07 0d 05 0b 03 09 01 - 1d 15 1b 13 19 11 17 0f
}

template<>
inline void block<x86::ymm2>::store(uint8_t *p) const {
	auto a0 = left.value0();  // 0e 06 0c 04 0a 02 08 00 - ABCD
	auto a1 = right.value0(); // 0f 07 0d 05 0b 03 09 01 - ABCD
	auto a2 = left.value1();  // 1c 14 1a 12 18 10 16 -- - ABCD
	auto a3 = right.value1(); // 1d 15 1b 13 19 11 17 -- - ABCD

	auto s = _mm256_set_epi8(12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3,
				 12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3);
	auto b0 = _mm256_shuffle_epi8(a0, s); // 0e 0c 06 04 0a 08 02 00 - DCBA
	auto b1 = _mm256_shuffle_epi8(a1, s); // 0f 0d 07 05 0b 09 03 01 - DCBA
	auto b2 = _mm256_shuffle_epi8(a2, s); // 1c 1a 14 12 18 16 10 -- - DCBA
	auto b3 = _mm256_shuffle_epi8(a3, s); // 1d 1b 15 13 19 17 11 -- - DCBA

	auto c0 = _mm256_unpacklo_epi32(b0, b1); // 07 06 05 04 03 02 01 00 - DCBA
	auto c1 = _mm256_unpackhi_epi32(b0, b1); // 0f 0e 0d 0c 07 06 05 04 - DCBA
	auto c2 = _mm256_unpacklo_epi32(b2, b3); // 15 14 13 12 11 10 -- -- - DCBA
	auto c3 = _mm256_unpackhi_epi32(b2, b3); // 1d 1c 1b 1a 19 18 17 16 - DCBA

	auto *q0 = reinterpret_cast<__m256i *>(p);
	auto *q1 = reinterpret_cast<__m256i *>(p + 56);
	_mm256_storeu_si256(q0,     c0);
	_mm256_storeu_si256(q1,     c2);
	_mm256_storeu_si256(q0 + 1, c1);
	_mm256_storeu_si256(q1 + 1, c3);
}

template<>
inline std::pair<block<x86::ymm2>, cbc_state> block<x86::ymm2>::cbc_post_decrypt(const block<x86::ymm2> &c, const cbc_state &state) const {
	auto c0 = c.left.value0();  // 7 3 6 2 5 1 4 0
	auto c1 = c.right.value0();
	auto c2 = c.left.value1();  // e a d 9 c 8 b 7
	auto c3 = c.right.value1();

	uint32_t s2 = _mm256_extract_epi32(c2, 7); // e
	uint32_t s3 = _mm256_extract_epi32(c3, 7);

	auto s  = _mm256_set_epi32(5, 4, 3, 2, 1, 0, 6, 7);
	auto a0 = _mm256_permutevar8x32_epi32(c0, s); // 6 2 5 1 4 0 3 7
	auto a1 = _mm256_permutevar8x32_epi32(c1, s);
	auto a2 = _mm256_permutevar8x32_epi32(c2, s); // d 9 c 8 b 7 a e
	auto a3 = _mm256_permutevar8x32_epi32(c3, s);

	auto x0 = _mm256_insert_epi32(a0, state.left,  0); // 6 2 5 1 4 0 3 s
	auto x1 = _mm256_insert_epi32(a1, state.right, 0);
	auto x2 = a2;                                      // d 9 c 8 b 7 a e
	auto x3 = a3;

	auto d0 = left.value0();  // 7 3 6 2 5 1 4 0
	auto d1 = right.value0();
	auto d2 = left.value1();  // e a d 9 c 8 b 7
	auto d3 = right.value1();

	auto p0 = _mm256_xor_si256(d0, x0);
	auto p1 = _mm256_xor_si256(d1, x1);
	auto p2 = _mm256_xor_si256(d2, x2);
	auto p3 = _mm256_xor_si256(d3, x3);

	return std::make_pair(block<x86::ymm2>{ { p0, p2 }, { p1, p3 } }, cbc_state{ s2, s3 });
}

template<>
inline x86::ymm2 rot<8, x86::ymm2>(const x86::ymm2 &v) {
	auto s = _mm256_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3,
				 14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
	return { _mm256_shuffle_epi8(v.value0(), s), _mm256_shuffle_epi8(v.value1(), s) };
}

template<>
inline x86::ymm2 rot<16, x86::ymm2>(const x86::ymm2 &v) {
	auto s = _mm256_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2,
				 13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
	return { _mm256_shuffle_epi8(v.value0(), s), _mm256_shuffle_epi8(v.value1(), s) };
}

template<>
inline x86::ymm2 rot1_add_dec<x86::ymm2>(const x86::ymm2 &v) {
	auto d0 = _mm256_cmpgt_epi32(v.value0(), _mm256_set1_epi32(-1));
	auto d1 = _mm256_cmpgt_epi32(v.value1(), _mm256_set1_epi32(-1));
	return v + v + v + x86::ymm2{ d0, d1 };
}

}

#endif /* __AVX2__ */
