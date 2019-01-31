#include <array>
#include <cstdint>
#include <cstddef>
#include <new>
#include <optional>

#include "multi2.h"
#include "multi2_error_code.h"

#include "multi2_cipher.h"

namespace multi2 {

struct multi2 : public MULTI2 {
	uint32_t ref_count;
	uint32_t round;

	std::optional<system_key_type> system_key;
	std::optional<iv_type> iv;

	std::array<std::optional<data_key_type>, 2> data_key;
	std::array<std::optional<work_key_type>, 2> work_key;

	inline void set_system_key(uint8_t *p) {
		system_key.emplace();

		for (size_t i = 0; i < system_key->size(); ++i) {
			(*system_key)[i] = load_be(p + i * 4);
		}
	}

	inline void set_iv(uint8_t *p) {
		iv.emplace();

		(*iv)[0] = load_be(p);
		(*iv)[1] = load_be(p + 4);
	}

	inline void set_work_keys(uint8_t *p) {
		std::array<data_key_type, 2> k{{
			{ load_be(p),     load_be(p +  4) },
			{ load_be(p + 8), load_be(p + 12) }
		}};

		for (int i = 0; i < 2; ++i) {
			if (!data_key[i] || *data_key[i] != k[i]) {
				data_key[i] = k[i];
				work_key[i].reset();
			}
		}
	}

	inline void clear_work_keys() {
		for (int i = 0; i < 2; ++i) {
			data_key[i].reset();
			work_key[i].reset();
		}
	}

	inline int encrypt(int32_t type, uint8_t *b, size_t n) {
		int i{ type == 0x02 };

		if (!iv) {
			return MULTI2_ERROR_UNSET_CBC_INIT;
		}

		if (!work_key[i]) {
			if (!system_key) {
				return MULTI2_ERROR_UNSET_SYSTEM_KEY;
			}
			if (!data_key[i]) {
				return MULTI2_ERROR_UNSET_SCRAMBLE_KEY;
			}
			work_key[i] = schedule(*data_key[i], *system_key);
		}

		encrypt_cbc_ofb(b, n, *iv, *work_key[i], round);
		return 0;
	}

	inline int decrypt(int32_t type, uint8_t *b, size_t n) {
		int i{ type == 0x02 };

		if (!iv) {
			return MULTI2_ERROR_UNSET_CBC_INIT;
		}

		if (!work_key[i]) {
			if (!system_key) {
				return MULTI2_ERROR_UNSET_SYSTEM_KEY;
			}
			if (!data_key[i]) {
				return MULTI2_ERROR_UNSET_SCRAMBLE_KEY;
			}
			work_key[i] = schedule(*data_key[i], *system_key);
		}

		decrypt_cbc_ofb(b, n, *iv, *work_key[i], round);
		return 0;
	}
};

}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (interface method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_multi2(void *m2);
static int add_ref_multi2(void *m2);
static int set_round_multi2(void *m2, int32_t val);
static int set_system_key_multi2(void *m2, uint8_t *val);
static int set_init_cbc_multi2(void *m2, uint8_t *val);
static int set_scramble_key_multi2(void *m2, uint8_t *val);
static int clear_scramble_key_multi2(void *m2);
static int encrypt_multi2(void *m2, int32_t type, uint8_t *buf, int32_t size);
static int decrypt_multi2(void *m2, int32_t type, uint8_t *buf, int32_t size);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 global function implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
ARIB25_API_EXPORT MULTI2 *create_multi2()
{
	multi2::multi2 *m2;
	try {
		m2 = new multi2::multi2();
	} catch (std::bad_alloc &e) {
		return nullptr;
	}

	m2->ref_count = 1;
	m2->round     = 4;

	auto r = static_cast<MULTI2 *>(m2);
	r->private_data = m2;

	r->release            = release_multi2;
	r->add_ref            = add_ref_multi2;
	r->set_round          = set_round_multi2;
	r->set_system_key     = set_system_key_multi2;
	r->set_init_cbc       = set_init_cbc_multi2;
	r->set_scramble_key   = set_scramble_key_multi2;
	r->clear_scramble_key = clear_scramble_key_multi2;
	r->encrypt            = encrypt_multi2;
	r->decrypt            = decrypt_multi2;

	return r;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static multi2::multi2 *private_data(void *m2);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 interface method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_multi2(void *m2)
{
	auto prv = private_data(m2);
	if (!prv) {
		return;
	}

	--prv->ref_count;
	if (!prv->ref_count) {
		delete prv;
	}
}

static int add_ref_multi2(void *m2)
{
	auto prv = private_data(m2);
	if (!prv) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	++prv->ref_count;
	return 0;
}

static int set_round_multi2(void *m2, int32_t val)
{
	auto prv = private_data(m2);
	if (!prv) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->round = val;
	return 0;
}

static int set_system_key_multi2(void *m2, uint8_t *val)
{
	auto prv = private_data(m2);
	if (!prv || !val) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->set_system_key(val);
	return 0;
}

static int set_init_cbc_multi2(void *m2, uint8_t *val)
{
	auto prv = private_data(m2);
	if (!prv || !val) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->set_iv(val);
	return 0;
}

static int set_scramble_key_multi2(void *m2, uint8_t *val)
{
	auto prv = private_data(m2);
	if (!prv || !val) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->set_work_keys(val);
	return 0;
}

static int clear_scramble_key_multi2(void *m2)
{
	auto prv = private_data(m2);
	if (!prv) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->clear_work_keys();
	return 0;
}

static int encrypt_multi2(void *m2, int32_t type, uint8_t *buf, int32_t size)
{
	auto prv = private_data(m2);
	if (!prv || !buf || size < 1) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	return prv->encrypt(type, buf, size);
}

static int decrypt_multi2(void *m2, int32_t type, uint8_t *buf, int32_t size)
{
	auto prv = private_data(m2);
	if (!prv || !buf || size < 1) {
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	return prv->decrypt(type, buf, size);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static multi2::multi2 *private_data(void *m2)
{
	if (!m2) {
		return nullptr;
	}
	auto p = static_cast<MULTI2 *>(m2);
	auto r = static_cast<multi2::multi2 *>(p->private_data);
	if (static_cast<MULTI2 *>(r) != p) {
		return nullptr;
	}

	return r;
}
