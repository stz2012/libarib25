#pragma once

#include <algorithm>

namespace multi2 {

template<typename T, size_t N>
class array {
	T v[N];

	inline const T *begin() const { return &v[0]; }
	inline const T *end() const { return &v[N]; }

public:
	inline size_t size() { return N; }

	inline bool operator!=(const array &other) const {
		return !std::equal(begin(), end(), other.begin());
	}
	inline T &operator[](size_t n) { return v[n]; }
	inline const T &operator[](size_t n) const { return v[n]; }
};

template<typename T>
class optional {
	T v;
	bool has;

public:
	inline optional() : has(false) { }

	inline optional &operator=(const T &other) {
		v = other;
		has = true;
		return *this;
	}
	inline void reset() { has = false; }
	inline operator bool() const { return has; }
	inline T &operator*() { return v; }
};

}
