#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "arib_std_b25.h"
#include "arib_std_b25_error_code.h"
#include "multi2.h"
#include "ts_common_types.h"
#include "ts_section_parser.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 inner structures
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
typedef struct {
	int32_t           pid;
	int32_t           type;
	void             *prev;
	void             *next;
} TS_STREAM_ELEM;

typedef struct {
	TS_STREAM_ELEM   *head;
	TS_STREAM_ELEM   *tail;
	int32_t           count;
} TS_STREAM_LIST;

typedef struct {
	
	uint8_t          *pool;
	uint8_t          *head;
	uint8_t          *tail;
	int32_t           max;
	
} TS_WORK_BUFFER;

typedef struct {

	int32_t            phase;

	int32_t            program_number;

	int32_t            pmt_pid;
	TS_SECTION_PARSER *pmt;

	int32_t            pcr_pid;

	TS_STREAM_LIST     streams;
	TS_STREAM_LIST     old_strm;

} TS_PROGRAM;

typedef struct {

	int32_t            ref;
	int32_t            phase;

	int32_t            locked;

	int32_t            ecm_pid;
	TS_SECTION_PARSER *ecm;

	MULTI2            *m2;

	int32_t            unpurchased;
	int32_t            last_error;

	void              *prev;
	void              *next;

} DECRYPTOR_ELEM;

typedef struct {
	DECRYPTOR_ELEM    *head;
	DECRYPTOR_ELEM    *tail;
	int32_t            count;
} DECRYPTOR_LIST;

typedef struct {
	uint32_t           ref;
	uint32_t           type;
	int64_t            normal_packet;
	int64_t            undecrypted;
	void              *target;
} PID_MAP;

typedef struct {

	int32_t            multi2_round;
	int32_t            strip;
	int32_t            emm_proc_on;
	
	int32_t            unit_size;

	int32_t            sbuf_offset;

	TS_SECTION_PARSER *pat;
	TS_SECTION_PARSER *cat;

	TS_STREAM_LIST     strm_pool;
	
	int32_t            p_count;
	TS_PROGRAM        *program;

	DECRYPTOR_LIST     decrypt;

	PID_MAP            map[0x2000];

	B_CAS_CARD        *bcas;
	B_CAS_ID           casid;
	int32_t            ca_system_id;

	int32_t            emm_pid;
	TS_SECTION_PARSER *emm;

	TS_WORK_BUFFER     sbuf;
	TS_WORK_BUFFER     dbuf;
	
} ARIB_STD_B25_PRIVATE_DATA;

typedef struct {
	int64_t            card_id;
	int32_t            associated_information_length;
	int32_t            protocol_number;
	int32_t            broadcaster_group_id;
	int32_t            update_number;
	int32_t            expiration_date;
} EMM_FIXED_PART;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 constant values
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
enum TS_STREAM_TYPE {
	TS_STREAM_TYPE_11172_2_VIDEO                = 0x01,
	TS_STREAM_TYPE_13818_2_VIDEO                = 0x02,
	TS_STREAM_TYPE_11172_3_AUDIO                = 0x03,
	TS_STREAM_TYPE_13818_3_AUDIO                = 0x04,
	TS_STREAM_TYPE_13818_1_PRIVATE_SECTIONS     = 0x05,
	TS_STREAM_TYPE_13818_1_PES_PRIVATE_DATA     = 0x06,
	TS_STREAM_TYPE_13522_MHEG                   = 0x07,
	TS_STREAM_TYPE_13818_1_DSM_CC               = 0x08,
	TS_STREAM_TYPE_H_222_1                      = 0x09,
	TS_STREAM_TYPE_13818_6_TYPE_A               = 0x0a,
	TS_STREAM_TYPE_13818_6_TYPE_B               = 0x0b,
	TS_STREAM_TYPE_13818_6_TYPE_C               = 0x0c,
	TS_STREAM_TYPE_13818_6_TYPE_D               = 0x0d,
	TS_STREAM_TYPE_13818_1_AUX                  = 0x0e,
	TS_STREAM_TYPE_13818_7_AUDIO_ADTS           = 0x0f,
	TS_STREAM_TYPE_14496_2_VISUAL               = 0x10,
	TS_STREAM_TYPE_14496_3_AUDIO_LATM           = 0x11,
	TS_STREAM_TYPE_14496_1_PES_SL_PACKET        = 0x12,
	TS_STREAM_TYPE_14496_1_SECTIONS_SL_PACKET   = 0x13,
	TS_STREAM_TYPE_13818_6_SYNC_DWLOAD_PROTCOL  = 0x14,
};

enum TS_SECTION_ID {
	TS_SECTION_ID_PROGRAM_ASSOCIATION           = 0x00,
	TS_SECTION_ID_CONDITIONAL_ACCESS            = 0x01,
	TS_SECTION_ID_PROGRAM_MAP                   = 0x02,
	TS_SECTION_ID_DESCRIPTION                   = 0x03,
	TS_SECTION_ID_14496_SCENE_DESCRIPTION       = 0x04,
	TS_SECTION_ID_14496_OBJECT_DESCRIPTION      = 0x05,

	/* ARIB STD-B10 stuff */
	TS_SECTION_ID_DSM_CC_HEAD                   = 0x3a,
	TS_SECTION_ID_DSM_CC_TAIL                   = 0x3f,
	TS_SECTION_ID_NIT_ACTUAL                    = 0x40,
	TS_SECTION_ID_NIT_OTHER                     = 0x41,
	TS_SECTION_ID_SDT_ACTUAL                    = 0x42,
	TS_SECTION_ID_SDT_OTHER                     = 0x46,
	TS_SECTION_ID_BAT                           = 0x4a,
	TS_SECTION_ID_EIT_ACTUAL_CURRENT            = 0x4e,
	TS_SECTION_ID_EIT_OTHER_CURRENT             = 0x4f,
	TS_SECTION_ID_EIT_ACTUAL_SCHEDULE_HEAD      = 0x50,
	TS_SECTION_ID_EIT_ACTUAL_SCHEDULE_TAIL      = 0x5f,
	TS_SECTION_ID_EIT_OTHER_SCHEDULE_HEAD       = 0x60,
	TS_SECTION_ID_EIT_OTHER_SCHEDULE_TAIL       = 0x6f,
	TS_SECTION_ID_TDT                           = 0x70,
	TS_SECTION_ID_RST                           = 0x71,
	TS_SECTION_ID_ST                            = 0x72,
	TS_SECTION_ID_TOT                           = 0x73,
	TS_SECTION_ID_AIT                           = 0x74,
	TS_SECTION_ID_DIT                           = 0x7e,
	TS_SECTION_ID_SIT                           = 0x7f,
	TS_SECTION_ID_ECM_S                         = 0x82,
	TS_SECTION_ID_ECM                           = 0x83,
	TS_SECTION_ID_EMM_S                         = 0x84,
	TS_SECTION_ID_EMM_MESSAGE                   = 0x85,
	TS_SECTION_ID_DCT                           = 0xc0,
	TS_SECTION_ID_DLT                           = 0xc1,
	TS_SECTION_ID_PCAT                          = 0xc2,
	TS_SECTION_ID_SDTT                          = 0xc3,
	TS_SECTION_ID_BIT                           = 0xc4,
	TS_SECTION_ID_NBIT_BODY                     = 0xc5,
	TS_SECTION_ID_NBIT_REFERENCE                = 0xc6,
	TS_SECTION_ID_LDT                           = 0xc7,
	TS_SECTION_ID_CDT                           = 0xc8,
	TS_SECTION_ID_LIT                           = 0xd0,
	TS_SECTION_ID_ERT                           = 0xd1,
	TS_SECTION_ID_ITT                           = 0xd2,
};

enum TS_DESCRIPTOR_TAG {
	TS_DESCRIPTOR_TAG_VIDEO_STREAM              = 0x02,
	TS_DESCRIPTOR_TAG_AUDIO_STREAM              = 0x03,
	TS_DESCRIPTOR_TAG_HIERARCHY                 = 0x04,
	TS_DESCRIPTOR_TAG_REGISTRATION              = 0x05,
	TS_DESCRIPTOR_TAG_DATA_STREAM_ALIGNMENT     = 0x06,
	TS_DESCRIPTOR_TAG_TARGET_BACKGROUND_GRID    = 0x07,
	TS_DESCRIPTOR_TAG_VIDEO_WINDOW              = 0x08,
	TS_DESCRIPTOR_TAG_CA                        = 0x09,
	TS_DESCRIPTOR_TAG_ISO_639_LANGUAGE          = 0x0a,
	TS_DESCRIPTOR_TAG_SYSTEM_CLOCK              = 0x0b,
	TS_DESCRIPTOR_TAG_MULTIPLEX_BUF_UTILIZ      = 0x0c,
	TS_DESCRIPTOR_TAG_COPYRIGHT                 = 0x0d,
	TS_DESCRIPTOR_TAG_MAXIMUM_BITRATE           = 0x0e,
	TS_DESCRIPTOR_TAG_PRIVATE_DATA_INDICATOR    = 0x0f,
	TS_DESCRIPTOR_TAG_SMOOTHING_BUFFER          = 0x10,
	TS_DESCRIPTOR_TAG_STD                       = 0x11,
	TS_DESCRIPTOR_TAG_IBP                       = 0x12,
	TS_DESCRIPTOR_TAG_MPEG4_VIDEO               = 0x1b,
	TS_DESCRIPTOR_TAG_MPEG4_AUDIO               = 0x1c,
	TS_DESCRIPTOR_TAG_IOD                       = 0x1d,
	TS_DESCRIPTOR_TAG_SL                        = 0x1e,
	TS_DESCRIPTOR_TAG_FMC                       = 0x1f,
	TS_DESCRIPTOR_TAG_EXTERNAL_ES_ID            = 0x20,
	TS_DESCRIPTOR_TAG_MUXCODE                   = 0x21,
	TS_DESCRIPTOR_TAG_FMX_BUFFER_SIZE           = 0x22,
	TS_DESCRIPTOR_TAG_MULTIPLEX_BUFFER          = 0x23,
	TS_DESCRIPTOR_TAG_AVC_VIDEO                 = 0x28,
	TS_DESCRIPTOR_TAG_AVC_TIMING_HRD            = 0x2a,

