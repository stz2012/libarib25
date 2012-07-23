#include <stdlib.h>
#include <string.h>

#include "multi2.h"
#include "multi2_error_code.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 inline functions
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static __inline uint8_t *load_be_uint32(uint32_t *dst, uint8_t *src)
{
	*dst = ((src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3]);
	return src+4;
}

static __inline uint8_t *save_be_uint32(uint8_t *dst, uint32_t src)
{
	dst[0] = (uint8_t)((src>>24) & 0xff);
	dst[1] = (uint8_t)((src>>16) & 0xff);
	dst[2] = (uint8_t)((src>> 8) & 0xff);
	dst[3] = (uint8_t)( src      & 0xff);
	return dst+4;
}

static __inline uint32_t left_rotate_uint32(uint32_t val, uint32_t count)
{
	return ((val << count) | (val >> (32-count)));
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 inner structures
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
typedef struct {
	uint32_t key[8];
} CORE_PARAM;

typedef struct {
	uint32_t l;
	uint32_t r;
} CORE_DATA;

typedef struct {

	int32_t    ref_count;

	CORE_DATA  cbc_init;
	
	CORE_PARAM sys;
	CORE_DATA  scr[2]; /* 0: odd, 1: even */
	CORE_PARAM wrk[2]; /* 0: odd, 1: even */

	uint32_t   round;
	uint32_t   state;
	
} MULTI2_PRIVATE_DATA;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 constant values
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MULTI2_STATE_CBC_INIT_SET     (0x0001)
#define MULTI2_STATE_SYSTEM_KEY_SET   (0x0002)
#define MULTI2_STATE_SCRAMBLE_KEY_SET (0x0004)

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
MULTI2 *create_multi2()
{
	int n;
	
	MULTI2 *r;
	MULTI2_PRIVATE_DATA *prv;

	n  = sizeof(MULTI2_PRIVATE_DATA);
	n += sizeof(MULTI2);
	
	prv = (MULTI2_PRIVATE_DATA *)calloc(1, n);
	if(prv == NULL){
		return NULL;
	}

	r = (MULTI2 *)(prv+1);
	r->private_data = prv;

	prv->ref_count = 1;
	prv->round = 4;

	r->release = release_multi2;
	r->add_ref = add_ref_multi2;
	r->set_round = set_round_multi2;
	r->set_system_key = set_system_key_multi2;
	r->set_init_cbc = set_init_cbc_multi2;
	r->set_scramble_key = set_scramble_key_multi2;
	r->clear_scramble_key = clear_scramble_key_multi2;
	r->encrypt = encrypt_multi2;
	r->decrypt = decrypt_multi2;

	return r;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static MULTI2_PRIVATE_DATA *private_data(void *m2);

static void core_schedule(CORE_PARAM *work, CORE_PARAM *skey, CORE_DATA *dkey);

static void core_encrypt(CORE_DATA *dst, CORE_DATA *src, CORE_PARAM *w, int32_t round);
static void core_decrypt(CORE_DATA *dst, CORE_DATA *src, CORE_PARAM *w, int32_t round);

static void core_pi1(CORE_DATA *dst, CORE_DATA *src);
static void core_pi2(CORE_DATA *dst, CORE_DATA *src, uint32_t a);
static void core_pi3(CORE_DATA *dst, CORE_DATA *src, uint32_t a, uint32_t b);
static void core_pi4(CORE_DATA *dst, CORE_DATA *src, uint32_t a);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 interface method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_multi2(void *m2)
{
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if(prv == NULL){
		/* do nothing */
		return;
	}

	prv->ref_count -= 1;
	if(prv->ref_count == 0){
		free(prv);
	}
}

static int add_ref_multi2(void *m2)
{
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if(prv == NULL){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->ref_count += 1;

	return 0;
}

static int set_round_multi2(void *m2, int32_t val)
{
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if(prv == NULL){
		/* do nothing */
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	prv->round = val;

	return 0;
}

static int set_system_key_multi2(void *m2, uint8_t *val)
{
	int i;
	uint8_t *p;
	
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if( (prv == NULL) || (val == NULL) ){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	p = val;
	for(i=0;i<8;i++){
		p = load_be_uint32(prv->sys.key+i, p);
	}

	prv->state |= MULTI2_STATE_SYSTEM_KEY_SET;

	return 0;
}

static int set_init_cbc_multi2(void *m2, uint8_t *val)
{
	uint8_t *p;
	
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if( (prv == NULL) || (val == NULL) ){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	p = val;

	p = load_be_uint32(&(prv->cbc_init.l), p);
	p = load_be_uint32(&(prv->cbc_init.r), p);

	prv->state |= MULTI2_STATE_CBC_INIT_SET;

	return 0;
}

static int set_scramble_key_multi2(void *m2, uint8_t *val)
{
	uint8_t *p;
	
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if( (prv == NULL) || (val == NULL) ){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	p = val;

	p = load_be_uint32(&(prv->scr[0].l), p);
	p = load_be_uint32(&(prv->scr[0].r), p);
	p = load_be_uint32(&(prv->scr[1].l), p);
	p = load_be_uint32(&(prv->scr[1].r), p);
	
	core_schedule(prv->wrk+0, &(prv->sys), prv->scr+0);
	core_schedule(prv->wrk+1, &(prv->sys), prv->scr+1);

	prv->state |= MULTI2_STATE_SCRAMBLE_KEY_SET;

	return 0;
}

static int clear_scramble_key_multi2(void *m2)
{
	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if(prv == NULL){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	memset(prv->scr, 0, sizeof(prv->scr));
	memset(prv->wrk, 0, sizeof(prv->wrk));

	prv->state &= (~MULTI2_STATE_SCRAMBLE_KEY_SET);

	return 0;
}

static int encrypt_multi2(void *m2, int32_t type, uint8_t *buf, int32_t size)
{
	CORE_DATA src,dst;
	CORE_PARAM *prm;

	uint8_t *p;

	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if( (prv == NULL) || (buf == NULL) || (size < 1) ){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	if(prv->state != (MULTI2_STATE_CBC_INIT_SET|MULTI2_STATE_SYSTEM_KEY_SET|MULTI2_STATE_SCRAMBLE_KEY_SET)){
		if( (prv->state & MULTI2_STATE_CBC_INIT_SET) == 0 ){
			return MULTI2_ERROR_UNSET_CBC_INIT;
		}
		if( (prv->state & MULTI2_STATE_SYSTEM_KEY_SET) == 0 ){
			return MULTI2_ERROR_UNSET_SYSTEM_KEY;
		}
		if( (prv->state & MULTI2_STATE_SCRAMBLE_KEY_SET) == 0 ){
			return MULTI2_ERROR_UNSET_SCRAMBLE_KEY;
		}
	}
	
	if(type == 0x02){
		prm = prv->wrk+1;
	}else{
		prm = prv->wrk+0;
	}

	dst.l = prv->cbc_init.l;
	dst.r = prv->cbc_init.r;

	p = buf;
	while(size >= 8){
		load_be_uint32(&(src.l), p+0);
		load_be_uint32(&(src.r), p+4);
		src.l = src.l ^ dst.l;
		src.r = src.r ^ dst.r;
		core_encrypt(&dst, &src, prm, prv->round);
		p = save_be_uint32(p, dst.l);
		p = save_be_uint32(p, dst.r);
		size -= 8;
	}

	if(size > 0){
		int i;
		uint8_t tmp[8];
		
		src.l = dst.l;
		src.r = dst.r;
		core_encrypt(&dst, &src, prm, prv->round);
		save_be_uint32(tmp+0, dst.l);
		save_be_uint32(tmp+4, dst.r);

		for(i=0;i<size;i++){
			p[i] = (uint8_t)(p[i] ^ tmp[i]);
		}
	}

	return 0;
}

static int decrypt_multi2(void *m2, int32_t type, uint8_t *buf, int32_t size)
{
	CORE_DATA src,dst,cbc;
	CORE_PARAM *prm;

	uint8_t *p;

	MULTI2_PRIVATE_DATA *prv;

	prv = private_data(m2);
	if( (prv == NULL) || (buf == NULL) || (size < 1) ){
		return MULTI2_ERROR_INVALID_PARAMETER;
	}

	if(prv->state != (MULTI2_STATE_CBC_INIT_SET|MULTI2_STATE_SYSTEM_KEY_SET|MULTI2_STATE_SCRAMBLE_KEY_SET)){
		if( (prv->state & MULTI2_STATE_CBC_INIT_SET) == 0 ){
			return MULTI2_ERROR_UNSET_CBC_INIT;
		}
		if( (prv->state & MULTI2_STATE_SYSTEM_KEY_SET) == 0 ){
			return MULTI2_ERROR_UNSET_SYSTEM_KEY;
		}
		if( (prv->state & MULTI2_STATE_SCRAMBLE_KEY_SET) == 0 ){
			return MULTI2_ERROR_UNSET_SCRAMBLE_KEY;
		}
	}
	
	if(type == 0x02){
		prm = prv->wrk+1;
	}else{
		prm = prv->wrk+0;
	}

	cbc.l = prv->cbc_init.l;
	cbc.r = prv->cbc_init.r;

	p = buf;
	while(size >= 8){
		load_be_uint32(&(src.l), p+0);
		load_be_uint32(&(src.r), p+4);
		core_decrypt(&dst, &src, prm, prv->round);
		dst.l = dst.l ^ cbc.l;
		dst.r = dst.r ^ cbc.r;
		cbc.l = src.l;
		cbc.r = src.r;
		p = save_be_uint32(p, dst.l);
		p = save_be_uint32(p, dst.r);
		size -= 8;
	}

	if(size > 0){
		int i;
		uint8_t tmp[8];
		
		core_encrypt(&dst, &cbc, prm, prv->round);
		save_be_uint32(tmp+0, dst.l);
		save_be_uint32(tmp+4, dst.r);

		for(i=0;i<size;i++){
			p[i] = (uint8_t)(p[i] ^ tmp[i]);
		}
	}

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static MULTI2_PRIVATE_DATA *private_data(void *m2)
{
	MULTI2_PRIVATE_DATA *r;
	MULTI2 *p;

	p = (MULTI2 *)m2;
	if(p == NULL){
		return NULL;
	}

	r = (MULTI2_PRIVATE_DATA *)(p->private_data);
	if( ((void *)(r+1)) != ((void *)p) ){
		return NULL;
	}

	return r;
}

static void core_schedule(CORE_PARAM *work, CORE_PARAM *skey, CORE_DATA *dkey)
{
	CORE_DATA b1,b2,b3,b4,b5,b6,b7,b8,b9;

	core_pi1(&b1, dkey);
	
	core_pi2(&b2, &b1, skey->key[0]);
	work->key[0] = b2.l;
	
	core_pi3(&b3, &b2, skey->key[1], skey->key[2]);
	work->key[1] = b3.r;
	
	core_pi4(&b4, &b3, skey->key[3]);
	work->key[2] = b4.l;

	core_pi1(&b5, &b4);
	work->key[3] = b5.r;

	core_pi2(&b6, &b5, skey->key[4]);
	work->key[4] = b6.l;

	core_pi3(&b7, &b6, skey->key[5], skey->key[6]);
	work->key[5] = b7.r;

	core_pi4(&b8, &b7, skey->key[7]);
	work->key[6] = b8.l;

	core_pi1(&b9, &b8);
	work->key[7] = b9.r;
}

static void core_encrypt(CORE_DATA *dst, CORE_DATA *src, CORE_PARAM *w, int32_t round)
{
	int32_t i;
	
	CORE_DATA tmp;
	
	dst->l = src->l;
	dst->r = src->r;
	for(i=0;i<round;i++){
		core_pi1(&tmp,  dst);
		core_pi2( dst, &tmp, w->key[0]);
		core_pi3(&tmp,  dst, w->key[1], w->key[2]);
		core_pi4( dst, &tmp, w->key[3]);
		core_pi1(&tmp,  dst);
		core_pi2( dst, &tmp, w->key[4]);
		core_pi3(&tmp,  dst, w->key[5], w->key[6]);
		core_pi4( dst, &tmp, w->key[7]);
	}
}

static void core_decrypt(CORE_DATA *dst, CORE_DATA *src, CORE_PARAM *w, int32_t round)
{
	int32_t i;
	
	CORE_DATA tmp;

	dst->l = src->l;
	dst->r = src->r;
	for(i=0;i<round;i++){
		core_pi4(&tmp,  dst, w->key[7]);
		core_pi3( dst, &tmp, w->key[5], w->key[6]);
		core_pi2(&tmp,  dst, w->key[4]);
		core_pi1( dst, &tmp);
		core_pi4(&tmp,  dst, w->key[3]);
		core_pi3( dst, &tmp, w->key[1], w->key[2]);
		core_pi2(&tmp,  dst, w->key[0]);
		core_pi1( dst, &tmp);
	}
}

static void core_pi1(CORE_DATA *dst, CORE_DATA *src)
{
	dst->l = src->l;
	dst->r = src->r ^ src->l;
}

static void core_pi2(CORE_DATA *dst, CORE_DATA *src, uint32_t a)
{
	uint32_t t0,t1,t2;

	t0 = src->r + a;
	t1 = left_rotate_uint32(t0, 1) + t0 - 1;
	t2 = left_rotate_uint32(t1, 4) ^ t1;

	dst->l = src->l ^ t2;
	dst->r = src->r;
}

static void core_pi3(CORE_DATA *dst, CORE_DATA *src, uint32_t a, uint32_t b)
{
	uint32_t t0,t1,t2,t3,t4,t5;

	t0 = src->l + a;
	t1 = left_rotate_uint32(t0, 2) + t0 + 1;
	t2 = left_rotate_uint32(t1, 8) ^ t1;
	t3 = t2 + b;
	t4 = left_rotate_uint32(t3, 1) - t3;
	t5 = left_rotate_uint32(t4, 16) ^ (t4 | src->l);

	dst->l = src->l;
	dst->r = src->r ^ t5;
}

static void core_pi4(CORE_DATA *dst, CORE_DATA *src, uint32_t a)
{
	uint32_t t0,t1;

	t0 = src->r + a;
	t1 = left_rotate_uint32(t0, 2) + t0 + 1;

	dst->l = src->l ^ t1;
	dst->r = src->r;
}
