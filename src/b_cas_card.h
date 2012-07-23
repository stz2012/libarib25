#ifndef B_CAS_CARD_H
#define B_CAS_CARD_H

#include "portable.h"

typedef struct {
	uint8_t  system_key[32];
	uint8_t  init_cbc[8];
	int64_t  bcas_card_id;
	int32_t  card_status;
	int32_t  ca_system_id;
} B_CAS_INIT_STATUS;

typedef struct {
	int64_t *data;
	int32_t  count;
} B_CAS_ID;

typedef struct {
	
	int32_t  s_yy; /* start date : year  */
	int32_t  s_mm; /* start date : month */
	int32_t  s_dd; /* start date : day   */
	
	int32_t  l_yy; /* limit date : year  */
	int32_t  l_mm; /* limit date : month */
	int32_t  l_dd; /* limit date : day   */

	int32_t  hold_time; /* in hour unit  */

	int32_t  broadcaster_group_id;
	
	int32_t  network_id;
	int32_t  transport_id;
	
} B_CAS_PWR_ON_CTRL;

typedef struct {
	B_CAS_PWR_ON_CTRL *data;
	int32_t  count;
} B_CAS_PWR_ON_CTRL_INFO;

typedef struct {
	uint8_t  scramble_key[16];
	uint32_t return_code;
} B_CAS_ECM_RESULT;

typedef struct {

	void *private_data;

	void (* release)(void *bcas);

	int (* init)(void *bcas);

	int (* get_init_status)(void *bcas, B_CAS_INIT_STATUS *stat);
	int (* get_id)(void *bcas, B_CAS_ID *dst);
	int (* get_pwr_on_ctrl)(void *bcas, B_CAS_PWR_ON_CTRL_INFO *dst);

	int (* proc_ecm)(void *bcas, B_CAS_ECM_RESULT *dst, uint8_t *src, int len);
	int (* proc_emm)(void *bcas, uint8_t *src, int len);
	
} B_CAS_CARD;

#ifdef __cplusplus
extern "C" {
#endif

extern B_CAS_CARD *create_b_cas_card();

#ifdef __cplusplus
}
#endif

#endif /* B_CAS_CARD_H */
