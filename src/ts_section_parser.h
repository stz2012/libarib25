#ifndef TS_SECTION_PARSER_H
#define TS_SECTION_PARSER_H

#include "ts_common_types.h"

typedef struct {
	int64_t total;      /* total received section count      */
	int64_t unique;     /* unique section count              */
	int64_t error;      /* crc and other error section count */
} TS_SECTION_PARSER_STAT;

typedef struct {

	void *private_data;

	void (* release)(void *parser);

	int (* reset)(void *parser);

	int (* put)(void *parser, TS_HEADER *hdr, uint8_t *data, int size);
	int (* get)(void *parser, TS_SECTION *sect);
	int (* ret)(void *parser, TS_SECTION *sect);

	int (* get_count)(void *parser);
	
	int (* get_stat)(void *parser, TS_SECTION_PARSER_STAT *stat);
	
} TS_SECTION_PARSER;

#ifdef __cplusplus
extern "C" {
#endif

extern TS_SECTION_PARSER *create_ts_section_parser();

#ifdef __cplusplus
}
#endif

#endif /* TS_SECTION_PARSER_H */
