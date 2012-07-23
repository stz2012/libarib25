#ifndef TS_COMMON_TYPES_H
#define TS_COMMON_TYPES_H

#include "portable.h"

typedef struct {
	int32_t           sync;                           /*  0- 7 :  8 bits */
	int32_t           transport_error_indicator;      /*  8- 8 :  1 bit  */
	int32_t           payload_unit_start_indicator;   /*  9- 9 :  1 bit  */
	int32_t           transport_priority;             /* 10-10 :  1 bits */
	int32_t           pid;                            /* 11-23 : 13 bits */
	int32_t           transport_scrambling_control;   /* 24-25 :  2 bits */
	int32_t           adaptation_field_control;       /* 26-27 :  2 bits */
	int32_t           continuity_counter;             /* 28-31 :  4 bits */
} TS_HEADER;

typedef struct {
	int32_t           table_id;                       /*  0- 7 :  8 bits */
	int32_t           section_syntax_indicator;       /*  8- 8 :  1 bit  */
	int32_t           private_indicator;              /*  9- 9 :  1 bit  */
	int32_t           section_length;                 /* 12-23 : 12 bits */
	int32_t           table_id_extension;             /* 24-39 : 16 bits */
	int32_t           version_number;                 /* 42-46 :  5 bits */
	int32_t           current_next_indicator;         /* 47-57 :  1 bit  */
	int32_t           section_number;                 /* 48-55 :  8 bits */
	int32_t           last_section_number;            /* 56-63 :  8 bits */
} TS_SECTION_HEADER;

typedef struct {
	TS_SECTION_HEADER  hdr;
	uint8_t           *raw;
	uint8_t           *data;
	uint8_t           *tail;
} TS_SECTION;

#endif /* TS_COMMON_TYPES_H */