#include <stdlib.h>
#include <string.h>

#include "ts_section_parser.h"
#include "ts_section_parser_error_code.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 inner structures
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
typedef struct {
	void                   *prev;
	void                   *next;
	TS_SECTION              sect;
	int32_t                 ref;
} TS_SECTION_ELEM;

typedef struct {
	TS_SECTION_ELEM        *head;
	TS_SECTION_ELEM        *tail;
	int32_t                 count;
} TS_SECTION_LIST;

typedef struct {

	int32_t                 pid;

	TS_SECTION_ELEM        *work;
	TS_SECTION_ELEM        *last;

	TS_SECTION_LIST         pool;
	TS_SECTION_LIST         buff;

	TS_SECTION_PARSER_STAT  stat;
	
} TS_SECTION_PARSER_PRIVATE_DATA;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 constant values
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MAX_RAW_SECTION_SIZE 4100

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (interface method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_ts_section_parser(void *parser);
static int reset_ts_section_parser(void *parser);
static int put_ts_section_parser(void *parser, TS_HEADER *hdr, uint8_t *data, int size);
static int get_ts_section_parser(void *parser, TS_SECTION *sect);
static int ret_ts_section_parser(void *parser, TS_SECTION *sect);
static int get_count_ts_section_parser(void *parser);
static int get_stat_ts_section_parser(void *parser, TS_SECTION_PARSER_STAT *stat);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 global function implementation (factory method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
TS_SECTION_PARSER *create_ts_section_parser()
{
	TS_SECTION_PARSER *r;
	TS_SECTION_PARSER_PRIVATE_DATA *prv;

	int n;

	n  = sizeof(TS_SECTION_PARSER_PRIVATE_DATA);
	n += sizeof(TS_SECTION_PARSER);
	
	prv = (TS_SECTION_PARSER_PRIVATE_DATA *)calloc(1, n);
	if(prv == NULL){
		/* failed on malloc() - no enough memory */
		return NULL;
	}

	prv->pid = -1;

	r = (TS_SECTION_PARSER *)(prv+1);
	r->private_data = prv;

	r->release = release_ts_section_parser;
	r->reset = reset_ts_section_parser;
	
	r->put = put_ts_section_parser;
	r->get = get_ts_section_parser;
	r->ret = ret_ts_section_parser;
	
	r->get_count = get_count_ts_section_parser;

	r->get_stat = get_stat_ts_section_parser;

	return r;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static TS_SECTION_PARSER_PRIVATE_DATA *private_data(void *parser);
static void teardown(TS_SECTION_PARSER_PRIVATE_DATA *prv);

static int put_exclude_section_start(TS_SECTION_PARSER_PRIVATE_DATA *prv, uint8_t *data, int size);
static int put_include_section_start(TS_SECTION_PARSER_PRIVATE_DATA *prv, uint8_t *data, int size);

static void reset_section(TS_SECTION *sect);
static void append_section_data(TS_SECTION *sect, uint8_t *data, int size);
static int check_section_complete(TS_SECTION *sect);

static int compare_elem_section(TS_SECTION_ELEM *a, TS_SECTION_ELEM *b);

static void cancel_elem_empty(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem);
static void cancel_elem_error(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem);
static void cancel_elem_same(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem);
static void commit_elem_updated(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem);

static TS_SECTION_ELEM *query_work_elem(TS_SECTION_PARSER_PRIVATE_DATA *prv);

static void extract_ts_section_header(TS_SECTION *sect);

static TS_SECTION_ELEM *create_ts_section_elem();
static TS_SECTION_ELEM *get_ts_section_list_head(TS_SECTION_LIST *list);
static void put_ts_section_list_tail(TS_SECTION_LIST *list, TS_SECTION_ELEM *elem);
static void unlink_ts_section_list(TS_SECTION_LIST *list, TS_SECTION_ELEM *elem);
static void clear_ts_section_list(TS_SECTION_LIST *list);

static uint32_t crc32(uint8_t *head, uint8_t *tail);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function implementation (interface method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_ts_section_parser(void *parser)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;

	prv = private_data(parser);
	if(prv == NULL){
		return;
	}

	teardown(prv);
	
	memset(parser, 0, sizeof(TS_SECTION_PARSER));
	free(prv);
}

static int reset_ts_section_parser(void *parser)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;

	prv = private_data(parser);
	if(prv == NULL){
		return TS_SECTION_PARSER_ERROR_INVALID_PARAM;
	}

	teardown(prv);

	return 0;
}

static int put_ts_section_parser(void *parser, TS_HEADER *hdr, uint8_t *data, int size)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;
	
	prv = private_data(parser);
	if( (prv == NULL) || (hdr == NULL) || (data == NULL) || (size < 1) ){
		return TS_SECTION_PARSER_ERROR_INVALID_PARAM;
	}

	if( (prv->pid >= 0) && (prv->pid != hdr->pid) ){
		return TS_SECTION_PARSER_ERROR_INVALID_TS_PID;
	}

	prv->pid = hdr->pid;

	if(hdr->payload_unit_start_indicator == 0){
		/* exclude section start */
		return put_exclude_section_start(prv, data, size);
	}else{
		/* include section start */
		return put_include_section_start(prv, data, size);
	}
}

static int get_ts_section_parser(void *parser, TS_SECTION *sect)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;
	TS_SECTION_ELEM *w;

	prv = private_data(parser);
	if( (prv == NULL) || (sect == NULL) ){
		return TS_SECTION_PARSER_ERROR_INVALID_PARAM;
	}

	w = get_ts_section_list_head(&(prv->buff));
	if(w == NULL){
		memset(sect, 0, sizeof(TS_SECTION));
		return TS_SECTION_PARSER_ERROR_NO_SECTION_DATA;
	}

	memcpy(sect, &(w->sect), sizeof(TS_SECTION));
	put_ts_section_list_tail(&(prv->pool), w);

	return 0;
}

