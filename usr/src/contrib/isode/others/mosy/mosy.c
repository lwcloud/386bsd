/* mosy.c - Managed Object Syntax-compiler (yacc-based) */

#ifndef	lint
static char *rcsid = "$Header: /f/osi/others/mosy/RCS/mosy.c,v 7.6 91/03/09 11:54:19 mrose Exp $";
#endif

/*
 * $Header: /f/osi/others/mosy/RCS/mosy.c,v 7.6 91/03/09 11:54:19 mrose Exp $
 *
 *
 * $Log:	mosy.c,v $
 * Revision 7.6  91/03/09  11:54:19  mrose
 * update
 * 
 * Revision 7.5  91/02/22  09:28:42  mrose
 * Interim 6.8
 * 
 * Revision 7.4  90/11/20  15:33:41  mrose
 * update
 * 
 * Revision 7.3  90/09/07  17:33:59  mrose
 * new-macros
 * 
 * Revision 7.2  90/08/29  12:24:19  mrose
 * touch-up
 * 
 * Revision 7.1  90/07/01  21:04:36  mrose
 * pepsy
 * 
 * Revision 7.0  89/11/23  22:00:38  mrose
 * Release 6.0
 * 
 */

/*
 *				  NOTICE
 *
 *    Acquisition, use, and distribution of this module and related
 *    materials are subject to the restrictions of a license agreement.
 *    Consult the Preface in the User's Manual for the full terms of
 *    this agreement.
 *
 */


#include <ctype.h>
#include <stdio.h>
#include <varargs.h>
#include "mosy-defs.h"

/*    DATA */

int	Cflag = 0;		/* mosy */
int	dflag = 0;
int	Pflag = 0;		/* pepy compat... */
int	doexternals;
static int linepos = 0;
static int mflag = 0;
static int mosydebug = 0;
static int sflag = 0;

static  char *eval = NULLCP;

char   *mymodule = "";
OID	mymoduleid;

int yysection = NULL;
char *yyencpref = "none";
char *yydecpref = "none";
char *yyprfpref = "none";
char *yyencdflt = "none";
char *yydecdflt = "none";
char *yyprfdflt = "none";

static char *yymode = "";

static char *classes[] = {
    "UNIVERSAL ",
    "APPLICATION ",
    "",
    "PRIVATE "
};

static char autogen[BUFSIZ];

char *sysin = NULLCP;
static char sysout[BUFSIZ];

/*  */

typedef struct yot {
    char   *yo_name;

    YP	    yo_syntax;
    YV	    yo_value;

    char   *yo_access;
    char   *yo_status;

    char   *yo_descr;
    char   *yo_refer;
    YV	    yo_index;
    YV	    yo_defval;
}		yot, *OT;
#define	NULLOT	((OT) 0)

typedef struct yoi {
    char   *yi_name;

    YV	    yi_value;
}		yoi, *OI;
#define	NULLOI	((OI) 0)

/*  */

typedef struct symlist {
    char   *sy_encpref;
    char   *sy_decpref;
    char   *sy_prfpref;
    char   *sy_module;
    char   *sy_name;

    union {
	OT	sy_un_yo;

	OI	sy_un_yi;

	YP	sy_un_yp;
    }	sy_un;
#define	sy_yo	sy_un.sy_un_yo
#define	sy_yi	sy_un.sy_un_yi
#define	sy_yp	sy_un.sy_un_yp

    struct symlist *sy_next;
}		symlist, *SY;
#define	NULLSY	((SY) 0)

static	SY	myobjects = NULLSY;

static	SY	myidentifiers = NULLSY;

static	SY	mytypes = NULLSY;


char   *modsym ();
SY	new_symbol (), add_symbol ();

char   *id2str ();

YP	lookup_type ();
char   *val2str ();

/*    MAIN */

/* ARGSUSED */

