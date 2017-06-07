#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(WIN32)
	#include <io.h>
	#include <windows.h>
	#include <crtdbg.h>
#else
	#define __STDC_FORMAT_MACROS
	#include <inttypes.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif

#include "arib_std_b25.h"
#include "b_cas_card.h"

typedef struct {
	int32_t round;
	int32_t strip;
	int32_t emm;
	int32_t verbose;
	int32_t power_ctrl;
} OPTION;

static void show_usage();
static int parse_arg(OPTION *dst, int argc, char **argv);
static void test_arib_std_b25(const char *src, const char *dst, OPTION *opt);
static void show_bcas_power_on_control_info(B_CAS_CARD *bcas);

int main(int argc, char **argv)
{
	int n;
	OPTION opt;
	
	#if defined(WIN32)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_LEAK_CHECK_DF);
	#endif

	n = parse_arg(&opt, argc, argv);
	if(n+2 > argc){
		show_usage();
		exit(EXIT_FAILURE);
	}

	for(;n<=(argc-2);n+=2){
		test_arib_std_b25(argv[n+0], argv[n+1], &opt);
	}
	
	#if defined(WIN32)
	_CrtDumpMemoryLeaks();
	#endif

	return EXIT_SUCCESS;
}

static void show_usage()
{
	fprintf(stderr, "b25 - ARIB STD-B25 test program version %s (%s)\n", ARIB25_VERSION_STRING, BUILD_GIT_REVISION);
	fprintf(stderr, "  built with %s %s on %s\n", BUILD_CC_NAME, BUILD_CC_VERSION, BUILD_OS_NAME);
	fprintf(stderr, "usage: b25 [options] src.m2t dst.m2t [more pair ..]\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -r round (integer, default=4)\n");
	fprintf(stderr, "  -s strip\n");
	fprintf(stderr, "     0: keep null(padding) stream (default)\n");
	fprintf(stderr, "     1: strip null stream\n");
	fprintf(stderr, "  -m EMM\n");
	fprintf(stderr, "     0: ignore EMM (default)\n");
	fprintf(stderr, "     1: send EMM to B-CAS card\n");
	fprintf(stderr, "  -p power_on_control_info\n");
	fprintf(stderr, "     0: do nothing additionaly\n");
	fprintf(stderr, "     1: show B-CAS EMM receiving request (default)\n");
	fprintf(stderr, "  -v verbose\n");
	fprintf(stderr, "     0: silent\n");
	fprintf(stderr, "     1: show processing status (default)\n");
	fprintf(stderr, "\n");
}

