#pragma once

#include <cstring>

#include "portable.h"

#include "multi2_block.h"
#include "multi2_ymm2.h"
#include "multi2_ymm.h"
#include "multi2_xmm.h"
#include "multi2_neon2.h"
#include "multi2_neon.h"

#if defined(__GNUC__) || defined(__clang__)
# define MULTI2_ALWAYS_INLINE __attribute__((always_inline))
# define MULTI2_LIKELY(x)     __builtin_expect(!!(x), 1)
#else
# define MULTI2_ALWAYS_INLINE
# define MULTI2_LIKELY(x)     (x)
#endif

namespace multi2 {

typedef array<uint32_t, 8> system_key_type;
typedef array<uint32_t, 2> iv_type;
typedef array<uint32_t, 2> data_key_type;
typedef array<uint32_t, 8> work_key_type;

template<typename T>
struct pi {
	typedef block<T> block_type;

	static inline block_type pi1(const block_type &p) {
		return block_type(p.left, p.right ^ p.left);
	}

	static inline block_type pi2(const block_type &p, uint32_t k1) {
		T x = p.right;
		T y = x + T(k1);
		T z = rot1_add_dec(y);
		return block_type(p.left ^ rot<4>(z) ^ z, p.right);
	}

	static inline block_type pi3(const block_type &p, uint32_t k2, uint32_t k3) {
		T x = p.left;
		T y = x + T(k2);
		T z = rot<2>(y) + y + T(1);
		T a = rot<8>(z) ^ z;
		T b = a + T(k3);
		T c = rot1_sub(b);
		return block_type(p.left, p.right ^ rot<16>(c) ^ (c | x));
	}

	static inline block_type pi4(const block_type &p, uint32_t k4) {
		T x = p.right;
		T y = x + T(k4);
		return block_type(p.left ^ (rot<2>(y) + y + T(1)), p.right);
	}
};

template<typename T>
struct cipher {
	typedef block<T> block_type;
	typedef pi<T> p;

	static inline block_type encrypt(const block_type &b, const work_key_type &wk, int n) {
		block_type t = b;

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
		block_type t = b;

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
	typedef pi<uint32_t> p;

	block<uint32_t> a0 = p::pi1(block<uint32_t>(dk[0], dk[1]));
	block<uint32_t> a1 = p::pi2(a0, sk[0]);
	block<uint32_t> a2 = p::pi3(a1, sk[1], sk[2]);
	block<uint32_t> a3 = p::pi4(a2, sk[3]);
	block<uint32_t> a4 = p::pi1(a3);
	block<uint32_t> a5 = p::pi2(a4, sk[4]);
	block<uint32_t> a6 = p::pi3(a5, sk[5], sk[6]);
	block<uint32_t> a7 = p::pi4(a6, sk[7]);
	block<uint32_t> a8 = p::pi1(a7);

	work_key_type w;
	w[0] = a1.left;
	w[1] = a2.right;
	w[2] = a3.left;
	w[3] = a4.right;
	w[4] = a5.left;
	w[5] = a6.right;
	w[6] = a7.left;
	w[7] = a8.right;

	return w;
}

inline void encrypt_cbc_ofb(uint8_t *buf, size_t n, const iv_type &iv, const work_key_type &key, int round) {

	cbc_state state(iv[0], iv[1]);

	while (block_size<uint32_t>() <= n) {
		block<uint32_t> p;
		p.load(buf);

		block<uint32_t> c = cipher<uint32_t>::encrypt(p ^ state, key, round);
		c.store(buf);

		state = c;
		buf += block_size<uint32_t>();
		n   -= block_size<uint32_t>();
	}
	if (0 < n) {
		array<uint8_t, 8> t;
		memcpy(&t[0], buf, n);
		memset(&t[n], 0,   8 - n);

		block<uint32_t> p;
		p.load(&t[0]);

		block<uint32_t> c = p ^ cipher<uint32_t>::encrypt(state, key, round);
		c.store(&t[0]);
		memcpy(buf, &t[0], n);
	}
}

template<typename T>
MULTI2_ALWAYS_INLINE
static inline void decrypt_block(uint8_t *&buf, size_t &n, cbc_state &state, const work_key_type &key, int round) {
	block<T> c;
	c.load(buf);

	block<T> d = cipher<T>::decrypt(c, key, round);
	std::pair<block<T>, cbc_state> ps = d.cbc_post_decrypt(c, state);
	ps.first.store(buf);

	state = ps.second;
	buf += block_size<T>();
	n   -= block_size<T>();
}

inline void decrypt_cbc_ofb(uint8_t *buf, size_t n, const iv_type &iv, const work_key_type &key, int round) {

	cbc_state state(iv[0], iv[1]);

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

#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
	if (MULTI2_LIKELY(n == 184)) {
		decrypt_block<arm::neon2<7> >(buf, n, state, key, round);
		decrypt_block<arm::neon2<8> >(buf, n, state, key, round);
		decrypt_block<arm::neon2<8> >(buf, n, state, key, round);
		return;
	}
	while (block_size<arm::neon2<8> >() <= n) {
		decrypt_block<arm::neon2<8> >(buf, n, state, key, round);
	}
	if (block_size<arm::neon>() <= n) {
		decrypt_block<arm::neon>(buf, n, state, key, round);
	}

#endif

	while (block_size<uint32_t>() <= n) {
		decrypt_block<uint32_t>(buf, n, state, key, round);
	}
	if (0 < n) {
		array<uint8_t, 8> t;
		memcpy(&t[0], buf, n);
		memset(&t[n], 0,   8 - n);

		block<uint32_t> c;
		c.load(&t[0]);

		block<uint32_t> p = c ^ cipher<uint32_t>::encrypt(state, key, round);
		p.store(&t[0]);
		memcpy(buf, &t[0], n);
	}
}

}

#undef MULTI2_ALWAYS_INLINE
#undef MULTI2_LIKELY