main (argc, argv, envp)
int	argc;
char  **argv,
      **envp;
{
    register char  *cp,
		   *sp;

    fprintf (stderr, "%s\n", mosyversion);

    sysout[0] = NULL;
    for (argc--, argv++; argc > 0; argc--, argv++) {
	cp = *argv;

	if (strcmp (cp, "-d") == 0) {
	    dflag++;
	    continue;
	}
	if (strcmp (cp, "-m") == 0) {
	    mflag++;
	    continue;
	}
	if (strcmp (cp, "-o") == 0) {
	    if (sysout[0]) {
		fprintf (stderr, "too many output files\n");
		exit (1);
	    }
	    argc--, argv++;
	    if ((cp = *argv) == NULL || (*cp == '-' && cp[1] != NULL))
		goto usage;
	    (void) strcpy (sysout, cp);

	    continue;
	}
	if (strcmp (cp, "-s") == 0) {
	    sflag++;
	    continue;
	}

	if (sysin) {
usage: ;
	    fprintf (stderr,
		"usage: mosy [-d] [-o module.defs] [-s] module.my\n");
	    exit (1);
	}

	if (*cp == '-') {
	    if (*++cp != NULL)
		goto usage;
	    sysin = "";
	}
	sysin = cp;

	if (sysout[0])
	    continue;
	if (sp = rindex (cp, '/'))
	    sp++;
	if (sp == NULL || *sp == NULL)
	    sp = cp;
	sp += strlen (cp = sp) - 3;
	if (sp > cp && strcmp (sp, ".my") == 0)
	    (void) sprintf (sysout, "%.*s.defs", sp - cp, cp);
	else
	    (void) sprintf (sysout, "%s.defs", cp);
    }

    switch (mosydebug = (cp = getenv ("MOSYTEST")) && *cp ? atoi (cp) : 0) {
	case 2:
	    yydebug++;		/* fall */
	case 1:
	    sflag++;		/*   .. */
	case 0:
	    break;
    }

    if (sysin == NULLCP)
	sysin = "";

    if (*sysin && freopen (sysin, "r", stdin) == NULL) {
	fprintf (stderr, "unable to read "), perror (sysin);
	exit (1);
    }

    if (strcmp (sysout, "-") == 0)
	sysout[0] = NULL;
    if (*sysout && freopen (sysout, "w", stdout) == NULL) {
	fprintf (stderr, "unable to write "), perror (sysout);
	exit (1);
    }

    if (cp = index (mosyversion, ')'))
	for (cp++; *cp != ' '; cp++)
	    if (*cp == NULL) {
		cp = NULL;
		break;
	    }
    if (cp == NULL)
	cp = mosyversion + strlen (mosyversion);
    (void) sprintf (autogen, "%*.*s", 
	    cp - mosyversion, cp - mosyversion, mosyversion);
    printf ("-- automatically generated by %s, do not edit!\n\n", autogen);

    initoidtbl ();

    exit (yyparse ());		/* NOTREACHED */
}

/*    ERRORS */

yyerror (s)
register char   *s;
{
    yyerror_aux (s);

    if (*sysout)
	(void) unlink (sysout);

    exit (1);
}

#ifndef lint
warning (va_alist)
va_dcl
{
    char	buffer[BUFSIZ];
    char	buffer2[BUFSIZ];
    char	*cp;
    va_list	ap;

    va_start (ap);

    _asprintf (buffer, NULLCP, ap);

    va_end (ap);

    (void) sprintf (buffer2, "Warning: %s", buffer);
    yyerror_aux (buffer2);
}

#else

/* VARARGS1 */
warning (fmt)
char	*fmt;
{
    warning (fmt);
}
#endif

static	yyerror_aux (s)
register char   *s;
{
    if (linepos)
	fprintf (stderr, "\n"), linepos = 0;

    if (eval)
	fprintf (stderr, "%s %s: ", yymode, eval);
    else
	fprintf (stderr, "line %d: ", yylineno);
    fprintf (stderr, "%s\n", s);
    if (!eval)
	fprintf (stderr, "last token read was \"%s\"\n", yytext);
}

/*  */

#ifndef	lint
myyerror (va_alist)
va_dcl
{
    char    buffer[BUFSIZ];
    va_list ap;

    va_start (ap);

    _asprintf (buffer, NULLCP, ap);

    va_end (ap);

    yyerror (buffer);
}
#else
/* VARARGS */

myyerror (fmt)
char   *fmt;
{
    myyerror (fmt);
}
#endif

/*  */

yywrap () {
    if (linepos)
	fprintf (stderr, "\n"), linepos = 0;

    return 1;
}

/*  */

/* ARGSUSED */

yyprint (s, f, top)
char   *s;
int	f,
	top;
{
}