	/* ARIB STD-B10 stuff */
	TS_DESCRIPTOR_TAG_NETWORK_NAME              = 0x40,
	TS_DESCRIPTOR_TAG_SERVICE_LIST              = 0x41,
	TS_DESCRIPTOR_TAG_STUFFING                  = 0x42,
	TS_DESCRIPTOR_TAG_SATELLITE_DELIVERY_SYS    = 0x43,
	TS_DESCRIPTOR_TAG_CABLE_DISTRIBUTION        = 0x44,
	TS_DESCRIPTOR_TAG_BOUNQUET_NAME             = 0x47,
	TS_DESCRIPTOR_TAG_SERVICE                   = 0x48,
	TS_DESCRIPTOR_TAG_COUNTRY_AVAILABILITY      = 0x49,
	TS_DESCRIPTOR_TAG_LINKAGE                   = 0x4a,
	TS_DESCRIPTOR_TAG_NVOD_REFERENCE            = 0x4b,
	TS_DESCRIPTOR_TAG_TIME_SHIFTED_SERVICE      = 0x4c,
	TS_DESCRIPTOR_TAG_SHORT_EVENT               = 0x4d,
	TS_DESCRIPTOR_TAG_EXTENDED_EVENT            = 0x4e,
	TS_DESCRIPTOR_TAG_TIME_SHIFTED_EVENT        = 0x4f,
	TS_DESCRIPTOR_TAG_COMPONENT                 = 0x50,
	TS_DESCRIPTOR_TAG_MOSAIC                    = 0x51,
	TS_DESCRIPTOR_TAG_STREAM_IDENTIFIER         = 0x52,
	TS_DESCRIPTOR_TAG_CA_IDENTIFIER             = 0x53,
	TS_DESCRIPTOR_TAG_CONTENT                   = 0x54,
	TS_DESCRIPTOR_TAG_PARENTAL_RATING           = 0x55,
	TS_DESCRIPTOR_TAG_LOCAL_TIME_OFFSET         = 0x58,
	TS_DESCRIPTOR_TAG_PARTIAL_TRANSPORT_STREAM  = 0x63,
	TS_DESCRIPTOR_TAG_HIERARCHICAL_TRANSMISSION = 0xc0,
	TS_DESCRIPTOR_TAG_DIGITAL_COPY_CONTROL      = 0xc1,
	TS_DESCRIPTOR_TAG_NETWORK_IDENTIFICATION    = 0xc2,
	TS_DESCRIPTOR_TAG_PARTIAL_TRANSPORT_TIME    = 0xc3,
	TS_DESCRIPTOR_TAG_AUDIO_COMPONENT           = 0xc4,
	TS_DESCRIPTOR_TAG_HYPERLINK                 = 0xc5,
	TS_DESCRIPTOR_TAG_TARGET_REGION             = 0xc6,
	TS_DESCRIPTOR_TAG_DATA_COTENT               = 0xc7,
	TS_DESCRIPTOR_TAG_VIDEO_DECODE_CONTROL      = 0xc8,
	TS_DESCRIPTOR_TAG_DOWNLOAD_CONTENT          = 0xc9,
	TS_DESCRIPTOR_TAG_CA_EMM_TS                 = 0xca,
	TS_DESCRIPTOR_TAG_CA_CONTRACT_INFORMATION   = 0xcb,
	TS_DESCRIPTOR_TAG_CA_SERVICE                = 0xcc,
	TS_DESCRIPTOR_TAG_TS_INFORMATION            = 0xcd,
	TS_DESCRIPTOR_TAG_EXTENDED_BROADCASTER      = 0xce,
	TS_DESCRIPTOR_TAG_LOGO_TRANSMISSION         = 0xcf,
	TS_DESCRIPTOR_TAG_BASIC_LOCAL_EVENT         = 0xd0,
	TS_DESCRIPTOR_TAG_REFERENCE                 = 0xd1,
	TS_DESCRIPTOR_TAG_NODE_RELATION             = 0xd2,
	TS_DESCRIPTOR_TAG_SHORT_NODE_INFORMATION    = 0xd3,
	TS_DESCRIPTOR_TAG_STC_REFERENCE             = 0xd4,
	TS_DESCRIPTOR_TAG_SERIES                    = 0xd5,
	TS_DESCRIPTOR_TAG_EVENT_GROUP               = 0xd6,
	TS_DESCRIPTOR_TAG_SI_PARAMETER              = 0xd7,
	TS_DESCRIPTOR_TAG_BROADCASTER_NAME          = 0xd8,
	TS_DESCRIPTOR_TAG_COMPONENT_GROUP           = 0xd9,
	TS_DESCRIPTOR_TAG_SI_PRIME_TS               = 0xda,
	TS_DESCRIPTOR_TAG_BOARD_INFORMATION         = 0xdb,
	TS_DESCRIPTOR_TAG_LDT_LINKAGE               = 0xdc,
	TS_DESCRIPTOR_TAG_CONNECTED_TRANSMISSION    = 0xdd,
	TS_DESCRIPTOR_TAG_CONTENT_AVAILABILITY      = 0xde,
	TS_DESCRIPTOR_TAG_VALUE_EXTENSION           = 0xdf,
	TS_DESCRIPTOR_TAG_SERVICE_GROUP             = 0xe0,
	TS_DESCRIPTOR_TAG_CARUSEL_COMPOSITE         = 0xf7,
	TS_DESCRIPTOR_TAG_CONDITIONAL_PLAYBACK      = 0xf8,
	TS_DESCRIPTOR_TAG_CABLE_TS_DIVISSION        = 0xf9,
	TS_DESCRIPTOR_TAG_TERRESTRIAL_DELIVERY_SYS  = 0xfa,
	TS_DESCRIPTOR_TAG_PARTIAL_RECEPTION         = 0xfb,
	TS_DESCRIPTOR_TAG_EMERGENCY_INFOMATION      = 0xfc,
	TS_DESCRIPTOR_TAG_DATA_COMPONENT            = 0xfd,
	TS_DESCRIPTOR_TAG_SYSTEM_MANAGEMENT         = 0xfe,
};

enum PID_MAP_TYPE {
	PID_MAP_TYPE_UNKNOWN                        = 0x0000,
	PID_MAP_TYPE_PAT                            = 0x0100,
	PID_MAP_TYPE_PMT                            = 0x0200,
	PID_MAP_TYPE_NIT                            = 0x0300,
	PID_MAP_TYPE_PCR                            = 0x0400,
	PID_MAP_TYPE_ECM                            = 0x0500,
	PID_MAP_TYPE_EMM                            = 0x0600,
	PID_MAP_TYPE_EIT                            = 0x0700,
	PID_MAP_TYPE_CAT                            = 0x0800,
	PID_MAP_TYPE_OTHER                          = 0xff00,
};

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (interface method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_arib_std_b25(void *std_b25);
static int set_multi2_round_arib_std_b25(void *std_b25, int32_t round);
static int set_strip_arib_std_b25(void *std_b25, int32_t strip);
static int set_emm_proc_arib_std_b25(void *std_b25, int32_t on);
static int set_b_cas_card_arib_std_b25(void *std_b25, B_CAS_CARD *bcas);
static int reset_arib_std_b25(void *std_b25);
static int flush_arib_std_b25(void *std_b25);
static int put_arib_std_b25(void *std_b25, ARIB_STD_B25_BUFFER *buf);
static int get_arib_std_b25(void *std_b25, ARIB_STD_B25_BUFFER *buf);
static int get_program_count_arib_std_b25(void *std_b25);
static int get_program_info_arib_std_b25(void *std_b25, ARIB_STD_B25_PROGRAM_INFO *info, int idx);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 global function implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
ARIB_STD_B25 *create_arib_std_b25()
{
	int n;
	
	ARIB_STD_B25 *r;
	ARIB_STD_B25_PRIVATE_DATA *prv;

	n  = sizeof(ARIB_STD_B25_PRIVATE_DATA);
	n += sizeof(ARIB_STD_B25);
	
	prv = (ARIB_STD_B25_PRIVATE_DATA *)calloc(1, n);
	if(prv == NULL){
		return NULL;
	}

	prv->multi2_round = 4;

	r = (ARIB_STD_B25 *)(prv+1);
	r->private_data = prv;

	r->release = release_arib_std_b25;
	r->set_multi2_round = set_multi2_round_arib_std_b25;
	r->set_strip = set_strip_arib_std_b25;
	r->set_emm_proc = set_emm_proc_arib_std_b25;
	r->set_b_cas_card = set_b_cas_card_arib_std_b25;
	r->reset = reset_arib_std_b25;
	r->flush = flush_arib_std_b25;
	r->put = put_arib_std_b25;
	r->get = get_arib_std_b25;
	r->get_program_count = get_program_count_arib_std_b25;
	r->get_program_info = get_program_info_arib_std_b25;

	return r;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prottypes (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static ARIB_STD_B25_PRIVATE_DATA *private_data(void *std_b25);
static void teardown(ARIB_STD_B25_PRIVATE_DATA *prv);
static int select_unit_size(ARIB_STD_B25_PRIVATE_DATA *prv);
static int find_pat(ARIB_STD_B25_PRIVATE_DATA *prv);
static int proc_pat(ARIB_STD_B25_PRIVATE_DATA *prv);
static int check_pmt_complete(ARIB_STD_B25_PRIVATE_DATA *prv);
static int find_pmt(ARIB_STD_B25_PRIVATE_DATA *prv);
static int proc_pmt(ARIB_STD_B25_PRIVATE_DATA *prv, TS_PROGRAM *pgrm);
static int32_t find_ca_descriptor_pid(uint8_t *head, uint8_t *tail, int32_t ca_system_id);
static int32_t add_ecm_stream(ARIB_STD_B25_PRIVATE_DATA *prv, TS_STREAM_LIST *list, int32_t ecm_pid);
static int check_ecm_complete(ARIB_STD_B25_PRIVATE_DATA *prv);
static int find_ecm(ARIB_STD_B25_PRIVATE_DATA *prv);
static int proc_ecm(DECRYPTOR_ELEM *dec, B_CAS_CARD *bcas, int32_t multi2_round);
static int proc_arib_std_b25(ARIB_STD_B25_PRIVATE_DATA *prv);

static int proc_cat(ARIB_STD_B25_PRIVATE_DATA *prv);
static int proc_emm(ARIB_STD_B25_PRIVATE_DATA *prv);

static void release_program(ARIB_STD_B25_PRIVATE_DATA *prv, TS_PROGRAM *pgrm);

static void unref_stream(ARIB_STD_B25_PRIVATE_DATA *prv, int32_t pid);

static DECRYPTOR_ELEM *set_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv, int32_t pid);
static void remove_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv, DECRYPTOR_ELEM *dec);
static DECRYPTOR_ELEM *select_active_decryptor(DECRYPTOR_ELEM *a, DECRYPTOR_ELEM *b, int32_t pid);
static void bind_stream_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv, int32_t pid, DECRYPTOR_ELEM *dec);
static void unlock_all_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv);