static int ret_ts_section_parser(void *parser, TS_SECTION *sect)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;
	TS_SECTION_ELEM *w;

	prv = private_data(parser);
	if( (prv == NULL) || (sect == NULL) ){
		return TS_SECTION_PARSER_ERROR_INVALID_PARAM;
	}

	w = prv->pool.tail;
	while(w != NULL){
		if(w->sect.data == sect->data){
			break;
		}
		w = (TS_SECTION_ELEM *)(w->prev);
	}

	if( (w != NULL) && (w->ref > 0) ){
		w->ref -= 1;
	}

	return 0;
}

static int get_count_ts_section_parser(void *parser)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;
	
	prv = private_data(parser);
	if(prv == NULL){
		return TS_SECTION_PARSER_ERROR_INVALID_PARAM;
	}

	return prv->buff.count;
}

static int get_stat_ts_section_parser(void *parser, TS_SECTION_PARSER_STAT *stat)
{
	TS_SECTION_PARSER_PRIVATE_DATA *prv;

	prv = private_data(parser);
	if( (prv == NULL) || (stat == NULL) ){
		return TS_SECTION_PARSER_ERROR_INVALID_PARAM;
	}

	memcpy(stat, &(prv->stat), sizeof(TS_SECTION_PARSER_STAT));

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function implementation (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static TS_SECTION_PARSER_PRIVATE_DATA *private_data(void *parser)
{
	TS_SECTION_PARSER_PRIVATE_DATA *r;
	TS_SECTION_PARSER *p;

	p = (TS_SECTION_PARSER *)parser;
	if(p == NULL){
		return NULL;
	}

	r = (TS_SECTION_PARSER_PRIVATE_DATA *)(p->private_data);
	if( ((void *)(r+1)) != parser ){
		return NULL;
	}

	return r;
}

static void teardown(TS_SECTION_PARSER_PRIVATE_DATA *prv)
{
	prv->pid = -1;

	if(prv->work != NULL){
		free(prv->work);
		prv->work = NULL;
	}

	prv->last = NULL;
	
	clear_ts_section_list(&(prv->pool));
	clear_ts_section_list(&(prv->buff));

	memset(&(prv->stat), 0, sizeof(TS_SECTION_PARSER_STAT));
}

static int put_exclude_section_start(TS_SECTION_PARSER_PRIVATE_DATA *prv, uint8_t *data, int size)
{
	TS_SECTION_ELEM *w;

	w = prv->work;
	if( (w == NULL) || (w->sect.raw == w->sect.tail) ){
		/* no previous data */
		return 0;
	}

	append_section_data(&(w->sect), data, size);
	if(check_section_complete(&(w->sect)) == 0){
		/* need more data */
		return 0;
	}

	prv->work = NULL;

	if( (w->sect.hdr.section_syntax_indicator != 0) &&
	    (crc32(w->sect.raw, w->sect.tail) != 0) ){
		cancel_elem_error(prv, w);
		return TS_SECTION_PARSER_WARN_CRC_MISSMATCH;
	}

	if(compare_elem_section(w, prv->last) == 0){
		/* same section data */
		cancel_elem_same(prv, w);
		return 0;
	}

	commit_elem_updated(prv, w);
	return 0;
}

static int put_include_section_start(TS_SECTION_PARSER_PRIVATE_DATA *prv, uint8_t *data, int size)
{
	TS_SECTION_ELEM *w;
	
	int pointer_field;

	uint8_t *p;
	uint8_t *tail;

	int r;
	int length;

	p = data;
	tail = p + size;

	r = 0;

	pointer_field = p[0];
	p += 1;

	if( (p+pointer_field) >= tail ){
		/* input data is probably broken */
		w = prv->work;
		prv->work = NULL;
		if(w != NULL) {
			if(w->sect.raw != w->sect.tail){
				cancel_elem_error(prv, w);
				return TS_SECTION_PARSER_WARN_LENGTH_MISSMATCH;
			}
			cancel_elem_empty(prv, w);
		}
		return 0;
	}

	if(pointer_field > 0){
		r = put_exclude_section_start(prv, p, pointer_field);
		if(r < 0){
			return r;
		}
		p += pointer_field;
	}

	w = prv->work;
	prv->work = NULL;
	
	if(w != NULL){
		if(w->sect.raw != w->sect.tail){
			cancel_elem_error(prv, w);
			r = TS_SECTION_PARSER_WARN_LENGTH_MISSMATCH;
		}else{
			cancel_elem_empty(prv, w);
		}
		w = NULL;
	}

	do {
		
		w = query_work_elem(prv);
		if(w == NULL){
			return TS_SECTION_PARSER_ERROR_NO_ENOUGH_MEMORY;
		}
		
		append_section_data(&(w->sect), p, tail-p);
		if(check_section_complete(&(w->sect)) == 0){
			/* need more data */
			prv->work = w;
			return 0;
		}
		length = (w->sect.tail - w->sect.raw);

		if( (w->sect.hdr.section_syntax_indicator != 0) &&
		    (crc32(w->sect.raw, w->sect.tail) != 0) ){
			cancel_elem_error(prv, w);
			r = TS_SECTION_PARSER_WARN_CRC_MISSMATCH;
		}else if(compare_elem_section(w, prv->last) == 0){
			cancel_elem_same(prv, w);
		}else{
			commit_elem_updated(prv, w);
		}

		p += length;
		
	} while ( (p < tail) && (p[0] != 0xff) );

	return r;
}

static void reset_section(TS_SECTION *sect)
{
	memset(&(sect->hdr), 0, sizeof(TS_SECTION_HEADER));
	sect->tail = sect->raw;
	sect->data = NULL;
}

static void append_section_data(TS_SECTION *sect, uint8_t *data, int size)
{
	int m,n;
	
	m = sect->tail - sect->raw;
	n = MAX_RAW_SECTION_SIZE - m;

	if(size < n){
		n = size;
	}
	memcpy(sect->tail, data, n);
	sect->tail += n;
	m += n;

	if(sect->data == NULL){
		extract_ts_section_header(sect);
	}

	if(sect->data == NULL){
		/* need more data */
		return;
	}

	n = sect->hdr.section_length + 3;
	if(m > n){
		sect->tail = sect->raw + n;
	}

	return;
}

static int check_section_complete(TS_SECTION *sect)
{
	int m,n;
	
	if(sect->data == NULL){
		return 0;
	}

	m = sect->tail - sect->raw;
	n = sect->hdr.section_length + 3;

	if(n > m){
		return 0;
	}

	return 1;
}

static int compare_elem_section(TS_SECTION_ELEM *a, TS_SECTION_ELEM *b)
{
	int m,n;
	
	if( (a == NULL) || (b == NULL) ){
		return 1;
	}

	m = a->sect.tail - a->sect.raw;
	n = b->sect.tail - b->sect.raw;
	if( m != n ){
		return 1;
	}

	if(memcmp(a->sect.raw, b->sect.raw, m) != 0){
		return 1;
	}

	return 0;
}

static void cancel_elem_empty(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem)
{
	reset_section(&(elem->sect));
	elem->ref = 0;
	put_ts_section_list_tail(&(prv->pool), elem);
}

static void cancel_elem_error(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem)
{
	reset_section(&(elem->sect));
	elem->ref = 0;
	put_ts_section_list_tail(&(prv->pool), elem);
	prv->stat.total += 1;
	prv->stat.error += 1;
}

static void cancel_elem_same(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem)
{
	reset_section(&(elem->sect));
	elem->ref = 0;
	put_ts_section_list_tail(&(prv->pool), elem);
	prv->stat.total +=1;
}

static void commit_elem_updated(TS_SECTION_PARSER_PRIVATE_DATA *prv, TS_SECTION_ELEM *elem)
{
	if( (prv->last != NULL) && (prv->last->ref > 0) ){
		prv->last->ref -= 1;
	}

	elem->ref = 2;
	prv->last = elem;
	put_ts_section_list_tail(&(prv->buff), elem);
	prv->stat.total += 1;
	prv->stat.unique += 1;
}

static TS_SECTION_ELEM *query_work_elem(TS_SECTION_PARSER_PRIVATE_DATA *prv)
{
	TS_SECTION_ELEM *r;

	r = prv->pool.head;
	while(r != NULL){
		if(r->ref < 1){
			break;
		}
		r = (TS_SECTION_ELEM *)(r->next);
	}

	if(r != NULL){
		unlink_ts_section_list(&(prv->pool), r);
		reset_section(&(r->sect));
		r->ref = 0;
		return r;
	}

	return create_ts_section_elem();
}

static void extract_ts_section_header(TS_SECTION *sect)
{
	int size;
	uint8_t *p;
	
	sect->data = NULL;
	
	size = sect->tail - sect->raw;
	if(size < 3){
		/* need more data */
		return;
	}

	p = sect->raw;
	
	sect->hdr.table_id                 =  p[0];
	sect->hdr.section_syntax_indicator = (p[1] >> 7) & 0x01;
	sect->hdr.private_indicator        = (p[1] >> 6) & 0x01;
	sect->hdr.section_length           =((p[1] << 8) | p[2]) & 0x0fff;
	
	if(sect->hdr.section_syntax_indicator == 0){
		/* short format section header */
		sect->data = p+3;
		return;
	}

	/* long format section header */

	if(size < 8){
		/* need more data */
		return;
	}

	sect->hdr.table_id_extension       =((p[3] << 8) | p[4]);
	sect->hdr.version_number           = (p[5] >> 1) & 0x1f;
	sect->hdr.current_next_indicator   =  p[5]       & 0x01;
	sect->hdr.section_number           =  p[6];
	sect->hdr.last_section_number      =  p[7];

	sect->data = p+8;

	return;
}

static TS_SECTION_ELEM *create_ts_section_elem()
{
	TS_SECTION_ELEM *r;
	int n;

	n = sizeof(TS_SECTION_ELEM) + MAX_RAW_SECTION_SIZE;
	r = (TS_SECTION_ELEM *)calloc(1, n);
	if(r == NULL){
		/* failed on malloc() */
		return NULL;
	}

	r->sect.raw = (uint8_t *)(r+1);
	r->sect.tail = r->sect.raw;

	return r;
}

static TS_SECTION_ELEM *get_ts_section_list_head(TS_SECTION_LIST *list)
{
	TS_SECTION_ELEM *r;

	if(list == NULL){/* invalid param */
		return NULL;
	}

	r = list->head;
	if(r == NULL){
		return NULL;
	}

	list->head = (TS_SECTION_ELEM *)(r->next);
	if(list->head != NULL){
		list->head->prev = NULL;
		list->count -= 1;
	}else{
		list->tail = NULL;
		list->count = 0;
	}

	r->next = NULL;
	
	return r;
}

static void put_ts_section_list_tail(TS_SECTION_LIST *list, TS_SECTION_ELEM *elem)
{
	if( (list == NULL) || (elem == NULL) ){
		/* invalid param */
		return;
	}

	if(list->tail != NULL){
		elem->prev = list->tail;
		elem->next = NULL;
		list->tail->next = elem;
		list->tail = elem;
		list->count += 1;
	}else{
		elem->prev = NULL;
		elem->next = NULL;
		list->head = elem;
		list->tail = elem;
		list->count = 1;
	}
}

static void unlink_ts_section_list(TS_SECTION_LIST *list, TS_SECTION_ELEM *elem)
{
	TS_SECTION_ELEM *prev;
	TS_SECTION_ELEM *next;

	if( (list == NULL) || (elem == NULL) ){
		/* invalid param */
		return;
	}

	prev = (TS_SECTION_ELEM *)(elem->prev);
	next = (TS_SECTION_ELEM *)(elem->next);

	if(prev != NULL){
		prev->next = next;
	}else{
		list->head = next;
	}
	if(next != NULL){
		next->prev = prev;
	}else{
		list->tail = prev;
	}
	list->count -= 1;
}

static void clear_ts_section_list(TS_SECTION_LIST *list)
{
	TS_SECTION_ELEM *e;
	TS_SECTION_ELEM *n;

	if(list == NULL){ /* invalid param */
		return;
	}

	e = list->head;
	while(e != NULL){
		n = (TS_SECTION_ELEM *)(e->next);
		free(e);
		e = n;
	}
	
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

static uint32_t crc32(uint8_t *head, uint8_t *tail)
{
	uint32_t crc;
	uint8_t *p;

	static const uint32_t table[256] = {
		0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
		0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
		0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 
		0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
		
		0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9,
		0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 
		0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011,
		0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
		
		0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039,
		0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
		0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 
		0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
		
		0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49,
		0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
		0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 
		0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,

		0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE,
		0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072, 
		0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16,
		0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,

		0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 
		0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
		0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066,
		0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
		
		0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E,
		0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
		0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 
		0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,

		0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E,
		0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 
		0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686,
		0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,

		0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 
		0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
		0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F,
		0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53, 
		
		0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47,
		0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
		0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF,
		0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,

		0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7,
		0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 
		0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F,
		0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
		
		0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 
		0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
		0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F,
		0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3, 
		
		0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640,
		0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
		0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8,
		0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,

		0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30,
		0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
		0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088,
		0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,

		0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0,
		0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
		0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18,
		0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4, 

		0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0,
		0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
		0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 
		0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4,
	};
	
	crc = 0xffffffff;

	p = head;
	while(p < tail){
		crc = (crc << 8) ^ table[ ((crc >> 24) ^ p[0]) & 0xff ];
		p += 1;
	}

	return crc;
}