static  yyprint_aux (s, mode)
char   *s,
       *mode;
{
    int	    len;
    static int nameoutput = 0;
    static int outputlinelen = 79;

    if (sflag)
	return;

    if (strcmp (yymode, mode)) {
	if (linepos)
	    fprintf (stderr, "\n\n");

	fprintf (stderr, "%s", mymodule);
	nameoutput = (linepos = strlen (mymodule)) + 1;

	fprintf (stderr, " %ss", yymode = mode);
	linepos += strlen (yymode) + 1;
	fprintf (stderr, ":");
	linepos += 2;
    }

    len = strlen (s);
    if (linepos != nameoutput)
	if (len + linepos + 1 > outputlinelen)
	    fprintf (stderr, "\n%*s", linepos = nameoutput, "");
	else
	    fprintf (stderr, " "), linepos++;
    fprintf (stderr, "%s", s);
    linepos += len;
}

/*    PASS1 */

pass1 ()
{
    printf ("-- object definitions compiled from %s", mymodule);
    if (mymoduleid)
	printf (" %s", oidprint(mymoduleid));
    printf ("\n\n");
}

/*  */

pass1_oid (mod, id, value)
char   *mod,
       *id;
YV	value;
{
    register SY	    sy;
    register OI	    yi;

    if ((yi = (OI) calloc (1, sizeof *yi)) == NULLOI)
	yyerror ("out of memory");

    yi -> yi_name = id;
    yi -> yi_value = value;

    if (mosydebug) {
	if (linepos)
	    fprintf (stderr, "\n"), linepos = 0;

	fprintf (stderr, "%s.%s\n", mod ? mod : mymodule, id);
	print_yi (yi, 0);
	fprintf (stderr, "--------\n");
    }
    else
	yyprint_aux (id, "identifier");

    sy = new_symbol (NULLCP, NULLCP, NULLCP, mod, id);
    sy -> sy_yi = yi;
    myidentifiers = add_symbol (myidentifiers, sy);
}

/*  */

pass1_obj (mod, id, syntax, value, aname, sname, descr, refer, idx, defval)
char   *mod,
       *id,
       *aname,
       *sname,
       *descr,
       *refer;
YP	syntax;
YV	value,
    	idx,
    	defval;
{
    register SY	    sy;
    register OT	    yo;

    {
	register char *cp;

	for (cp = id; *cp; cp++)
	    if (*cp == '-') {
		warning ("object %s contains a `-' in its descriptor", id);
		break;
	    }

	if (strcmp (syntax, "Counter") == 0 && id[strlen (id) - 1] != 's')
	    warning ("descriptor of counter object %s doesn't end in `s'", id);
    }
    if (strcmp (aname, "read-only")
	    && strcmp (aname, "read-write")
	    && strcmp (aname, "read-write")
	    && strcmp (aname, "not-accessible"))
	warning ("value of ACCESS clause isn't a valid keyword");

    if (strcmp (sname, "mandatory")
	    && strcmp (sname, "optional")
	    && strcmp (sname, "obsolete")
	    && strcmp (sname, "deprecated"))
	warning ("value of STATUS clause isn't a valid keyword");

    if ((yo = (OT) calloc (1, sizeof *yo)) == NULLOT)
	yyerror ("out of memory");

    yo -> yo_name = id;
    yo -> yo_syntax = syntax;
    yo -> yo_value = value;
    yo -> yo_access = aname;
    yo -> yo_status = sname;
    yo -> yo_descr = descr;
    yo -> yo_refer = refer;
    yo -> yo_index = idx;
    yo -> yo_defval = defval;

    if (mosydebug) {
	if (linepos)
	    fprintf (stderr, "\n"), linepos = 0;

	fprintf (stderr, "%s.%s\n", mod ? mod : mymodule, id);
	print_yo (yo, 0);
	fprintf (stderr, "--------\n");
    }
    else
	yyprint_aux (id, "object");

    sy = new_symbol (NULLCP, NULLCP, NULLCP, mod, id);
    sy -> sy_yo = yo;
    myobjects = add_symbol (myobjects, sy);
}

/*  */

/* ARGSUSED */

pass1_trap (mod, id, enterprise, number, var, descr, refer)
char   *mod,
       *id;
YV	enterprise;
int	number;
YV	var;
char   *descr,
       *refer;
{
}

/*  */

pass1_type (encpref, decpref, prfpref, mod, id, yp)
register char  *encpref,
	       *decpref,
	       *prfpref,
	       *mod,
	       *id;
