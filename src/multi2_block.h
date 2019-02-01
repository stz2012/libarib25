#pragma once

#include <utility>

#include "portable.h"

namespace multi2 {

inline uint32_t load_be(const uint8_t *p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

inline void store_be(uint8_t *p, uint32_t v) {
	p[0] = (v >> 24) & 0xff;
	p[1] = (v >> 16) & 0xff;
	p[2] = (v >>  8) & 0xff;
	p[3] =  v        & 0xff;
}

template<typename T>
struct block {
	T left;
	T right;

	inline block() { }
	inline block(const T &l, const T &r) : left(l), right(r) { }

	void load(const uint8_t *p);
	void store(uint8_t *p) const;

	block<T> operator^(const block<T> &other) const;

	std::pair<block<T>, block<uint32_t> > cbc_post_decrypt(const block<T> &ciphertext, const block<uint32_t> &state) const;
};

template<typename T>
inline size_t block_size() {
	return sizeof(T) * 2;
}

typedef block<uint32_t> cbc_state;;

template<>
inline void block<uint32_t>::load(const uint8_t *p) {
	left  = load_be(p);
	right = load_be(p + 4);
}

template<>
inline void block<uint32_t>::store(uint8_t *p) const {
	store_be(p,     left);
	store_be(p + 4, right);
}

template<>
inline block<uint32_t> block<uint32_t>::operator^(const block<uint32_t> &other) const {
	return block<uint32_t>(left ^ other.left, right ^ other.right);
}

template<>
inline std::pair<block<uint32_t>, cbc_state> block<uint32_t>::cbc_post_decrypt(const block<uint32_t> &c, const cbc_state &state) const {
	block<uint32_t> p = *this ^ state;
	return std::make_pair(p, c);
}

template<size_t N, typename T>
inline T rot(const T &v) {
	return (v << N) | (v >> (32 - N));
}

template<typename T>
inline T rot1_sub(const T &v) {
	return v + (v >> 31);
}

template<typename T>
inline T rot1_add_dec(const T &v) {
	return rot<1>(v) + v - T(1);
}

}
