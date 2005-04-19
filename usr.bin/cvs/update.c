/*	$OpenBSD: update.c,v 1.24 2005/04/19 00:38:39 joris Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "rcs.h"
#include "log.h"
#include "proto.h"


int cvs_update_file(CVSFILE *, void *);
int cvs_update_prune(CVSFILE *, void *);
int cvs_update_options(char *, int, char **, int *);
int cvs_update_sendflags(struct cvsroot *);

struct cvs_cmd_info cvs_update = {
	cvs_update_options,
	cvs_update_sendflags,
	cvs_update_file,
	NULL, NULL,
	CF_SORT | CF_RECURSE | CF_IGNORE | CF_KNOWN | CF_NOSYMS,
	CVS_REQ_UPDATE,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2 | CVS_CMD_SENDDIR
};

static int Pflag, dflag, Aflag;

int
cvs_update_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	Pflag = dflag = Aflag = 0;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'C':
		case 'D':
		case 'd':
			dflag = 1;
			break;
		case 'f':
			break;
		case 'l':
			cvs_update.file_flags &= ~CF_RECURSE;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'p':
		case 'Q':
		case 'q':
			break;
		case 'R':
			cvs_update.file_flags |= CF_RECURSE;
			break;
		case 'r':
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

int
cvs_update_sendflags(struct cvsroot *root)
{
	if (Pflag && cvs_sendarg(root, "-P", 0) < 0)
		return (CVS_EX_PROTO);
	if (Aflag && cvs_sendarg(root, "-a", 0) < 0)
		return (CVS_EX_PROTO);
	if (dflag && cvs_sendarg(root, "-d", 0) < 0)
		return (CVS_EX_PROTO);
	return (0);
}

/*
 * cvs_update_file()
 *
 * Update a single file.  In the case where we act as client, send any
 * pertinent information about that file to the server.
 */
int
cvs_update_file(CVSFILE *cf, void *arg)
{
	int ret, l;
	char *fname, *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);
	fname = CVS_FILE_NAME(cf);

	if (cf->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cf->cf_cvstat == CVS_FST_UNKNOWN)
				ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
				    CVS_FILE_NAME(cf));
			else
				ret = cvs_senddir(root, cf);
		}

		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_sendentry(root, cf) < 0) {
			return (CVS_EX_PROTO);
		}

		switch (cf->cf_cvstat) {
		case CVS_FST_UNKNOWN:
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, fname);
			break;
		case CVS_FST_UPTODATE:
			ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, fname);
			break;
		case CVS_FST_ADDED:
		case CVS_FST_MODIFIED:
			ret = cvs_sendreq(root, CVS_REQ_MODIFIED, fname);
			if (ret == 0)
				ret = cvs_sendfile(root, fpath);
			break;
		default:
			break;
		}
	} else {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_printf("? %s\n", fpath);
			return (0);
		}

		l = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, fname, RCS_FILE_EXT);
		if (l == -1 || l >= (int)sizeof(rcspath)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", rcspath);
			return (-1);
		}

		rf = rcs_open(rcspath, RCS_READ);
		if (rf == NULL) {
			return (CVS_EX_DATA);
		}

		rcs_close(rf);
	}

	return (ret);
}


/*
 * cvs_update_prune()
 *
 * Prune all directories which contain no more files known to CVS.
 */
int
cvs_update_prune(CVSFILE *cf, void *arg)
{

	return (0);
}