register YP	yp;
{
    register SY	    sy;

    if (dflag && lookup_type (mod, id))	/* no duplicate entries, please... */
	return;

    if (mosydebug) {
	if (linepos)
	    fprintf (stderr, "\n"), linepos = 0;

	fprintf (stderr, "%s.%s\n", mod ? mod : mymodule, id);
	print_type (yp, 0);
	fprintf (stderr, "--------\n");
    }
    else
	if (!(yp -> yp_flags & YP_IMPORTED))
	    yyprint_aux (id, "type");

    sy = new_symbol (encpref, decpref, prfpref, mod, id);
    sy -> sy_yp = yp;
    mytypes = add_symbol (mytypes, sy);
}

/*    PASS2 */

pass2 () {
    register SY	    sy;
    register YP	    yp;

    if (!sflag)
	(void) fflush (stderr);

    yymode = "identifier";
    for (sy = myidentifiers; sy; sy = sy -> sy_next) {
	if (sy -> sy_module == NULLCP)
	    yyerror ("no module name associated with symbol");

	do_id (sy -> sy_yi, eval = sy -> sy_name);
    }
    if (myidentifiers)
	printf ("\n");

    yymode = "object";
    for (sy = myobjects; sy; sy = sy -> sy_next) {
	if (sy -> sy_module == NULLCP)
	    yyerror ("no module name associated with symbol");

	do_obj1 (sy -> sy_yo, eval = sy -> sy_name);
    }
    if (myobjects)
	printf ("\n\n");

    (void) fflush (stdout);

    if (ferror (stdout))
	myyerror ("write error - %s", sys_errname (errno));
}

/*  */

/* ARGSUSED */

static	do_id (yi, id)
register OI	yi;
char   *id;
{
    printf ("%-20s %s\n", yi -> yi_name, id2str (yi -> yi_value));
}

/*  */

/* ARGSUSED */

static	do_obj1 (yo, id)
register OT	yo;
char   *id;
{
    register YP	    yp,
		    yz;

    printf ("%-20s %-16s ", yo -> yo_name, id2str (yo -> yo_value));

    if ((yp = yo -> yo_syntax) == NULLYP)
	yyerror ("no syntax associated with object type");

again: ;
    switch (yp -> yp_code) {
	case YP_INT:
	case YP_INTLIST:
	    id = "INTEGER";
	    break;

       case YP_OCT:
	    id = "OctetString";
	    break;

       case YP_OID:
	    id = "ObjectID";
	    break;

       case YP_NULL:
	    id = "NULL";
	    break;

	case YP_SEQTYPE:
	    if ((yz = yp -> yp_type) -> yp_code != YP_IDEFINED
		    || (yz = lookup_type (yz -> yp_module,
					  yz -> yp_identifier)) == NULL
		    || yz -> yp_code != YP_SEQLIST)
		warning ("value of SYNTAX clause isn't SEQUENCE OF Type,\n\twhere Type ::= SEQUENCE { ... }");
	    /* and fall... */
	case YP_SEQLIST:
	    id = "Aggregate";
	    if (strcmp (yo -> yo_access, "not-accessible"))
		warning ("value of ACCESS clause isn't not-accessible");
	    break;

	default:
	    id = "Invalid";
	    warning ("invalid value of SYNTAX clause");
	    break;

	case YP_IDEFINED:
	    if (yp = lookup_type (yp -> yp_module, id = yp -> yp_identifier))
		goto again;
	    break;
    }

    printf ("%-15s %-15s %s\n", id, yo -> yo_access, yo -> yo_status);
}

/*    IDENTIFIER HANDLING */

static char *id2str (yv)
register YV	yv;
{
    register char *cp,
		  *dp;
    static char buffer[BUFSIZ];

    if (yv -> yv_code != YV_OIDLIST)
	yyerror ("need an object identifer");
	
    cp = buffer;
    for (yv = yv -> yv_idlist, dp = ""; yv; yv = yv -> yv_next, dp = ".") {
	(void) sprintf (cp, "%s%s", dp, val2str (yv));
	cp += strlen (cp);
    }
    *cp = NULL;

    return buffer;
}

/*    TYPE HANDLING */

static YP  lookup_type (mod, id)
register char *mod,
	      *id;
{
    register SY	    sy;

    for (sy = mytypes; sy; sy = sy -> sy_next) {
	if (mod) {
	    if (strcmp (sy -> sy_module, mod))
		continue;
	}
	else
	    if (strcmp (sy -> sy_module, mymodule)
		    && strcmp (sy -> sy_module, "UNIV"))
		continue;

	if (strcmp (sy -> sy_name, id) == 0)
	    return sy -> sy_yp;
    }

    return NULLYP;
}