static TS_STREAM_ELEM *get_stream_list_head(TS_STREAM_LIST *list);
static TS_STREAM_ELEM *find_stream_list_elem(TS_STREAM_LIST *list, int32_t pid);
static TS_STREAM_ELEM *create_stream_elem(int32_t pid, int32_t type);
static void put_stream_list_tail(TS_STREAM_LIST *list, TS_STREAM_ELEM *elem);
static void clear_stream_list(TS_STREAM_LIST *list);

static int reserve_work_buffer(TS_WORK_BUFFER *buf, int32_t size);
static int append_work_buffer(TS_WORK_BUFFER *buf, uint8_t *data, int32_t size);
static void reset_work_buffer(TS_WORK_BUFFER *buf);
static void release_work_buffer(TS_WORK_BUFFER *buf);

static void extract_ts_header(TS_HEADER *dst, uint8_t *src);
static void extract_emm_fixed_part(EMM_FIXED_PART *dst, uint8_t *src);

static uint8_t *resync(uint8_t *head, uint8_t *tail, int32_t unit);
static uint8_t *resync_force(uint8_t *head, uint8_t *tail, int32_t unit);

static uint32_t crc32(uint8_t *head, uint8_t *tail);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 interface method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void release_arib_std_b25(void *std_b25)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return;
	}

	teardown(prv);
	free(prv);
}

static int set_multi2_round_arib_std_b25(void *std_b25, int32_t round)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	prv->multi2_round = round;

	return 0;
}

static int set_strip_arib_std_b25(void *std_b25, int32_t strip)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	prv->strip = strip;

	return 0;
}

static int set_emm_proc_arib_std_b25(void *std_b25, int32_t on)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	prv->emm_proc_on = on;

	return 0;
}

static int set_b_cas_card_arib_std_b25(void *std_b25, B_CAS_CARD *bcas)
{
	int n;
	B_CAS_INIT_STATUS is;
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	prv->bcas = bcas;
	if(prv->bcas != NULL){
		n = prv->bcas->get_init_status(bcas, &is);
		if(n < 0){
			return ARIB_STD_B25_ERROR_INVALID_B_CAS_STATUS;
		}
		prv->ca_system_id = is.ca_system_id;
		n = prv->bcas->get_id(prv->bcas, &(prv->casid));
		if(n < 0){
			return ARIB_STD_B25_ERROR_INVALID_B_CAS_STATUS;
		}
	}

	return 0;
}

static int reset_arib_std_b25(void *std_b25)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	teardown(prv);

	return 0;
}

static int flush_arib_std_b25(void *std_b25)
{
	int r;
	int m,n;

	int32_t crypt;
	int32_t unit;
	int32_t pid;

	uint8_t *p;
	uint8_t *curr;
	uint8_t *tail;

	TS_HEADER hdr;
	DECRYPTOR_ELEM *dec;
	TS_PROGRAM *pgrm;

	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	if(prv->unit_size < 188){
		r = select_unit_size(prv);
		if(r < 0){
			return r;
		}
	}

	r = proc_arib_std_b25(prv);
	if(r < 0){
		return r;
	}

	unit = prv->unit_size;
	curr = prv->sbuf.head;
	tail = prv->sbuf.tail;

	m = prv->dbuf.tail - prv->dbuf.head;
	n = tail - curr;
	if(!reserve_work_buffer(&(prv->dbuf), m+n)){
		return ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
	}

	r = 0;

	while( (curr+188) <= tail ){
		
		if(curr[0] != 0x47){
			p = resync_force(curr, tail, unit);
			if(p == NULL){
				goto LAST;
			}
			curr = p;
		}
		
		extract_ts_header(&hdr, curr);
		crypt = hdr.transport_scrambling_control;
		pid = hdr.pid;

		if(hdr.transport_error_indicator != 0){
			/* bit error - append output buffer without parsing */
			if(!append_work_buffer(&(prv->dbuf), curr, 188)){
				r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				goto LAST;
			}
			goto NEXT;
		}
		
		if( (pid == 0x1fff) && (prv->strip) ){
			goto NEXT;
		}
		
		p = curr+4;
		if(hdr.adaptation_field_control & 0x02){
			p += (p[0]+1);
		}
		n = 188 - (p-curr);
		if( (n < 1) && ((n < 0) || (hdr.adaptation_field_control & 0x01)) ){
			/* broken packet */
			curr += 1;
			continue;
		}

		if( (crypt != 0) &&
		    (hdr.adaptation_field_control & 0x01) ){
			
			if(prv->map[pid].type == PID_MAP_TYPE_OTHER){
				dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
			}else if( (prv->map[pid].type == 0) &&
				  (prv->decrypt.count == 1) ){
				dec = prv->decrypt.head;
			}else{
				dec = NULL;
			}

			if( (dec != NULL) && (dec->m2 != NULL) ){
				m = dec->m2->decrypt(dec->m2, crypt, p, n);
				if(m < 0){
					r = ARIB_STD_B25_ERROR_DECRYPT_FAILURE;
					goto LAST;
				}
				curr[3] &= 0x3f;
				prv->map[pid].normal_packet += 1;
			}else{
				prv->map[pid].undecrypted += 1;
			}
		}else{
			prv->map[pid].normal_packet += 1;
		}

		if(!append_work_buffer(&(prv->dbuf), curr, 188)){
			r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
			goto LAST;
		}

		if(prv->map[pid].type == PID_MAP_TYPE_ECM){
			dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
			if( (dec == NULL) || (dec->ecm == NULL) ){
				/* this code will never execute */
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				goto LAST;
			}
			m = dec->ecm->put(dec->ecm, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				goto LAST;
			}
			m = dec->ecm->get_count(dec->ecm);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_ecm(dec, prv->bcas, prv->multi2_round);
			if(r < 0){
				goto LAST;
			}
		}else if(prv->map[pid].type == PID_MAP_TYPE_PMT){
			pgrm = (TS_PROGRAM *)(prv->map[pid].target);
			if( (pgrm == NULL) || (pgrm->pmt == NULL) ){
				/* this code will never execute */
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				goto LAST;
			}
			m = pgrm->pmt->put(pgrm->pmt, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				goto LAST;
			}
			m = pgrm->pmt->get_count(pgrm->pmt);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_pmt(prv, pgrm);
			if(r < 0){
				goto LAST;
			}
		}else if(prv->map[pid].type == PID_MAP_TYPE_EMM){
			if( prv->emm_proc_on == 0){
				goto NEXT;
			}
			if( prv->emm == NULL ){
				prv->emm = create_ts_section_parser();
				if(prv->emm == NULL){
					r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
					goto LAST;
				}
			}
			m = prv->emm->put(prv->emm, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
				goto LAST;
			}
			m = prv->emm->get_count(prv->emm);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_emm(prv);
			if(r < 0){
				goto LAST;
			}
		}else if(pid == 0x0001){
			if( prv->cat == NULL ){
				prv->cat = create_ts_section_parser();
				if(prv->cat == NULL){
					r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
					goto LAST;
				}
			}
			m = prv->cat->put(prv->cat, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
				goto LAST;
			}
			m = prv->cat->get_count(prv->cat);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_cat(prv);
			if(r < 0){
				goto LAST;
			}
		}else if(pid == 0x0000){
			if( prv->pat == NULL ){
				prv->pat = create_ts_section_parser();
				if(prv->pat == NULL){
					r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
					goto LAST;
				}
			}
			m = prv->pat->put(prv->pat, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
				goto LAST;
			}
			m = prv->pat->get_count(prv->pat);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_pat(prv);
			if(r < 0){
				goto LAST;
			}
		}
			
	NEXT:
		curr += unit;
	}

LAST:
	
	m = curr - prv->sbuf.head;
	n = tail - curr;
	if( (n < 1024) || (m > (prv->sbuf.max/2) ) ){
		p = prv->sbuf.pool;
		memcpy(p, curr, n);
		prv->sbuf.head = p;
		prv->sbuf.tail = p+n;
	}else{
		prv->sbuf.head = curr;
	}

	return r;
}

