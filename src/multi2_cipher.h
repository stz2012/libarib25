#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include "multi2_block.h"
#include "multi2_ymm2.h"
#include "multi2_ymm.h"
#include "multi2_xmm.h"

#if defined(__GNUC__)
# define MULTI2_ALWAYS_INLINE __attribute__((always_inline))
# define MULTI2_LIKELY(x)     __builtin_expect(!!(x), 1)
#else
# define MULTI2_ALWAYS_INLINE
# define MULTI2_LIKELY(x)     (x)
#endif

namespace multi2 {

using system_key_type = std::array<uint32_t, 8>;
using iv_type = std::array<uint32_t, 2>;
using data_key_type = std::array<uint32_t, 2>;
using work_key_type = std::array<uint32_t, 8>;

template<typename T>
struct pi {
	using block_type = block<T>;

	static inline block_type pi1(const block_type &p) {
		return { p.left, p.right ^ p.left };
	}

	static inline block_type pi2(const block_type &p, uint32_t k1) {
		auto x = p.right;
		auto y = x + T{ k1 };
		auto z = rot1_add_dec(y);
		return { p.left ^ rot<4>(z) ^ z, p.right };
	}

	static inline block_type pi3(const block_type &p, uint32_t k2, uint32_t k3) {
		auto x = p.left;
		auto y = x + T{ k2 };
		auto z = rot<2>(y) + y + T{ 1 };
		auto a = rot<8>(z) ^ z;
		auto b = a + T{ k3 };
		auto c = rot1_sub(b);
		return { p.left, p.right ^ rot<16>(c) ^ (c | x) };
	}

	static inline block_type pi4(const block_type &p, uint32_t k4) {
		auto x = p.right;
		auto y = x + T{ k4 };
		return { p.left ^ (rot<2>(y) + y + T{ 1 }), p.right };
	}
};

template<typename T>
struct cipher {
	using block_type = block<T>;
	using p = pi<T>;

	static inline block_type encrypt(const block_type &b, const work_key_type &wk, int n) {
		auto t = b;

		for (int i = 0; i < n; ++i) {
			t = p::pi1(t);
			t = p::pi2(t, wk[0]);
			t = p::pi3(t, wk[1], wk[2]);
			t = p::pi4(t, wk[3]);
			t = p::pi1(t);
			t = p::pi2(t, wk[4]);
			t = p::pi3(t, wk[5], wk[6]);
			t = p::pi4(t, wk[7]);
		}
		return t;
	}

	static inline block_type decrypt(const block_type &b, const work_key_type &wk, int n) {
		auto t = b;

		for (int i = 0; i < n; ++i) {
			t = p::pi4(t, wk[7]);
			t = p::pi3(t, wk[5], wk[6]);
			t = p::pi2(t, wk[4]);
			t = p::pi1(t);
			t = p::pi4(t, wk[3]);
			t = p::pi3(t, wk[1], wk[2]);
			t = p::pi2(t, wk[0]);
			t = p::pi1(t);
		}
		return t;
	}
};


inline work_key_type schedule(const data_key_type &dk, const system_key_type &sk) {
	using p = pi<uint32_t>;

	auto a0 = p::pi1({ dk[0], dk[1] });
	auto a1 = p::pi2(a0, sk[0]);
	auto a2 = p::pi3(a1, sk[1], sk[2]);
	auto a3 = p::pi4(a2, sk[3]);
	auto a4 = p::pi1(a3);
	auto a5 = p::pi2(a4, sk[4]);
	auto a6 = p::pi3(a5, sk[5], sk[6]);
	auto a7 = p::pi4(a6, sk[7]);
	auto a8 = p::pi1(a7);

	auto w1 = a1.left;
	auto w2 = a2.right;
	auto w3 = a3.left;
	auto w4 = a4.right;
	auto w5 = a5.left;
	auto w6 = a6.right;
	auto w7 = a7.left;
	auto w8 = a8.right;

	return { w1, w2, w3, w4, w5, w6, w7, w8 };
}

inline void encrypt_cbc_ofb(uint8_t *buf, size_t n, const iv_type &iv, const work_key_type &key, int round) {

	cbc_state state{ iv[0], iv[1] };

	while (block_size<uint32_t>() <= n) {
		block<uint32_t> p;
		p.load(buf);

		auto c = cipher<uint32_t>::encrypt(p ^ state, key, round);
		c.store(buf);

		state = c;
		buf += block_size<uint32_t>();
		n   -= block_size<uint32_t>();
	}
	if (0 < n) {
		std::array<uint8_t, 8> t;
		memcpy(&t[0], buf, n);
		memset(&t[n], 0,   8 - n);

		block<uint32_t> p;
		p.load(&t[0]);

		auto c = p ^ cipher<uint32_t>::encrypt(state, key, round);
		c.store(&t[0]);
		memcpy(buf, &t[0], n);
	}
}

template<typename T>
MULTI2_ALWAYS_INLINE
static inline void decrypt_block(uint8_t *&buf, size_t &n, cbc_state &state, const work_key_type &key, int round) {
	block<T> c;
	c.load(buf);

	auto d = cipher<T>::decrypt(c, key, round);
	auto ps = d.cbc_post_decrypt(c, state);
	ps.first.store(buf);

	state = ps.second;
	buf += block_size<T>();
	n   -= block_size<T>();
}

inline void decrypt_cbc_ofb(uint8_t *buf, size_t n, const iv_type &iv, const work_key_type &key, int round) {

	cbc_state state{ iv[0], iv[1] };

#if defined(__AVX2__)
	if (MULTI2_LIKELY(n == 184)) {
		decrypt_block<x86::ymm2>(buf, n, state, key, round);
		decrypt_block<x86::ymm>(buf, n, state, key, round);
		return;
	}
	if (block_size<x86::ymm2>() <= n) {
		decrypt_block<x86::ymm2>(buf, n, state, key, round);
	}
	if (block_size<x86::ymm>() <= n) {
		decrypt_block<x86::ymm>(buf, n, state, key, round);
	}
#if defined(__SSE2__)
	if (block_size<x86::xmm>() <= n) {
		decrypt_block<x86::xmm>(buf, n, state, key, round);
	}
#endif

#elif defined(__SSE2__)
	while (block_size<x86::xmm>() <= n) {
		decrypt_block<x86::xmm>(buf, n, state, key, round);
	}

#endif

	while (block_size<uint32_t>() <= n) {
		decrypt_block<uint32_t>(buf, n, state, key, round);
	}
	if (0 < n) {
		std::array<uint8_t, 8> t;
		memcpy(&t[0], buf, n);
		memset(&t[n], 0,   8 - n);

		block<uint32_t> c;
		c.load(&t[0]);

		auto p = c ^ cipher<uint32_t>::encrypt(state, key, round);
		p.store(&t[0]);
		memcpy(buf, &t[0], n);
	}
}

}

#undef MULTI2_ALWAYS_INLINE
#undef MULTI2_LIKELY