/*    VALUE HANDLING */

static char *val2str (yv)
register YV	yv;
{
    static char buffer[BUFSIZ];

    switch (yv -> yv_code) {
	case YV_BOOL:
	    yyerror ("need a sub-identifier, not a boolean");

	case YV_NUMBER:
	    (void) sprintf (buffer, "%d", yv -> yv_number);
	    return buffer;

	case YV_STRING:
	    yyerror ("need a sub-identifier, not a string");

	case YV_IDEFINED:
	    return yv -> yv_identifier;

	case YV_IDLIST:
	    yyerror ("haven't written symbol table for values yet");

	case YV_VALIST:
	    yyerror ("need a sub-identifier, not a list of values");

	case YV_OIDLIST:
	    yyerror ("need a sub-identifier, not an object identifier");

	case YV_NULL:
	    yyerror ("need a sub-identifier, not NULL");

	default:
	    myyerror ("unknown value: %d", yv -> yv_code);
    }
/* NOTREACHED */
}

/*  */

static int  val2int (yv)
register YV	yv;
{
    switch (yv -> yv_code) {
	case YV_BOOL:
	case YV_NUMBER:
	    return yv -> yv_number;

	case YV_STRING:
	    yyerror ("need an integer, not a string");

	case YV_IDEFINED:
	case YV_IDLIST:
	    yyerror ("haven't written symbol table for values yet");

	case YV_VALIST:
	    yyerror ("need an integer, not a list of values");

	case YV_OIDLIST:
	    yyerror ("need an integer, not an object identifier");

	case YV_NULL:
	    yyerror ("need an integer, not NULL");

	default:
	    myyerror ("unknown value: %d", yv -> yv_code);
    }
/* NOTREACHED */
}

/*  */

static	val2prf (yv, level)
register YV	yv;
int	level;
{
    register YV    y;

    if (yv -> yv_flags & YV_ID)
	printf ("%s ", yv -> yv_id);

#ifdef	notdef
    if (yv -> yv_flags & YV_TYPE)	/* will this REALLY work??? */
	do_type (yv -> yv_type, level, NULLCP);
#endif

    switch (yv -> yv_code) {
	case YV_BOOL: 
	    printf (yv -> yv_number ? "TRUE" : "FALSE");
	    break;

	case YV_NUMBER: 
	    if (yv -> yv_named)
		printf ("%s", yv -> yv_named);
	    else
		printf ("%d", yv -> yv_number);
	    break;

	case YV_STRING: 
	    printf ("\"%s\"", yv -> yv_string);
	    break;

	case YV_IDEFINED: 
	    if (yv -> yv_module)
		printf ("%s.", yv -> yv_module);
	    printf ("%s", yv -> yv_identifier);
	    break;

	case YV_IDLIST: 
	case YV_VALIST: 
	    printf ("{");
	    for (y = yv -> yv_idlist; y; y = y -> yv_next) {
		printf (" ");
		val2prf (y, level + 1);
		printf (y -> yv_next ? ", " : " ");
	    }
	    printf ("}");
	    break;

	case YV_OIDLIST: 
	    printf ("{");
	    for (y = yv -> yv_idlist; y; y = y -> yv_next) {
		printf (" ");
		val2prf (y, level + 1);
		printf (" ");
	    }
	    printf ("}");
	    break;

	case YV_NULL: 
	    printf ("NULL");
	    break;

	default: 
	    myyerror ("unknown value: %d", yv -> yv_code);
	/* NOTREACHED */
    }
}

/*    ACTION HANDLING */

static	act2prf (cp, level, e1, e2)
char   *cp,
       *e1,
       *e2;
int	level;
{
    register int    i,
                    j,
                    l4;
    register char  *dp,
                   *ep,
                   *fp;
    char   *gp;

    if (e1)
	printf (e1, level * 4, "");

    if (!(ep = index (dp = cp, '\n'))) {
	printf ("%s", dp);
	goto out;
    }

    for (;;) {
	i = expand (dp, ep, &gp);
	if (gp) {
	    if (i == 0)
		printf ("%*.*s\n", ep - dp, ep - dp, dp);
	    else
		break;
	}

	if (!(ep = index (dp = ep + 1, '\n'))) {
	    printf ("%s", dp);
	    return;
	}
    }


    printf ("\n");
    l4 = (level + 1) * 4;
    for (; *dp; dp = fp) {
	if (ep = index (dp, '\n'))
	    fp = ep + 1;
	else
	    fp = ep = dp + strlen (dp);

	j = expand (dp, ep, &gp);
	if (gp == NULL) {
	    if (*fp)
		printf ("\n");
	    continue;
	}

	if (j < i)
	    j = i;
	if (j)
	    printf ("%*s", l4 + j - i, "");
	printf ("%*.*s\n", ep - gp, ep - gp, gp);
    }

    printf ("%*s", level * 4, "");
out: ;
    if (e2)
	printf (e2, level * 4, "");
}