static int parse_arg(OPTION *dst, int argc, char **argv)
{
	int i;
	
	dst->round = 4;
	dst->strip = 0;
	dst->emm = 0;
	dst->power_ctrl = 1;
	dst->verbose = 1;

	for(i=1;i<argc;i++){
		if(argv[i][0] != '-'){
			break;
		}
		switch(argv[i][1]){
		case 'm':
			if(argv[i][2]){
				dst->emm = atoi(argv[i]+2);
			}else{
				dst->emm = atoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'p':
			if(argv[i][2]){
				dst->power_ctrl = atoi(argv[i]+2);
			}else{
				dst->power_ctrl = atoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'r':
			if(argv[i][2]){
				dst->round = atoi(argv[i]+2);
			}else{
				dst->round = atoi(argv[i+1]);
				i += 1;
			}
			break;
		case 's':
			if(argv[i][2]){
				dst->strip = atoi(argv[i]+2);
			}else{
				dst->strip = atoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'v':
			if(argv[i][2]){
				dst->verbose = atoi(argv[i]+2);
			}else{
				dst->verbose = atoi(argv[i+1]);
				i += 1;
			}
			break;
		default:
			fprintf(stderr, "error - unknown option '-%c'\n", argv[i][1]);
			return argc;
		}
	}

	return i;
}

static void test_arib_std_b25(const char *src, const char *dst, OPTION *opt)
{
	int code,i,n,m;
	int sfd,dfd;

	int64_t total;
	int64_t offset;
#if defined(WIN32)
	unsigned long tick,tock;
#else
	struct timeval tick,tock;
	double millisec;
#endif
	double mbps;

	ARIB_STD_B25 *b25;
	B_CAS_CARD   *bcas;

	ARIB_STD_B25_PROGRAM_INFO pgrm;

	uint8_t data[64*1024];

	ARIB_STD_B25_BUFFER sbuf;
	ARIB_STD_B25_BUFFER dbuf;

	sfd = -1;
	dfd = -1;
	b25 = NULL;
	bcas = NULL;

	sfd = _open(src, _O_BINARY|_O_RDONLY|_O_SEQUENTIAL);
	if(sfd < 0){
		fprintf(stderr, "error - failed on _open(%s) [src]\n", src);
		goto LAST;
	}
	
	_lseeki64(sfd, 0, SEEK_END);
	total = _telli64(sfd);
	_lseeki64(sfd, 0, SEEK_SET);

	b25 = create_arib_std_b25();
	if(b25 == NULL){
		fprintf(stderr, "error - failed on create_arib_std_b25()\n");
		goto LAST;
	}

	code = b25->set_multi2_round(b25, opt->round);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_multi2_round() : code=%d\n", code);
		goto LAST;
	}

	code = b25->set_strip(b25, opt->strip);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_strip() : code=%d\n", code);
		goto LAST;
	}

	code = b25->set_emm_proc(b25, opt->emm);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_emm_proc() : code=%d\n", code);
		goto LAST;
	}

	bcas = create_b_cas_card();
	if(bcas == NULL){
		fprintf(stderr, "error - failed on create_b_cas_card()\n");
		goto LAST;
	}

	code = bcas->init(bcas);
	if(code < 0){
		fprintf(stderr, "error - failed on B_CAS_CARD::init() : code=%d\n", code);
		goto LAST;
	}

	code = b25->set_b_cas_card(b25, bcas);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_b_cas_card() : code=%d\n", code);
		goto LAST;
	}

	dfd = _open(dst, _O_BINARY|_O_WRONLY|_O_SEQUENTIAL|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE);
	if(dfd < 0){
		fprintf(stderr, "error - failed on _open(%s) [dst]\n", dst);
		goto LAST;
	}

	offset = 0;
#if defined(WIN32)
	tock = GetTickCount();
#else
	gettimeofday(&tock, NULL);
#endif
	while( (n = _read(sfd, data, sizeof(data))) > 0 ){
		sbuf.data = data;
		sbuf.size = n;

		code = b25->put(b25, &sbuf);
		if(code < 0){
			fprintf(stderr, "error - failed on ARIB_STD_B25::put() : code=%d\n", code);
			goto LAST;
		}

		code = b25->get(b25, &dbuf);
		if(code < 0){
			fprintf(stderr, "error - failed on ARIB_STD_B25::get() : code=%d\n", code);
			goto LAST;
		}

		if(dbuf.size > 0){
			n = _write(dfd, dbuf.data, dbuf.size);
			if(n != dbuf.size){
				fprintf(stderr, "error failed on _write(%d)\n", dbuf.size);
				goto LAST;
			}
		}
		
		offset += sbuf.size;
		if(opt->verbose != 0){
			m = (int)(10000*offset/total);
			mbps = 0.0;
#if defined(WIN32)
			tick = GetTickCount();
			if (tick-tock > 100) {
				mbps = offset;
				mbps /= 1024;
				mbps /= (tick-tock);
			}
#else
			gettimeofday(&tick, NULL);
			millisec = (tick.tv_sec - tock.tv_sec) * 1000;
			millisec += (tick.tv_usec - tock.tv_usec) / 1000;
			if(millisec > 100.0) {
				mbps = offset;
				mbps /= 1024;
				mbps /= millisec;
			}
#endif
			fprintf(stderr, "\rprocessing: %2d.%02d%% [%6.2f MB/sec]", m/100, m%100, mbps);
		}
	}

	code = b25->flush(b25);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::flush() : code=%d\n", code);
		goto LAST;
	}
	
	code = b25->get(b25, &dbuf);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::get() : code=%d\n", code);
		goto LAST;
	}

	if(dbuf.size > 0){
		n = _write(dfd, dbuf.data, dbuf.size);
		if(n != dbuf.size){
			fprintf(stderr, "error - failed on _write(%d)\n", dbuf.size);
			goto LAST;
		}
	}

	if(opt->verbose != 0){
		mbps = 0.0;
#if defined(WIN32)
		tick = GetTickCount();
		if (tick-tock > 100) {
			mbps = offset;
			mbps /= 1024;
			mbps /= (tick-tock);
		}
#else
		gettimeofday(&tick, NULL);
		millisec = (tick.tv_sec - tock.tv_sec) * 1000;
		millisec += (tick.tv_usec - tock.tv_usec) / 1000;
		if(millisec > 100.0) {
			mbps = offset;
			mbps /= 1024;
			mbps /= millisec;
		}
#endif
		fprintf(stderr, "\rprocessing: finish  [%6.2f MB/sec]\n", mbps);
		fflush(stderr);
		fflush(stdout);
	}

	n = b25->get_program_count(b25);
	if(n < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::get_program_count() : code=%d\n", code);
		goto LAST;
	}
	for(i=0;i<n;i++){
		code = b25->get_program_info(b25, &pgrm, i);
		if(code < 0){
			fprintf(stderr, "error - failed on ARIB_STD_B25::get_program_info(%d) : code=%d\n", i, code);
			goto LAST;
		}
		if(pgrm.ecm_unpurchased_count > 0){
			fprintf(stderr, "warning - unpurchased ECM is detected\n");
			fprintf(stderr, "  channel:               %d\n", pgrm.program_number);
			fprintf(stderr, "  unpurchased ECM count: %d\n", pgrm.ecm_unpurchased_count);
			fprintf(stderr, "  last ECM error code:   %04x\n", pgrm.last_ecm_error_code);
			#if defined(WIN32)
			fprintf(stderr, "  undecrypted TS packet: %I64d\n", pgrm.undecrypted_packet_count);
			fprintf(stderr, "  total TS packet:       %I64d\n", pgrm.total_packet_count);
			#else
			fprintf(stderr, "  undecrypted TS packet: %"PRId64"\n", pgrm.undecrypted_packet_count);
			fprintf(stderr, "  total TS packet:       %"PRId64"\n", pgrm.total_packet_count);
			#endif
		}
	}

	if(opt->power_ctrl != 0){
		show_bcas_power_on_control_info(bcas);
	}