static int put_arib_std_b25(void *std_b25, ARIB_STD_B25_BUFFER *buf)
{
	int32_t n;
	
	ARIB_STD_B25_PRIVATE_DATA *prv;

	prv = private_data(std_b25);
	if( (prv == NULL) || (buf == NULL) ){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	if(!append_work_buffer(&(prv->sbuf), buf->data, buf->size)){
		return ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
	}

	if(prv->unit_size < 188){
		n = select_unit_size(prv);
		if(n < 0){
			return n;
		}
		if(prv->unit_size < 188){
			/* need more data */
			return 0;
		}
	}

	if(prv->p_count < 1){
		n = find_pat(prv);
		if(n < 0){
			return n;
		}
		if(prv->p_count < 1){
			if(prv->sbuf_offset < (16*1024*1024)){
				/* need more data */
				return 0;
			}else{
				/* exceed sbuf limit */
				return ARIB_STD_B25_ERROR_NO_PAT_IN_HEAD_16M;
			}
		}
		prv->sbuf_offset = 0;
	}

	if(!check_pmt_complete(prv)){
		n = find_pmt(prv);
		if(n < 0){
			return n;
		}
		if(!check_pmt_complete(prv)){
			if(prv->sbuf_offset < (32*1024*1024)){
				/* need more data */
				return 0;
			}else{
				/* exceed sbuf limit */
				return ARIB_STD_B25_ERROR_NO_PMT_IN_HEAD_32M;
			}
		}
		prv->sbuf_offset = 0;
	}

	if(!check_ecm_complete(prv)){
		n = find_ecm(prv);
		if(n < 0){
			return n;
		}
		if(!check_ecm_complete(prv)){
			if(prv->sbuf_offset < (32*1024*1024)){
				/* need more data */
				return 0;
			}else{
				/* exceed sbuf limit */
				return ARIB_STD_B25_ERROR_NO_ECM_IN_HEAD_32M;
			}
		}
		prv->sbuf_offset = 0;
	}

	return proc_arib_std_b25(prv);
}

static int get_arib_std_b25(void *std_b25, ARIB_STD_B25_BUFFER *buf)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;
	prv = private_data(std_b25);
	if( (prv == NULL) || (buf == NULL) ){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	buf->data = prv->dbuf.head;
	buf->size = prv->dbuf.tail - prv->dbuf.head;

	reset_work_buffer(&(prv->dbuf));

	return 0;
}

static int get_program_count_arib_std_b25(void *std_b25)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;
	
	prv = private_data(std_b25);
	if(prv == NULL){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	return prv->p_count;
}

static int get_program_info_arib_std_b25(void *std_b25, ARIB_STD_B25_PROGRAM_INFO *info, int idx)
{
	ARIB_STD_B25_PRIVATE_DATA *prv;

	TS_PROGRAM *pgrm;
	
	TS_STREAM_ELEM *strm;
	DECRYPTOR_ELEM *dec;

	int32_t pid;
	
	prv = private_data(std_b25);
	if( (prv == NULL) || (info == NULL) || (idx < 0) || (idx >= prv->p_count) ){
		return ARIB_STD_B25_ERROR_INVALID_PARAM;
	}

	pgrm = prv->program + idx;

	memset(info, 0, sizeof(ARIB_STD_B25_PROGRAM_INFO));

	info->program_number = pgrm->program_number;
	
	pid = pgrm->pmt_pid;
	info->total_packet_count += prv->map[pid].normal_packet;
	info->total_packet_count += prv->map[pid].undecrypted;
	info->undecrypted_packet_count += prv->map[pid].undecrypted;

	pid = pgrm->pcr_pid;
	if( (pid != 0) && (pid != 0x1fff) ){
		info->total_packet_count += prv->map[pid].normal_packet;
		info->total_packet_count += prv->map[pid].undecrypted;
		info->undecrypted_packet_count += prv->map[pid].undecrypted;
	}

	strm = pgrm->streams.head;
	while(strm != NULL){
		pid = strm->pid;
		if(prv->map[pid].type == PID_MAP_TYPE_ECM){
			dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
			info->ecm_unpurchased_count += dec->unpurchased;
			info->last_ecm_error_code = dec->last_error;
		}
		info->total_packet_count += prv->map[pid].normal_packet;
		info->total_packet_count += prv->map[pid].undecrypted;
		info->undecrypted_packet_count += prv->map[pid].undecrypted;
		strm = (TS_STREAM_ELEM *)(strm->next);
	}

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static ARIB_STD_B25_PRIVATE_DATA *private_data(void *std_b25)
{
	ARIB_STD_B25 *p;
	ARIB_STD_B25_PRIVATE_DATA *r;

	p = (ARIB_STD_B25 *)std_b25;
	if(p == NULL){
		return NULL;
	}

	r = (ARIB_STD_B25_PRIVATE_DATA *)p->private_data;
	if( ((void *)(r+1)) != ((void *)p) ){
		return NULL;
	}

	return r; 
}

static void teardown(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int i;
	
	prv->unit_size = 0;
	prv->sbuf_offset = 0;

	if(prv->pat != NULL){
		prv->pat->release(prv->pat);
		prv->pat = NULL;
	}
	if(prv->cat != NULL){
		prv->cat->release(prv->cat);
		prv->cat = NULL;
	}

	if(prv->program != NULL){
		for(i=0;i<prv->p_count;i++){
			release_program(prv, prv->program+i);
		}
		free(prv->program);
		prv->program = NULL;
	}
	prv->p_count = 0;

	clear_stream_list(&(prv->strm_pool));

	while(prv->decrypt.head != NULL){
		remove_decryptor(prv, prv->decrypt.head);
	}

	memset(prv->map, 0, sizeof(prv->map));

	prv->emm_pid = 0;
	if(prv->emm != NULL){
		prv->emm->release(prv->emm);
		prv->emm = NULL;
	}

	release_work_buffer(&(prv->sbuf));
	release_work_buffer(&(prv->dbuf));
}

static int select_unit_size(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int i;
	int m,n,w;
	int count[320-188];

	unsigned char *head;
	unsigned char *buf;
	unsigned char *tail;

	head = prv->sbuf.head;
	tail = prv->sbuf.tail;
	
	buf = head;
	memset(count, 0, sizeof(count));

	// 1st step, count up 0x47 interval
	while( (buf+188) < tail ){
		if(buf[0] != 0x47){
			buf += 1;
			continue;
		}
		m = 320;
		if( buf+m > tail ){
			m = tail-buf;
		}
		for(i=188;i<m;i++){
			if(buf[i] == 0x47){
				count[i-188] += 1;
			}
		}
		buf += 1;
	}

	// 2nd step, select maximum appeared interval
	m = 0;
	n = 0;
	for(i=188;i<320;i++){
		if(m < count[i-188]){
			m = count[i-188];
			n = i;
		}
	}

	// 3rd step, verify unit_size
	w = m*n;
	if( (m < 8) || ((w+3*n) < (tail-head)) ){
		return ARIB_STD_B25_ERROR_NON_TS_INPUT_STREAM;
	}

	prv->unit_size = n;

	return 0;
}

static int find_pat(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int n,size;
	
	int32_t unit;

	uint8_t *p;
	uint8_t *curr;
	uint8_t *tail;

	TS_HEADER hdr;

	r = 0;
	unit = prv->unit_size;
	curr = prv->sbuf.head + prv->sbuf_offset;
	tail = prv->sbuf.tail;

	while( (curr+unit) < tail ){
		if( (curr[0] != 0x47) || (curr[unit] != 0x47) ){
			p = resync(curr, tail, unit);
			if(p == NULL){
				goto LAST;
			}
			curr = p;
		}
		extract_ts_header(&hdr, curr);
		if(hdr.pid == 0x0000){
			
			p = curr+4;
			if(hdr.adaptation_field_control & 0x02){
				p += (p[0]+1);
			}
			size = 188 - (p-curr);
			if(size < 1){
				goto NEXT;
			}
			
			if(prv->pat == NULL){
				prv->pat = create_ts_section_parser();
				if(prv->pat == NULL){
					return ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				}
			}
			
			n = prv->pat->put(prv->pat, &hdr, p, size);
			if(n < 0){
				r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			n = prv->pat->get_count(prv->pat);
			if(n < 0){
				r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			if(n > 0){
				curr += unit;
				goto LAST;
			}
		}
	NEXT:
		curr += unit;
	}

LAST:
	prv->sbuf_offset = curr - prv->sbuf.head;

	if( (prv->pat != NULL) && (prv->pat->get_count(prv->pat) > 0) ){
		r = proc_pat(prv);
	}

	return r;
}

static int proc_pat(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int i,n;
	int len;
	int count;
	
	int32_t program_number;
	int32_t pid;

	uint8_t *head;
	uint8_t *tail;
	
	TS_PROGRAM *work;
	TS_SECTION  sect;

	r = 0;
	memset(&sect, 0, sizeof(sect));

	n = prv->pat->get(prv->pat, &sect);
	if(n < 0){
		r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
		goto LAST;
	}

	if(sect.hdr.table_id != TS_SECTION_ID_PROGRAM_ASSOCIATION){
		r = ARIB_STD_B25_WARN_TS_SECTION_ID_MISSMATCH;
		goto LAST;
	}

	len = (sect.tail - sect.data) - 4;

	count = len / 4;
	work = (TS_PROGRAM *)calloc(count, sizeof(TS_PROGRAM));
	if(work == NULL){
		r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
		goto LAST;
	}
	
	if(prv->program != NULL){
		for(i=0;i<prv->p_count;i++){
			release_program(prv, prv->program+i);
		}
		free(prv->program);
		prv->program = NULL;
	}
	prv->p_count = 0;
	memset(&(prv->map), 0, sizeof(prv->map));

	head = sect.data;
	tail = sect.tail-4;

	i = 0;
	while( (head+4) <= tail ){
		program_number = ((head[0] << 8) | head[1]);
		pid = ((head[2] << 8) | head[3]) & 0x1fff;
		if(program_number != 0){
			work[i].program_number = program_number;
			work[i].pmt_pid = pid;
			work[i].pmt = create_ts_section_parser();
			if(work[i].pmt == NULL){
				r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				break;
			}
			prv->map[pid].type = PID_MAP_TYPE_PMT;
			prv->map[pid].target = work+i;
			i += 1;
		}
		head += 4;
	}

	prv->program = work;
	prv->p_count = i;
	
	prv->map[0x0000].ref = 1;
	prv->map[0x0000].type = PID_MAP_TYPE_PAT;
	prv->map[0x0000].target = NULL;

LAST:
	if(sect.raw != NULL){
		n = prv->pat->ret(prv->pat, &sect);
		if( (n < 0) && (r == 0) ){
			r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
		}
	}

	return r;
}

static int check_pmt_complete(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int i,n;
	int num[3];

	memset(num, 0, sizeof(num));

	for(i=0;i<prv->p_count;i++){
		n = prv->program[i].phase;
		if(n < 0){
			n = 0;
		}else if(n > 2){
			n = 2;
		}
		num[n] += 1;
	}

	if(num[2] > 0){
		return 1;
	}

	if(num[0] > 0){
		return 0;
	}

	return 1;
}

static int find_pmt(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int n,size;
	
	int32_t unit;

	uint8_t *p;
	uint8_t *curr;
	uint8_t *tail;

	TS_HEADER hdr;
	TS_PROGRAM *pgrm;

	r = 0;
	unit = prv->unit_size;
	curr = prv->sbuf.head + prv->sbuf_offset;
	tail = prv->sbuf.tail;

	while( (curr+unit) < tail ){
		
		if( (curr[0] != 0x47) || (curr[unit] != 0x47) ){
			p = resync(curr, tail, unit);
			if(p == NULL){
				goto LAST;
			}
			curr = p;
		}
		
		extract_ts_header(&hdr, curr);
		
		if(prv->map[hdr.pid].type != PID_MAP_TYPE_PMT){
			goto NEXT;
		}
		pgrm = (TS_PROGRAM *)(prv->map[hdr.pid].target);
		if(pgrm == NULL){
			goto NEXT;
		}
		
		if(pgrm->phase == 0){
			
			p = curr + 4;
			if(hdr.adaptation_field_control & 0x02){
				p += (p[0]+1);
			}
			size = 188 - (p-curr);
			if(size < 1){
				goto NEXT;
			}
			
			if(pgrm->pmt == NULL){
				/* this code will never execute */
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			
			n = pgrm->pmt->put(pgrm->pmt, &hdr, p, size);
			if(n < 0){
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			n = pgrm->pmt->get_count(pgrm->pmt);
			if(n < 0){
				r =ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			if(n == 0){
				goto NEXT;
			}
			r = proc_pmt(prv, pgrm);
			if(r < 0){
				curr += unit;
				goto LAST;
			}
			if(r > 0){
				/* broken or unexpected section data */
				goto NEXT;
			}
			pgrm->phase = 1;
			if(check_pmt_complete(prv)){
				curr += unit;
				goto LAST;
			}
		}else{
			pgrm->phase = 2;
			curr += unit;
			goto LAST;
		}
		
	NEXT:
		curr += unit;
	}

LAST:
	prv->sbuf_offset = curr - prv->sbuf.head;

	return r;
}

static int proc_pmt(ARIB_STD_B25_PRIVATE_DATA *prv, TS_PROGRAM *pgrm)
{
	int r;

	int n;
	int length;

	uint8_t *head;
	uint8_t *tail;

	int32_t ecm_pid;
	int32_t pid;
	int32_t type;

	TS_SECTION sect;
	
	DECRYPTOR_ELEM *dec[2];
	DECRYPTOR_ELEM *dw;
	
	TS_STREAM_ELEM *strm;

	r = 0;
	dec[0] = NULL;
	memset(&sect, 0, sizeof(sect));

	n = pgrm->pmt->get(pgrm->pmt, &sect);
	if(n < 0){
		r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
		goto LAST;
	}
	if(sect.hdr.table_id != TS_SECTION_ID_PROGRAM_MAP){
		r = ARIB_STD_B25_WARN_TS_SECTION_ID_MISSMATCH;
		goto LAST;
	}
	
	head = sect.data;
	tail = sect.tail-4;

	pgrm->pcr_pid = ((head[0] << 8) | head[1]) & 0x1fff;
	length = ((head[2] << 8) | head[3]) & 0x0fff;
	head += 4;
	if(head+length > tail){
		r = ARIB_STD_B25_WARN_BROKEN_TS_SECTION;
		goto LAST;
	}

	/* find major ecm_pid and regist decryptor */
	ecm_pid = find_ca_descriptor_pid(head, head+length, prv->ca_system_id);
	if( (ecm_pid != 0) && (ecm_pid != 0x1fff) ){
		dec[0] = set_decryptor(prv, ecm_pid);
		if(dec[0] == NULL){
			r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
			goto LAST;
		}
		dec[0]->ref += 1;
	} else {
		if (prv->decrypt.count == 1) {
			dec[0] = prv->decrypt.head;
			dec[0]->ref += 1;
		}
	}
	head += length;

	
	/* unref old stream entries */
	while( (strm = get_stream_list_head(&(pgrm->old_strm))) != NULL ){
		unref_stream(prv, strm->pid);
		memset(strm, 0, sizeof(TS_STREAM_ELEM));
		put_stream_list_tail(&(prv->strm_pool), strm);
	}

	/* save current streams */
	memcpy(&(pgrm->old_strm), &(pgrm->streams), sizeof(TS_STREAM_LIST));
	memset(&(pgrm->streams), 0, sizeof(TS_STREAM_LIST));

	/* add current stream entries */
	if( (ecm_pid != 0) && (ecm_pid != 0x1fff) ){
		if(!add_ecm_stream(prv, &(pgrm->streams), ecm_pid)){
			r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
			goto LAST;
		}
	}

	while( head+4 < tail ){

		type = head[0];
		pid = ((head[1] << 8) | head[2]) & 0x1fff;
		length = ((head[3] << 8) | head[4]) & 0x0fff;
		head += 5;
		ecm_pid = find_ca_descriptor_pid(head, head+length, prv->ca_system_id);
		head += length;
		
		if( (ecm_pid != 0) && (ecm_pid != 0x1fff) ){
			dec[1] = set_decryptor(prv, ecm_pid);
			if(dec[1] == NULL){
				r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				goto LAST;
			}
			if(!add_ecm_stream(prv, &(pgrm->streams), ecm_pid)){
				r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				goto LAST;
			}
		}else{
			dec[1] = NULL;
		}

		strm = get_stream_list_head(&(prv->strm_pool));
		if( strm == NULL ){
			strm = create_stream_elem(pid, type);
			if(strm == NULL){
				r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				goto LAST;
			}
		}else{
			strm->pid = pid;
			strm->type = type;
		}
		
		prv->map[pid].type = PID_MAP_TYPE_OTHER;
		prv->map[pid].ref += 1;

		dw = select_active_decryptor(dec[0], dec[1], ecm_pid);
		bind_stream_decryptor(prv, pid, dw);
		
		put_stream_list_tail(&(pgrm->streams), strm);
	}
	
LAST:
	if( dec[0] != NULL ){
		dec[0]->ref -= 1;
		if( dec[0]->ref < 1 ){
			remove_decryptor(prv, dec[0]);
			dec[0] = NULL;
		}
	}

	if(sect.raw != NULL){
		n = pgrm->pmt->ret(pgrm->pmt, &sect);
		if( (n < 0) && (r == 0) ){
			return ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
		}
	}

	return r;
}
		
static int32_t find_ca_descriptor_pid(uint8_t *head, uint8_t *tail, int32_t ca_system_id)
{
	uint32_t ca_pid;
	uint32_t ca_sys_id;
	
	uint32_t tag;
	uint32_t len;

	while(head+1 < tail){
		tag = head[0];
		len = head[1];
		head += 2;
		if( (tag == 0x09) && /* CA_descriptor */
		    (len >= 4) &&
		    (head+len <= tail) ){
			ca_sys_id = ((head[0] << 8) | head[1]);
			ca_pid = ((head[2] << 8) | head[3]) & 0x1fff;
			if(ca_sys_id == ca_system_id){
				return ca_pid;
			}
		}
		head += len;
	}

	return 0;
}

static int add_ecm_stream(ARIB_STD_B25_PRIVATE_DATA *prv, TS_STREAM_LIST *list, int32_t ecm_pid)
{
	TS_STREAM_ELEM *strm;

	strm = find_stream_list_elem(list, ecm_pid);
	if(strm != NULL){
		// ECM is already registered
		return 1;
	}

	strm = get_stream_list_head(&(prv->strm_pool));
	if(strm == NULL){
		strm = create_stream_elem(ecm_pid, PID_MAP_TYPE_ECM);
		if(strm == NULL){
			return 0;
		}
	}else{
		strm->pid = ecm_pid;
		strm->type = PID_MAP_TYPE_ECM;
	}

	put_stream_list_tail(list, strm);
	prv->map[ecm_pid].ref += 1;

	return 1;
}

static int check_ecm_complete(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int n,num[3];
	DECRYPTOR_ELEM *e;

	memset(num, 0, sizeof(num));

	e = prv->decrypt.head;
	while( e != NULL ){
		n = e->phase;
		if(n < 0){
			n = 0;
		}else if(n > 2){
			n = 2;
		}
		num[n] += 1;
		e = (DECRYPTOR_ELEM *)(e->next);
	}

	if(num[2] > 0){
		return 1;
	}

	if(num[0] > 0){
		return 0;
	}

	return 1;
}

static int find_ecm(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int n,size;

	int32_t unit;

	uint8_t *p;
	uint8_t *curr;
	uint8_t *tail;

	TS_HEADER hdr;
	DECRYPTOR_ELEM *dec;

	r = 0;
	unit = prv->unit_size;
	curr = prv->sbuf.head + prv->sbuf_offset;
	tail = prv->sbuf.tail;

	while( (curr+unit) < tail ){
		if( (curr[0] != 0x47) || (curr[unit] != 0x47) ){
			p = resync(curr, tail, unit);
			if(p == NULL){
				goto LAST;
			}
			curr = p;
		}
		extract_ts_header(&hdr, curr);
		if(prv->map[hdr.pid].type != PID_MAP_TYPE_ECM){
			goto NEXT;
		}
		dec = (DECRYPTOR_ELEM *)(prv->map[hdr.pid].target);
		if(dec == NULL){
			goto NEXT;
		}

		if(dec->phase == 0){
			
			p = curr + 4;
			if(hdr.adaptation_field_control & 0x02){
				p += (p[0]+1);
			}
			size = 188 - (p-curr);
			if(size < 1){
				goto NEXT;
			}
			
			if(dec->ecm == NULL){
				/*  this code will never execute */
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
				
			n = dec->ecm->put(dec->ecm, &hdr, p, size);
			if(n < 0){
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			n = dec->ecm->get_count(dec->ecm);
			if(n < 0){
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				curr += unit;
				goto LAST;
			}
			if(n == 0){
				goto NEXT;
			}

			r = proc_ecm(dec, prv->bcas, prv->multi2_round);
			if(r < 0){
				curr += unit;
				goto LAST;
			}
			if( (r > 0) && (r != ARIB_STD_B25_WARN_UNPURCHASED_ECM) ){
				/* broken or unexpected section data */
				goto NEXT;
			}
			
			dec->phase = 1;
			if(check_ecm_complete(prv)){
				curr += unit;
				goto LAST;
			}

		}else{
			dec->phase = 2;
			curr += unit;
			goto LAST;
		}
		
	NEXT:
		curr += unit;
	}

LAST:
	prv->sbuf_offset = curr - prv->sbuf.head;

	return r;
}

static int proc_ecm(DECRYPTOR_ELEM *dec, B_CAS_CARD *bcas, int32_t multi2_round)
{
	int r,n;
	int length;

	uint8_t *p;
	
	B_CAS_INIT_STATUS is;
	B_CAS_ECM_RESULT res;

	TS_SECTION sect;

	r = 0;
	memset(&sect, 0, sizeof(sect));

	if(bcas == NULL){
		r = ARIB_STD_B25_ERROR_EMPTY_B_CAS_CARD;
		goto LAST;
	}

	n = dec->ecm->get(dec->ecm, &sect);
	if(n < 0){
		r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
		goto LAST;
	}
	if(sect.hdr.table_id != TS_SECTION_ID_ECM_S){
		r = ARIB_STD_B25_WARN_TS_SECTION_ID_MISSMATCH;
		goto LAST;
	}
	
	if(dec->locked){
		/* previous ECM has returned unpurchased
		   skip this pid for B-CAS card load reduction */
		dec->unpurchased += 1;
		r = ARIB_STD_B25_WARN_UNPURCHASED_ECM;
		goto LAST;
	}

	length = (sect.tail - sect.data) - 4;
	p = sect.data;

	r = bcas->proc_ecm(bcas, &res, p, length);
	if(r < 0){
		if(dec->m2 != NULL){
			dec->m2->clear_scramble_key(dec->m2);
		}
		r = ARIB_STD_B25_ERROR_ECM_PROC_FAILURE;
		goto LAST;
	}
	
	if( (res.return_code != 0x0800) &&
	    (res.return_code != 0x0400) &&
	    (res.return_code != 0x0200) ){
		/* return_code is not equal "purchased" */
		if(dec->m2 != NULL){
			dec->m2->release(dec->m2);
			dec->m2 = NULL;
		}
		dec->unpurchased += 1;
		dec->last_error = res.return_code;
		dec->locked += 1;
		r = ARIB_STD_B25_WARN_UNPURCHASED_ECM;
		goto LAST;
	}

	if(dec->m2 == NULL){
		dec->m2 = create_multi2();
		if(dec->m2 == NULL){
			return ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
		}
		r = bcas->get_init_status(bcas, &is);
		if(r < 0){
			return ARIB_STD_B25_ERROR_INVALID_B_CAS_STATUS;
		}
		dec->m2->set_system_key(dec->m2, is.system_key);
		dec->m2->set_init_cbc(dec->m2, is.init_cbc);
		dec->m2->set_round(dec->m2, multi2_round);
	}

	dec->m2->set_scramble_key(dec->m2, res.scramble_key);

	if (0) {
		int i;
		fprintf(stdout, "----\n");
		fprintf(stdout, "odd: ");
		for(i=0;i<8;i++){
			fprintf(stdout, " %02x", res.scramble_key[i]);
		}
		fprintf(stdout, "\n");
		fprintf(stdout, "even:");
		for(i=8;i<16;i++){
			fprintf(stdout, " %02x", res.scramble_key[i]);
		}
		fprintf(stdout, "\n");
		fflush(stdout);
	}
	
LAST:
	if(sect.raw != NULL){
		n = dec->ecm->ret(dec->ecm, &sect);
		if( (n < 0) && (r == 0) ){
			r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
		}
	}
	
	return r;
}

static void dump_pts(uint8_t *src, int32_t crypt)
{
	int32_t pts_dts_flag;
	int64_t pts,dts;
	
	src += 4; // TS ヘッダ部
	src += 4; // start_code_prefix + stream_id 部
	src += 2; // packet_length 部

	pts_dts_flag = (src[1] >> 6) & 3;

	src += 3;
	if(pts_dts_flag & 2){
		// PTS
		pts = (src[0] >> 1) & 0x07;
		pts <<= 15;
		pts += ((src[1] << 8) + src[2]) >> 1;
		pts <<= 15;
		pts += ((src[3] << 8) + src[4]) >> 1;
		src += 5;
	}
	if(pts_dts_flag & 1){
		// DTS
		dts = (src[0] >> 1) & 0x07;
		dts <<= 15;
		dts += ((src[1] << 8) + src[2]) >> 1;
		dts <<= 15;
		dts += ((src[3] << 8) + src[4]) >> 1;
	}

	if(pts_dts_flag == 2){
		fprintf(stdout, "  key=%d, pts=%I64d\n", crypt, pts/90);
		fflush(stdout);
	}
}

static int proc_arib_std_b25(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int m,n;

	int32_t crypt;
	int32_t unit;
	int32_t pid;

	uint8_t *p;
	uint8_t *curr;
	uint8_t *tail;

	TS_HEADER hdr;
	DECRYPTOR_ELEM *dec;
	TS_PROGRAM *pgrm;

	unit = prv->unit_size;
	curr = prv->sbuf.head;
	tail = prv->sbuf.tail;

	m = prv->dbuf.tail - prv->dbuf.head;
	n = tail - curr;
	if(!reserve_work_buffer(&(prv->dbuf), m+n)){
		return ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
	}

	r = 0;

	while( (curr+unit) < tail ){
		
		if( (curr[0] != 0x47) || (curr[unit] != 0x47) ){
			p = resync(curr, tail, unit);
			if(p == NULL){
				goto LAST;
			}
			curr = p;
		}
		
		extract_ts_header(&hdr, curr);
		crypt = hdr.transport_scrambling_control;
		pid = hdr.pid;

		if(hdr.transport_error_indicator != 0){
			/* bit error - append output buffer without parsing */
			if(!append_work_buffer(&(prv->dbuf), curr, 188)){
				r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
				goto LAST;
			}
			goto NEXT;
		}
		
		if( (pid == 0x1fff) && (prv->strip) ){
			/* strip null(padding) stream */
			goto NEXT;
		}

		p = curr+4;
		if(hdr.adaptation_field_control & 0x02){
			p += (p[0]+1);
		}
		n = 188 - (p-curr);
		if( (n < 1) && ((n < 0) || (hdr.adaptation_field_control & 0x01)) ){
			/* broken packet */
			curr += 1;
			continue;
		}

		if( (crypt != 0) &&
		    (hdr.adaptation_field_control & 0x01) ){
			
			if(prv->map[pid].type == PID_MAP_TYPE_OTHER){
				dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
			}else if( (prv->map[pid].type == 0) &&
			          (prv->decrypt.count == 1) ){
				dec = prv->decrypt.head;
			}else{
				dec = NULL;
			}

			if( (dec != NULL) && (dec->m2 != NULL) ){
				m = dec->m2->decrypt(dec->m2, crypt, p, n);
				if(m < 0){
					r = ARIB_STD_B25_ERROR_DECRYPT_FAILURE;
					goto LAST;
				}
				curr[3] &= 0x3f;
				prv->map[pid].normal_packet += 1;
			}else{
				prv->map[pid].undecrypted += 1;
			}
		}else{
			prv->map[pid].normal_packet += 1;
		}
#if 0
		if( (hdr.payload_unit_start_indicator != 0) && (pid == 0x111) ){
			dump_pts(curr, crypt);
		}
#endif
		if(!append_work_buffer(&(prv->dbuf), curr, 188)){
			r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
			goto LAST;
		}

		if(prv->map[pid].type == PID_MAP_TYPE_ECM){
			dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
			if( (dec == NULL) || (dec->ecm == NULL) ){
				/* this code will never execute */
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				goto LAST;
			}
			m = dec->ecm->put(dec->ecm, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				goto LAST;
			}
			m = dec->ecm->get_count(dec->ecm);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_ecm(dec, prv->bcas, prv->multi2_round);
			if(r < 0){
				goto LAST;
			}
		}else if(prv->map[pid].type == PID_MAP_TYPE_PMT){
			pgrm = (TS_PROGRAM *)(prv->map[pid].target);
			if( (pgrm == NULL) || (pgrm->pmt == NULL) ){
				/* this code will never execute */
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				goto LAST;
			}
			m = pgrm->pmt->put(pgrm->pmt, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				goto LAST;
			}
			m = pgrm->pmt->get_count(pgrm->pmt);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_pmt(prv, pgrm);
			if(r < 0){
				goto LAST;
			}
		}else if(prv->map[pid].type == PID_MAP_TYPE_EMM){
			if( prv->emm_proc_on == 0){
				goto NEXT;
			}
			if( prv->emm == NULL ){
				prv->emm = create_ts_section_parser();
				if(prv->emm == NULL){
					r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
					goto LAST;
				}
			}
			m = prv->emm->put(prv->emm, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
				goto LAST;
			}
			m = prv->emm->get_count(prv->emm);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_emm(prv);
			if(r < 0){
				goto LAST;
			}
		}else if(pid == 0x0001){
			if( prv->cat == NULL ){
				prv->cat = create_ts_section_parser();
				if(prv->cat == NULL){
					r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
					goto LAST;
				}
			}
			m = prv->cat->put(prv->cat, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
				goto LAST;
			}
			m = prv->cat->get_count(prv->cat);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_cat(prv);
			if(r < 0){
				goto LAST;
			}
		}else if(pid == 0x0000){
			if( prv->pat == NULL ){
				prv->pat = create_ts_section_parser();
				if(prv->pat == NULL){
					r = ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY;
					goto LAST;
				}
			}
			m = prv->pat->put(prv->pat, &hdr, p, n);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
				goto LAST;
			}
			m = prv->pat->get_count(prv->pat);
			if(m < 0){
				r = ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE;
				goto LAST;
			}
			if(m == 0){
				goto NEXT;
			}
			r = proc_pat(prv);
			goto LAST;
		}
			
	NEXT:
		curr += unit;
	}

LAST:
	m = curr - prv->sbuf.head;
	n = tail - curr;
	if( (n < 1024) || (m > (prv->sbuf.max/2) ) ){
		p = prv->sbuf.pool;
		memcpy(p, curr, n);
		prv->sbuf.head = p;
		prv->sbuf.tail = p+n;
	}else{
		prv->sbuf.head = curr;
	}

	return r;
}

static int proc_cat(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int n;
	int emm_pid;

	TS_SECTION sect;

	r = 0;
	memset(&sect, 0, sizeof(sect));

	n = prv->cat->get(prv->cat, &sect);
	if(n < 0){
		r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
		goto LAST;
	}

	if(sect.hdr.table_id != TS_SECTION_ID_CONDITIONAL_ACCESS){
		r = ARIB_STD_B25_WARN_TS_SECTION_ID_MISSMATCH;
		goto LAST;
	}

	emm_pid = find_ca_descriptor_pid(sect.data, sect.tail-4, prv->ca_system_id);
	if( (emm_pid != 0x0000) && (emm_pid != 0x1fff) ){
		if( (prv->map[emm_pid].target != NULL) &&
		    (prv->map[emm_pid].type == PID_MAP_TYPE_OTHER) ){
			DECRYPTOR_ELEM *dec;
			dec = (DECRYPTOR_ELEM *)(prv->map[emm_pid].target);
			dec->ref -= 1;
			if(dec->ref < 1){
				remove_decryptor(prv, dec);
			}
		}
		prv->emm_pid = emm_pid;
		prv->map[emm_pid].ref = 1;
		prv->map[emm_pid].type = PID_MAP_TYPE_EMM;
		prv->map[emm_pid].target = NULL;
	}
	
	prv->map[0x0001].ref = 1;
	prv->map[0x0001].type = PID_MAP_TYPE_CAT;
	prv->map[0x0001].target = NULL;

LAST:

	if(sect.raw != NULL){
		n = prv->cat->ret(prv->cat, &sect);
		if( (n < 0) && (r == 0) ){
			r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
		}
	}

	return r;
}

static int proc_emm(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	int r;
	int i,j,n;

	int len;

	uint8_t *head;
	uint8_t *tail;

	TS_SECTION sect;
	EMM_FIXED_PART emm_hdr;

	r = 0;
	memset(&sect, 0, sizeof(sect));

	if(prv->bcas == NULL){
		r = ARIB_STD_B25_ERROR_EMPTY_B_CAS_CARD;
		goto LAST;
	}

	while( (n = prv->emm->get_count(prv->emm)) > 0 ){

		n = prv->emm->get(prv->emm, &sect);
		if(n < 0){
			r = ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE;
			goto LAST;
		}

		if(sect.hdr.table_id == TS_SECTION_ID_EMM_MESSAGE){
			/* EMM_MESSAGE is not supported */
			goto NEXT;
		}else if(sect.hdr.table_id != TS_SECTION_ID_EMM_S){
			r = ARIB_STD_B25_WARN_TS_SECTION_ID_MISSMATCH;
			goto LAST;
		}

		head = sect.data;
		tail = sect.tail - 4;

		i = 0;
		while( (head+13) <= tail ){
			
			extract_emm_fixed_part(&emm_hdr, head);
			len = emm_hdr.associated_information_length+7;
			if( (head+len) > tail ){
				/* broken EMM element */
				goto NEXT;
			}
			
			for(j=0;j<prv->casid.count;j++){
				if(prv->casid.data[j] == emm_hdr.card_id){
					n = prv->bcas->proc_emm(prv->bcas, head, len);
					if(n < 0){
						r = ARIB_STD_B25_ERROR_EMM_PROC_FAILURE;
						goto LAST;
					}
					unlock_all_decryptor(prv);
				}
			}

			head += len;
		}

	NEXT:
		if(sect.raw != NULL){
			n = prv->emm->ret(prv->emm, &sect);
			if( (n < 0) && (r == 0) ){
				r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
				goto LAST;
			}
			memset(&sect, 0, sizeof(sect));
		}
	}

LAST:
	if(sect.raw != NULL){
		n = prv->emm->ret(prv->emm, &sect);
		if( (n < 0) && (r == 0) ){
			r = ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE;
		}
	}

	return r;
}

static void release_program(ARIB_STD_B25_PRIVATE_DATA *prv, TS_PROGRAM *pgrm)
{
	int32_t pid;
	TS_STREAM_ELEM *strm;

	pid = pgrm->pmt_pid;
	
	if(pgrm->pmt != NULL){
		pgrm->pmt->release(pgrm->pmt);
		pgrm->pmt = NULL;
	}

	while( (strm = get_stream_list_head(&(pgrm->old_strm))) != NULL ){
		unref_stream(prv, strm->pid);
		memset(strm, 0, sizeof(TS_STREAM_ELEM));
		put_stream_list_tail(&(prv->strm_pool), strm);
	}

	while( (strm = get_stream_list_head(&(pgrm->streams))) != NULL ){
		unref_stream(prv, strm->pid);
		memset(strm, 0, sizeof(TS_STREAM_ELEM));
		put_stream_list_tail(&(prv->strm_pool), strm);
	}

	prv->map[pid].type = PID_MAP_TYPE_UNKNOWN;
	prv->map[pid].ref = 0;
	prv->map[pid].target = NULL;
}

static void unref_stream(ARIB_STD_B25_PRIVATE_DATA *prv, int32_t pid)
{
	DECRYPTOR_ELEM *dec;

	prv->map[pid].ref -= 1;
	if( prv->map[pid].ref < 1 ){
		if( (prv->map[pid].target != NULL) &&
		    (prv->map[pid].type == PID_MAP_TYPE_OTHER) ){
			dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
			dec->ref -= 1;
			if(dec->ref < 1){
				remove_decryptor(prv, dec);
			}
		}
		prv->map[pid].type = PID_MAP_TYPE_UNKNOWN;
		prv->map[pid].ref = 0;
		prv->map[pid].target = NULL;
	}
}

static DECRYPTOR_ELEM *set_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv, int32_t pid)
{
	DECRYPTOR_ELEM *r;

	r = NULL;
	if(prv->map[pid].type == PID_MAP_TYPE_ECM){
		r = (DECRYPTOR_ELEM *)(prv->map[pid].target);
		if(r != NULL){
			return r;
		}
	}
	r = (DECRYPTOR_ELEM *)calloc(1, sizeof(DECRYPTOR_ELEM));
	if(r == NULL){
		return NULL;
	}
	r->ecm_pid = pid;
	r->ecm = create_ts_section_parser();
	if(r->ecm == NULL){
		free(r);
		return NULL;
	}

	if(prv->decrypt.tail != NULL){
		r->prev = prv->decrypt.tail;
		r->next = NULL;
		prv->decrypt.tail->next = r;
		prv->decrypt.tail = r;
		prv->decrypt.count += 1;
	}else{
		r->prev = NULL;
		r->next = NULL;
		prv->decrypt.head = r;
		prv->decrypt.tail = r;
		prv->decrypt.count = 1;
	}

	if( (prv->map[pid].type == PID_MAP_TYPE_OTHER) &&
	    (prv->map[pid].target != NULL) ){
		DECRYPTOR_ELEM *dec;
		dec = (DECRYPTOR_ELEM *)(prv->map[pid].target);
		dec->ref -= 1;
		if(dec->ref < 1){
			remove_decryptor(prv, dec);
		}
	}

	prv->map[pid].type = PID_MAP_TYPE_ECM;
	prv->map[pid].target = r;

	return r;
}

static void remove_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv, DECRYPTOR_ELEM *dec)
{
	int32_t pid;

	DECRYPTOR_ELEM *prev;
	DECRYPTOR_ELEM *next;

	pid = dec->ecm_pid;
	if( (prv->map[pid].type == PID_MAP_TYPE_ECM) &&
	    (prv->map[pid].target == ((void *)dec)) ){
		prv->map[pid].type = PID_MAP_TYPE_UNKNOWN;
		prv->map[pid].target = NULL;
	}

	prev = (DECRYPTOR_ELEM *)(dec->prev);
	next = (DECRYPTOR_ELEM *)(dec->next);
	if(prev != NULL){
		prev->next = next;
	}else{
		prv->decrypt.head = next;
	}
	if(next != NULL){
		next->prev = prev;
	}else{
		prv->decrypt.tail = prev;
	}
	prv->decrypt.count -= 1;

	if(dec->ecm != NULL){
		dec->ecm->release(dec->ecm);
		dec->ecm = NULL;
	}

	if(dec->m2 != NULL){
		dec->m2->release(dec->m2);
		dec->m2 = NULL;
	}

	free(dec);
}

static DECRYPTOR_ELEM *select_active_decryptor(DECRYPTOR_ELEM *a, DECRYPTOR_ELEM *b, int32_t pid)
{
	if( b != NULL ){
		return b;
	}
	if( pid == 0x1fff ){
		return NULL;
	}
	return a;
}

static void bind_stream_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv, int32_t pid, DECRYPTOR_ELEM *dec)
{
	DECRYPTOR_ELEM *old;

	old = (DECRYPTOR_ELEM *)(prv->map[pid].target);
	if(old == dec){
		/* already binded - do nothing */
		return;
	}
		 
	if(old != NULL){
		old->ref -= 1;
		if(old->ref == 0){
			remove_decryptor(prv, old);
		}
		prv->map[pid].target = NULL;
	}

	if(dec != NULL){
		prv->map[pid].target = dec;
		dec->ref += 1;
	}
}

static void unlock_all_decryptor(ARIB_STD_B25_PRIVATE_DATA *prv)
{
	DECRYPTOR_ELEM *e;

	e = prv->decrypt.head;
	while(e != NULL){
		e->locked = 0;
		e = (DECRYPTOR_ELEM *)(e->next);
	}
}

static TS_STREAM_ELEM *get_stream_list_head(TS_STREAM_LIST *list)
{
	TS_STREAM_ELEM *r;

	r = list->head;
	if(r == NULL){
		return NULL;
	}

	list->head = (TS_STREAM_ELEM *)(r->next);
	if(list->head == NULL){
		list->tail = NULL;
		list->count = 0;
	}else{
		list->head->prev = NULL;
		list->count -= 1;
	}

	r->prev = NULL;
	r->next = NULL;

	return r;
}

static TS_STREAM_ELEM *find_stream_list_elem(TS_STREAM_LIST *list, int32_t pid)
{
	TS_STREAM_ELEM *r;

	r = list->head;
	while(r != NULL){
		if(r->pid == pid){
			break;
		}
		r = (TS_STREAM_ELEM *)(r->next);
	}

	return r;
}

static TS_STREAM_ELEM *create_stream_elem(int32_t pid, int32_t type)
{
	TS_STREAM_ELEM *r;

	r = (TS_STREAM_ELEM *)calloc(1, sizeof(TS_STREAM_ELEM));
	if(r == NULL){
		return NULL;
	}

	r->pid = pid;
	r->type = type;

	return r;
}

static void put_stream_list_tail(TS_STREAM_LIST *list, TS_STREAM_ELEM *elem)
{
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

static void clear_stream_list(TS_STREAM_LIST *list)
{
	TS_STREAM_ELEM *p,*n;

	p = list->head;
	while(p != NULL){
		n = (TS_STREAM_ELEM *)(p->next);
		free(p);
		p = n;
	}

	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

static int reserve_work_buffer(TS_WORK_BUFFER *buf, int32_t size)
{
	int m,n;
	uint8_t *p;
	
	if(buf->max >= size){
		return 1;
	}

	if(buf->max < 512){
		n = 512;
	}else{
		n = buf->max * 2;
	}
	
	while(n < size){
		n += n;
	}

	p = (uint8_t *)malloc(n);
	if(p == NULL){
		return 0;
	}

	m = 0;
	if(buf->pool != NULL){
		m = buf->tail - buf->head;
		if(m > 0){
			memcpy(p, buf->head, m);
		}
		free(buf->pool);
		buf->pool = NULL;
	}

	buf->pool = p;
	buf->head = p;
	buf->tail = p+m;
	buf->max = n;

	return 1;
}

static int append_work_buffer(TS_WORK_BUFFER *buf, uint8_t *data, int32_t size)
{
	int m;

	if(size < 1){
		/* ignore - do nothing */
		return 1;
	}

	m = buf->tail - buf->pool;

	if( (m+size) > buf->max ){
		if(!reserve_work_buffer(buf, m+size)){
			return 0;
		}
	}

	memcpy(buf->tail, data, size);
	buf->tail += size;

	return 1;
}

static void reset_work_buffer(TS_WORK_BUFFER *buf)
{
	buf->head = buf->pool;
	buf->tail = buf->pool;
}

static void release_work_buffer(TS_WORK_BUFFER *buf)
{
	if(buf->pool != NULL){
		free(buf->pool);
	}
	buf->pool = NULL;
	buf->head = NULL;
	buf->tail = NULL;
	buf->max = 0;
}

static void extract_ts_header(TS_HEADER *dst, uint8_t *src)
{
	dst->sync                         =  src[0];
	dst->transport_error_indicator    = (src[1] >> 7) & 0x01;
	dst->payload_unit_start_indicator = (src[1] >> 6) & 0x01;
	dst->transport_priority           = (src[1] >> 5) & 0x01;
	dst->pid = ((src[1] & 0x1f) << 8) | src[2];
	dst->transport_scrambling_control = (src[3] >> 6) & 0x03;
	dst->adaptation_field_control     = (src[3] >> 4) & 0x03;
	dst->continuity_counter           =  src[3]       & 0x0f;
}

static void extract_emm_fixed_part(EMM_FIXED_PART *dst, uint8_t *src)
{
	int i;

	dst->card_id = 0;
	for(i=0;i<6;i++){
		dst->card_id = (dst->card_id << 8) | src[i];
	}
	
	dst->associated_information_length = src[ 6];
	dst->protocol_number               = src[ 7];
	dst->broadcaster_group_id          = src[ 8];
	dst->update_number                 = (src[ 9]<<8)|src[10];
	dst->expiration_date               = (src[11]<<8)|src[12];
}

static uint8_t *resync(uint8_t *head, uint8_t *tail, int32_t unit_size)
{
	int i;
	unsigned char *buf;

	buf = head;
	tail -= unit_size * 8;
	while( buf <= tail ){
		if(buf[0] == 0x47){
			for(i=1;i<8;i++){
				if(buf[unit_size*i] != 0x47){
					break;
				}
			}
			if(i == 8){
				return buf;
			}
		}
		buf += 1;
	}

	return NULL;
}

static uint8_t *resync_force(uint8_t *head, uint8_t *tail, int32_t unit_size)
{
	int i,n;
	unsigned char *buf;

	buf = head;
	while( buf <= (tail-188) ){
		if(buf[0] == 0x47){
			n = (tail - buf) / unit_size;
			if(n == 0){
				return buf;
			}
			for(i=1;i<n;i++){
				if(buf[unit_size*i] != 0x47){
					break;
				}
			}
			if(i == n){
				return buf;
			}
		}
		buf += 1;
	}

	return NULL;
}