static	expand (dp, ep, gp)
register char  *dp,
	       *ep;
char  **gp;
{
    register int    i;

    *gp = NULL;
    for (i = 0; dp < ep; dp++) {
	switch (*dp) {
	    case ' ': 
		i++;
		continue;

	    case '\t': 
		i += 8 - (i % 8);
		continue;

	    default: 
		*gp = dp;
		break;
	}
	break;
    }

    return i;
}

/*    DEBUG */

static	print_yo (yo, level)
register OT	yo;
register int	level;
{
    if (yo == NULLOT)
	return;

    fprintf (stderr, "%*sname=%s\n", level * 4, "", yo -> yo_name);

    if (yo -> yo_syntax) {
	fprintf (stderr, "%*ssyntax\n", level * 4, "");
	print_type (yo -> yo_syntax, level + 1);
    }
    if (yo -> yo_value) {
	fprintf (stderr, "%*svalue\n", level * 4, "");
	print_value (yo -> yo_value, level + 1);
    }
}

/*  */

static	print_yi (yi, level)
register OI	yi;
register int	level;
{
    if (yi == NULLOI)
	return;

    fprintf (stderr, "%*sname=%s\n", level * 4, "", yi -> yi_name);

    if (yi -> yi_value) {
	fprintf (stderr, "%*svalue\n", level * 4, "");
	print_value (yi -> yi_value, level + 1);
    }
}

/*  */

static	print_type (yp, level)
register YP	yp;
register int	level;
{
    register YP	    y;
    register YV	    yv;

    if (yp == NULLYP)
	return;

    fprintf (stderr, "%*scode=0x%x flags=%s\n", level * 4, "",
	    yp -> yp_code, sprintb (yp -> yp_flags, YPBITS));
    fprintf (stderr,
	    "%*sintexp=\"%s\" strexp=\"%s\" prfexp=%c declexp=\"%s\" varexp=\"%s\"\n",
	    level * 4, "", yp -> yp_intexp, yp -> yp_strexp, yp -> yp_prfexp,
	    yp -> yp_declexp, yp -> yp_varexp);
    fprintf(stderr,
	    "%*sstructname=\"%s\" ptrname=\"%s\"\n", level * 4, "",
	    yp -> yp_structname, yp -> yp_ptrname);

    if (yp -> yp_action0)
	fprintf (stderr, "%*saction0 at line %d=\"%s\"\n", level * 4, "",
		yp -> yp_act0_lineno, yp -> yp_action0);
    if (yp -> yp_action1)
	fprintf (stderr, "%*saction1 at line %d=\"%s\"\n", level * 4, "",
		yp -> yp_act1_lineno, yp -> yp_action1);
    if (yp -> yp_action2)
	fprintf (stderr, "%*saction2 at line %d=\"%s\"\n", level * 4, "",
		yp -> yp_act2_lineno, yp -> yp_action2);
    if (yp -> yp_action3)
	fprintf (stderr, "%*saction3 at line %d=\"%s\"\n", level * 4, "",
		yp -> yp_act3_lineno, yp -> yp_action3);

    if (yp -> yp_flags & YP_TAG) {
	fprintf (stderr, "%*stag class=0x%x value=0x%x\n", level * 4, "",
		yp -> yp_tag -> yt_class, yp -> yp_tag -> yt_value);
	print_value (yp -> yp_tag -> yt_value, level + 1);
    }

    if (yp -> yp_flags & YP_DEFAULT) {
	fprintf (stderr, "%*sdefault=0x%x\n", level * 4, "", yp -> yp_default);
	print_value (yp -> yp_default, level + 1);
    }

    if (yp -> yp_flags & YP_ID)
	fprintf (stderr, "%*sid=\"%s\"\n", level * 4, "", yp -> yp_id);

    if (yp -> yp_flags & YP_BOUND)
	fprintf (stderr, "%*sbound=\"%s\"\n", level * 4, "", yp -> yp_bound);

    if (yp -> yp_offset)
	fprintf (stderr, "%*soffset=\"%s\"\n", level * 4, "", yp -> yp_offset);

    switch (yp -> yp_code) {
	case YP_INTLIST:
	case YP_BITLIST:
	    fprintf (stderr, "%*svalue=0x%x\n", level * 4, "", yp -> yp_value);
	    for (yv = yp -> yp_value; yv; yv = yv -> yv_next) {
		print_value (yv, level + 1);
		fprintf (stderr, "%*s----\n", (level + 1) * 4, "");
	    }
	    break;

	case YP_SEQTYPE:
	case YP_SEQLIST:
	case YP_SETTYPE:
	case YP_SETLIST:
	case YP_CHOICE:
	    fprintf (stderr, "%*stype=0x%x\n", level * 4, "", yp -> yp_type);
	    for (y = yp -> yp_type; y; y = y -> yp_next) {
		print_type (y, level + 1);
		fprintf (stderr, "%*s----\n", (level + 1) * 4, "");
	    }
	    break;

	case YP_IDEFINED:
	    fprintf (stderr, "%*smodule=\"%s\" identifier=\"%s\"\n",
		    level * 4, "", yp -> yp_module ? yp -> yp_module : "",
		    yp -> yp_identifier);
	    break;

	default:
	    break;
    }
}

