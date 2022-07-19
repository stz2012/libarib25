#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "getopt.h"

#if !defined(_WIN32)
#define TCHAR char
#define _T(X) X
#define _ftprintf fprintf
#define _tcslen strlen
#endif

int getopt(int, TCHAR * const[], const TCHAR *);

TCHAR *optarg;
int optind=1, opterr=1, optopt, __optpos, optreset=0;

#define optpos __optpos

static void __getopt_msg(const TCHAR *a, const TCHAR *b, const TCHAR *c, size_t l)
{
	_ftprintf(stderr, _T("%s%s%s\n"), a, b, c);
}

int getopt(int argc, TCHAR * const argv[], const TCHAR *optstring)
{
	int i, c, d;
	int k, l;
	TCHAR *optchar;

	if (!optind || optreset) {
		optreset = 0;
		__optpos = 0;
		optind = 1;
	}

	if (optind >= argc || !argv[optind])
		return -1;

	if (argv[optind][0] != '-') {
		if (optstring[0] == '-') {
			optarg = argv[optind++];
			return 1;
		}
		return -1;
	}

	if (!argv[optind][1])
		return -1;

	if (argv[optind][1] == '-' && !argv[optind][2])
		return optind++, -1;

	if (!optpos) optpos++;
	c = argv[optind][optpos], k = 1;
	optchar = argv[optind]+optpos;
	optopt = c;
	optpos += k;

	if (!argv[optind][optpos]) {
		optind++;
		optpos = 0;
	}

	if (optstring[0] == '-' || optstring[0] == '+')
		optstring++;

	i = 0;
	d = 0;
	do {
		d = optstring[i], l = 1;
		if (l>0) i+=l; else i++;
	} while (l && d != c);

	if (d != c) {
		if (optstring[0] != ':' && opterr)
			__getopt_msg(argv[0], _T(": unrecognized option: "), optchar, k);
		return '?';
	}
	if (optstring[i] == ':') {
		if (optstring[i+1] == ':') optarg = 0;
		else if (optind >= argc) {
			if (optstring[0] == ':') return ':';
			if (opterr) __getopt_msg(argv[0],
				_T(": option requires an argument: "),
				optchar, k);
			return '?';
		}
		if (optstring[i+1] != ':' || optpos) {
			optarg = argv[optind++] + optpos;
			optpos = 0;
		}
	}
	return c;
}

static void permute(TCHAR *const *argv, int dest, int src)
{
	char **av = (char **)argv;
	char *tmp = av[src];
	int i;
	for (i=src; i>dest; i--)
		av[i] = av[i-1];
	av[dest] = tmp;
}

int __getopt_long_core(int argc, TCHAR *const *argv, const TCHAR *optstring, const struct option *longopts, int *idx, int longonly)
{
	optarg = 0;
	if (longopts && argv[optind][0] == '-' &&
		((longonly && argv[optind][1] && argv[optind][1] != '-') ||
		 (argv[optind][1] == '-' && argv[optind][2])))
	{
		int colon = optstring[optstring[0]=='+'||optstring[0]=='-']==':';
		int i, cnt, match;
		TCHAR *opt;
		for (cnt=i=0; longopts[i].name; i++) {
			const TCHAR *name = longopts[i].name;
			opt = argv[optind]+1;
			if (*opt == '-') opt++;
			for (; *name && *name == *opt; name++, opt++);
			if (*opt && *opt != '=') continue;
			match = i;
			if (!*name) {
				cnt = 1;
				break;
			}
			cnt++;
		}
		if (cnt==1) {
			i = match;
			optind++;
			optopt = longopts[i].val;
			if (*opt == '=') {
				if (!longopts[i].has_arg) {
					if (colon || !opterr)
						return '?';
					__getopt_msg(argv[0],
						_T(": option does not take an argument: "),
						longopts[i].name,
						_tcslen(longopts[i].name) * sizeof (TCHAR));
					return '?';
				}
				optarg = opt+1;
			} else if (longopts[i].has_arg == required_argument) {
				if (!(optarg = argv[optind])) {
					if (colon) return ':';
					if (!opterr) return '?';
					__getopt_msg(argv[0],
						_T(": option requires an argument: "),
						longopts[i].name,
						_tcslen(longopts[i].name) * sizeof (TCHAR));
					return '?';
				}
				optind++;
			}
			if (idx) *idx = i;
			if (longopts[i].flag) {
				*longopts[i].flag = longopts[i].val;
				return 0;
			}
			return longopts[i].val;
		}
		if (argv[optind][1] == '-') {
			if (!colon && opterr)
				__getopt_msg(argv[0], cnt ?
					_T(": option is ambiguous: ") :
					_T(": unrecognized option: "),
					argv[optind]+2,
					_tcslen(argv[optind]+2) * sizeof (TCHAR));
			optind++;
			return '?';
		}
	}
	return getopt(argc, argv, optstring);
}

static int __getopt_long(int argc, TCHAR *const *argv, const TCHAR *optstring, const struct option *longopts, int *idx, int longonly)
{
	int ret, skipped, resumed;
	if (!optind || optreset) {
		optreset = 0;
		__optpos = 0;
		optind = 1;
	}
	if (optind >= argc || !argv[optind]) return -1;
	skipped = optind;
	if (optstring[0] != '+' && optstring[0] != '-') {
		int i;
		for (i=optind; ; i++) {
			if (i >= argc || !argv[i]) return -1;
			if (argv[i][0] == '-' && argv[i][1]) break;
		}
		optind = i;
	}
	resumed = optind;
	ret = __getopt_long_core(argc, argv, optstring, longopts, idx, longonly);
	if (resumed > skipped) {
		int i, cnt = optind-resumed;
		for (i=0; i<cnt; i++)
			permute(argv, skipped, optind-1);
		optind = skipped + cnt;
	}
	return ret;
}

int getopt_long(int argc, TCHAR *const *argv, const TCHAR *optstring, const struct option *longopts, int *idx)
{
	return __getopt_long(argc, argv, optstring, longopts, idx, 0);
}

int getopt_long_only(int argc, TCHAR *const *argv, const TCHAR *optstring, const struct option *longopts, int *idx)
{
	return __getopt_long(argc, argv, optstring, longopts, idx, 1);
}