LAST:

	if(sfd >= 0){
		_close(sfd);
		sfd = -1;
	}

	if(dfd >= 0){
		_close(dfd);
		dfd = -1;
	}

	if(b25 != NULL){
		b25->release(b25);
		b25 = NULL;
	}

	if(bcas != NULL){
		bcas->release(bcas);
		bcas = NULL;
	}
}

static void show_bcas_power_on_control_info(B_CAS_CARD *bcas)
{
	int code;
	int i,w;
	B_CAS_PWR_ON_CTRL_INFO pwc;

	code = bcas->get_pwr_on_ctrl(bcas, &pwc);
	if(code < 0){
		fprintf(stderr, "error - failed on B_CAS_CARD::get_pwr_on_ctrl() : code=%d\n", code);
		return;
	}

	if(pwc.count == 0){
		fprintf(stdout, "no EMM receiving request\n");
		return;
	}

	fprintf(stdout, "total %d EMM receiving request\n", pwc.count);
	for(i=0;i<pwc.count;i++){
		fprintf(stdout, "+ [%d] : tune ", i);
		switch(pwc.data[i].network_id){
		case 4:
			w = pwc.data[i].transport_id;
			fprintf(stdout, "BS-%d/TS-%d ", ((w >> 4) & 0x1f), (w & 7));
			break;
		case 6:
		case 7:
			w = pwc.data[i].transport_id;
			fprintf(stdout, "ND-%d/TS-%d ", ((w >> 4) & 0x1f), (w & 7));
			break;
		default:
			fprintf(stdout, "unknown(b:0x%02x,n:0x%04x,t:0x%04x) ", pwc.data[i].broadcaster_group_id, pwc.data[i].network_id, pwc.data[i].transport_id);
			break;
		}
		fprintf(stdout, "between %04d %02d/%02d ", pwc.data[i].s_yy, pwc.data[i].s_mm, pwc.data[i].s_dd);
		fprintf(stdout, "to %04d %02d/%02d ", pwc.data[i].l_yy, pwc.data[i].l_mm, pwc.data[i].l_dd);
		fprintf(stdout, "least %d hours\n", pwc.data[i].hold_time);
	}
}