/*  */

static	print_value (yv, level)
register YV	yv;
register int	level;
{
    register YV	    y;

    if (yv == NULLYV)
	return;

    fprintf (stderr, "%*scode=0x%x flags=%s\n", level * 4, "",
	    yv -> yv_code, sprintb (yv -> yv_flags, YVBITS));

    if (yv -> yv_action)
	fprintf (stderr, "%*saction at line %d=\"%s\"\n", level * 4, "",
		yv -> yv_act_lineno, yv -> yv_action);

    if (yv -> yv_flags & YV_ID)
	fprintf (stderr, "%*sid=\"%s\"\n", level * 4, "", yv -> yv_id);

    if (yv -> yv_flags & YV_NAMED)
	fprintf (stderr, "%*snamed=\"%s\"\n", level * 4, "", yv -> yv_named);

    if (yv -> yv_flags & YV_TYPE) {
	fprintf (stderr, "%*stype=0x%x\n", level * 4, "", yv -> yv_type);
	print_type (yv -> yv_type, level + 1);
    }

    switch (yv -> yv_code) {
	case YV_NUMBER:
	case YV_BOOL:
	    fprintf (stderr, "%*snumber=0x%x\n", level * 4, "",
		    yv -> yv_number);
	    break;

	case YV_STRING:
	    fprintf (stderr, "%*sstring=\"%s\"\n", level * 4, "",
		    yv -> yv_string);
	    break;

	case YV_IDEFINED:
	    if (yv -> yv_flags & YV_BOUND)
		fprintf (stderr, "%*smodule=\"%s\" identifier=\"%s\"\n",
			level * 4, "", yv -> yv_module, yv -> yv_identifier);
	    else
		fprintf (stderr, "%*sbound identifier=\"%s\"\n",
			level * 4, "", yv -> yv_identifier);
	    break;

	case YV_IDLIST:
	case YV_VALIST:
	case YV_OIDLIST:
	    for (y = yv -> yv_idlist; y; y = y -> yv_next) {
		print_value (y, level + 1);
		fprintf (stderr, "%*s----\n", (level + 1) * 4, "");
	    }
	    break;

	default:
	    break;
    }
}

/*    SYMBOLS */

static SY  new_symbol (encpref, decpref, prfpref, mod, id)
register char  *encpref,
	       *decpref,
	       *prfpref,
	       *mod,
	       *id;
{
    register SY    sy;

    if ((sy = (SY) calloc (1, sizeof *sy)) == NULLSY)
	yyerror ("out of memory");
    sy -> sy_encpref = encpref;
    sy -> sy_decpref = decpref;
    sy -> sy_prfpref = prfpref;
    sy -> sy_module = mod;
    sy -> sy_name = id;

    return sy;
}


static SY  add_symbol (s1, s2)
register SY	s1,
		s2;
{
    register SY	    sy;

    if (s1 == NULLSY)
	return s2;

    for (sy = s1; sy -> sy_next; sy = sy -> sy_next)
	continue;
    sy -> sy_next = s2;

    return s1;
}

/*    TYPES */

YP	new_type (code)
int	code;
{
    register YP    yp;

    if ((yp = (YP) calloc (1, sizeof *yp)) == NULLYP)
	yyerror ("out of memory");
    yp -> yp_code = code;

    return yp;
}


YP	add_type (y1, y2)
register YP	y1,
		y2;
{
    register YP	    yp;

    for (yp = y1; yp -> yp_next; yp = yp -> yp_next)
	continue;
    yp -> yp_next = y2;

    return y1;
}

/*    VALUES */

YV	new_value (code)
int	code;
{
    register YV    yv;

    if ((yv = (YV) calloc (1, sizeof *yv)) == NULLYV)
	yyerror ("out of memory");
    yv -> yv_code = code;

    return yv;
}


YV	add_value (y1, y2)
register YV	y1,
		y2;
{
    register YV	    yv;

    for (yv = y1; yv -> yv_next; yv = yv -> yv_next)
	continue;
    yv -> yv_next = y2;

    return y1;
}

/*    TAGS */

YT	new_tag (class)
PElementClass	class;
{
    register YT    yt;

    if ((yt = (YT) calloc (1, sizeof *yt)) == NULLYT)
	yyerror ("out of memory");
    yt -> yt_class = class;

    return yt;
}

/*    STRINGS */

char   *new_string (s)
register char  *s;
{
    register char  *p;

    if ((p = malloc ((unsigned) (strlen (s) + 1))) == NULLCP)
	yyerror ("out of memory");

    (void) strcpy (p, s);
    return p;
}

/*    SYMBOLS */

static struct triple {
    char	   *t_name;
    PElementClass   t_class;
    PElementID	    t_id;
}		triples[] = {
    "IA5String", PE_CLASS_UNIV,	PE_DEFN_IA5S,
    "ISO646String", PE_CLASS_UNIV, PE_DEFN_IA5S,
    "NumericString", PE_CLASS_UNIV, PE_DEFN_NUMS,
    "PrintableString", PE_CLASS_UNIV, PE_DEFN_PRTS,
    "T61String", PE_CLASS_UNIV, PE_DEFN_T61S,
    "TeletexString", PE_CLASS_UNIV, PE_DEFN_T61S,
    "VideotexString", PE_CLASS_UNIV, PE_DEFN_VTXS,
    "GeneralizedTime", PE_CLASS_UNIV, PE_DEFN_GENT,
    "GeneralisedTime", PE_CLASS_UNIV, PE_DEFN_GENT,
    "UTCTime", PE_CLASS_UNIV, PE_DEFN_UTCT,
    "UniversalTime", PE_CLASS_UNIV, PE_DEFN_UTCT,
    "GraphicString", PE_CLASS_UNIV, PE_DEFN_GFXS,
    "VisibleString", PE_CLASS_UNIV, PE_DEFN_VISS,
    "GeneralString", PE_CLASS_UNIV, PE_DEFN_GENS,
    "EXTERNAL", PE_CLASS_UNIV, PE_CONS_EXTN,
    "ObjectDescriptor", PE_CLASS_UNIV, PE_PRIM_ODE,

    NULL
};

/*  */

static char *modsym (module, id, prefix)
register char  *module,
	       *id;
char   *prefix;
{
    char    buf1[BUFSIZ],
            buf2[BUFSIZ],
            buf3[BUFSIZ];
    register struct triple *t;
    static char buffer[BUFSIZ];

    if (module == NULLCP)
	for (t = triples; t -> t_name; t++)
	    if (strcmp (t -> t_name, id) == 0) {
		module = "UNIV";
		break;
	    }

    if (prefix)
	modsym_aux (prefix, buf1);
    modsym_aux (module ? module : mymodule, buf2);
    modsym_aux (id, buf3);
    if (prefix)
	(void) sprintf (buffer, "%s_%s_%s", buf1, buf2, buf3);
    else
	(void) sprintf (buffer, "%s_%s", buf2, buf3);

    return buffer;
}


static	modsym_aux (name, bp)
register char  *name,
	       *bp;
{
    register char   c;

    while (c = *name++)
	switch (c) {
	    case '-':
		*bp++ = '_';
		*bp++ = '_';
		break;

	    default:
		*bp++ = c;
		break;
	}

    *bp = NULL;
}