/*
 * Copyright (c) 1985
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "validator-internal.h"

#include "res_support.h"
#include "res_comp.h"
#include "res_debug.h"

const char     *_libsres_opcodes[] = {
    "QUERY",
    "IQUERY",
    "CQUERYM",
    "CQUERYU",                  /* experimental */
    "NOTIFY",                   /* experimental */
    "UPDATE",
    "6",
    "7",
    "8",
    "9",
    "10",
    "11",
    "12",
    "13",
    "ZONEINIT",
    "ZONEREF",
};



/*
 * some system headers are outdated
 */
#ifndef NS_ALG_DH
#define NS_ALG_DH               2       /* Diffie Hellman KEY */
#define NS_ALG_DSA              3       /* DSA KEY */
#define NS_ALG_DSS              NS_ALG_DSA
#endif

#ifndef HAVE_NS_CERT_TYPES
typedef enum __ns_cert_types {
    cert_t_pkix = 1,            /* PKIX (X.509v3) */
    cert_t_spki = 2,            /* SPKI */
    cert_t_pgp = 3,             /* PGP */
    cert_t_url = 253,           /* URL private type */
    cert_t_oid = 254            /* OID private type */
} ns_cert_types;
#endif

/*
 * some system prototypes are missing
 */
#if defined HAVE_DECL_P_RCODE && !HAVE_DECL_P_RCODE
const char     *p_rcode(int);
#endif


extern const char *_res_sectioncodes[];

#define ERRBUFLEN 80

static void
_print_or_log(void *file, int level, const char *template, ...)
{
    va_list         ap;

    va_start(ap, template);

    if (NULL == file)
        res_log_ap((val_context_t*)NULL, level, template, ap);
    else
        vfprintf(file, template, ap);

    va_end(ap);
}

static void
do_section(ns_msg * handle, ns_sect section, int pflag, FILE * file)
{
    int             n, rrnum;
    const static int buflen = 2048;
    char           *buf;
    ns_opcode       opcode;
    ns_rr           rr;
#ifdef HAVE_STRERROR_R
    char            err_buf[ERRBUFLEN + 1];
#endif

    buf = (char *)MALLOC(buflen);
    if (buf == NULL) {
        res_log(NULL,LOG_DEBUG, ";; memory allocation failure\n");
        return;
    }

    opcode = (ns_opcode) libsres_msg_getflag(*handle, ns_f_opcode);
    rrnum = 0;
    for (;;) {
        if (ns_parserr(handle, section, rrnum, &rr)) {
            if (errno != ENODEV) {
#ifdef HAVE_STRERROR_R
                if (!strerror_r(errno, err_buf, ERRBUFLEN))
                    _print_or_log(file, LOG_DEBUG, ";; ns_parserr: %s\n", err_buf);
                else
                    _print_or_log(file, LOG_DEBUG, ";; ns_parserr: Error\n");
#else
                _print_or_log(file, LOG_DEBUG, ";; ns_parserr: %s\n", strerror(errno));
#endif
            }
            goto cleanup;
        }
        if (section == ns_s_qd)
            _print_or_log(file, LOG_DEBUG, ";;\t%s, type = %s, class = %s\n",
                    ns_rr_name(rr),
                    p_type(ns_rr_type(rr)), p_class(ns_rr_class(rr)));
        else if (section == ns_s_ar && ns_rr_type(rr) == ns_t_opt) {
            u_int32_t       ttl = ns_rr_ttl(rr);
            _print_or_log(file, LOG_DEBUG,
                    "; EDNS: version: %u, udp=%u, flags=%04x\n",
                    (ttl >> 16) & 0xff, ns_rr_class(rr), ttl & 0xffff);
        } else {
            n = ns_sprintrr(handle, &rr, NULL, NULL, buf, buflen);
            if (n < 0) {
                if (errno == ENOSPC) {
                    FREE(buf);
                    buf = NULL;
#if 0
                    if (buflen < 131072)
                        buf = MALLOC(buflen += 1024);
#endif
                    if (buf == NULL) {
                        _print_or_log(file, LOG_DEBUG, ";; memory allocation failure\n");
                        return;
                    }
                    continue;
                }
#ifdef HAVE_STRERROR_R
                if (!strerror_r(errno, err_buf, ERRBUFLEN))
                    _print_or_log(file, LOG_DEBUG, ";; ns_sprintrr: %s\n", err_buf);
                else
                    _print_or_log(file, LOG_DEBUG, ";; ns_sprintrr: Error\n");
#else
                _print_or_log(file, LOG_DEBUG, ";; ns_sprintrr: %s\n", strerror(errno));
#endif
                goto cleanup;
            }
            _print_or_log(file, LOG_DEBUG, "%s\n", buf);
        }
        rrnum++;
    }
  cleanup:
    if (buf != NULL)
        FREE(buf);
}



const u_char   *
p_cdnname(const u_char * cp, const u_char * msg, int len, FILE * file)
{
    char            name[NS_MAXDNAME];
    int             n;

    if ((n = dn_expand(msg, msg + len, cp, name, sizeof(name))) < 0)
        return (NULL);
    if (name[0] == '\0')
        _print_or_log(file, LOG_DEBUG, "%c", '.');
    else
        _print_or_log(file, LOG_DEBUG, "%s", name);
    return (cp + n);
}

const u_char   *
p_cdname(const u_char * cp, const u_char * msg, FILE * file)
{
    return (p_cdnname(cp, msg, NS_PACKETSZ, file));
}

/*
 * Return a fully-qualified domain name from a compressed name (with
 * length supplied).  
 */

const u_char   *
p_fqnname(const u_char *cp, const u_char *msg, int msglen, char *name, int namelen) 
{
    int             n, newlen;

    if ((n = dn_expand(msg, cp + msglen, cp, name, namelen)) < 0)
        return (NULL);
    newlen = strlen(name);
    if (newlen == 0 || name[newlen - 1] != '.') {
        if (newlen + 1 >= namelen)      /* Lack space for final dot */
            return (NULL);
        else
            strcpy(name + newlen, ".");
    }
    return (cp + n);
}

/*
 * XXX: the rest of these functions need to become length-limited, too. 
 */

const u_char   *
p_fqname(const u_char * cp, const u_char * msg, FILE * file)
{
    char            name[NS_MAXDNAME];
    const u_char   *n;

    n = p_fqnname(cp, msg, NS_MAXCDNAME, name, sizeof(name));
    if (n == NULL)
        return (NULL);
    _print_or_log(file, LOG_DEBUG, "%s", name);
    return (n);
}

/*
 * the original res_sym struct used regular, non-const, char* pointers.
 * this causes a slew of warning about initialization discarding qualifiers,
 * so this is the same structure but with const char* pointers.
 * On BSD, the definition from resolv.h cannot be included safely with the current
 * configure script, so we use this internal type.
 *
 * Unfortuantely, on OS X, __p_class_sym is a macro, and exported with the
 * original res_sym type. So in this case, we don't use this hack.
 *
 * And, for once, solaris has a better header than the rest, and has const
 * char ptrs in res_sym.
 */
#if (defined(__APPLE__) || defined(sun) || defined(__CYGWIN__) || defined(linux)) && !defined(ANDROID)
#define RES_SYM_TYPE res_sym
#else
struct res_sym_const {
    int             number;     /* Identifying number, like T_MX */
    const char     *name;       /* Its symbolic name, like "MX" */
    const char     *humanname;  /* Its fun name, like "mail exchanger" */
};
#define RES_SYM_TYPE res_sym_const
#endif

/*
 * Names of RR classes and qclasses.  Classes and qclasses are the same, except
 * that ns_c_any is a qclass but not a class.  (You can ask for records of class
 * ns_c_any, but you can't have any records of that class in the database.)
 */
const struct RES_SYM_TYPE __p_res_class_syms[] = {
    {ns_c_in, "IN", (char *) 0},
    {ns_c_chaos, "CH", (char *) 0},
    {ns_c_chaos, "CHAOS", (char *) 0},
    {ns_c_hs, "HS", (char *) 0},
    {ns_c_hs, "HESIOD", (char *) 0},
    {ns_c_any, "ANY", (char *) 0},
    {ns_c_none, "NONE", (char *) 0},
    {ns_c_in, (char *) 0, (char *) 0}
};

/*
 * Names of message sections.
 */
const struct RES_SYM_TYPE __p_default_section_syms[] = {
    {ns_s_qd, "QUERY", (char *) 0},
    {ns_s_an, "ANSWER", (char *) 0},
    {ns_s_ns, "AUTHORITY", (char *) 0},
    {ns_s_ar, "ADDITIONAL", (char *) 0},
    {0, (char *) 0, (char *) 0}
};

const struct RES_SYM_TYPE __p_update_section_syms[] = {
    {ns_s_zn, "ZONE", (char *) 0},
    {ns_s_pr, "PREREQUISITE", (char *) 0},
    {ns_s_ud, "UPDATE", (char *) 0},
    {ns_s_ar, "ADDITIONAL", (char *) 0},
    {0, (char *) 0, (char *) 0}
};

const struct RES_SYM_TYPE __p_key_syms[] = {
    {NS_ALG_MD5RSA, "RSA", "RSA KEY with MD5 hash"},
    {NS_ALG_DH, "DH", "Diffie Hellman"},
    {NS_ALG_DSA, "DSA", "Digital Signature Algorithm"},
    {NS_ALG_EXPIRE_ONLY, "EXPIREONLY", "No algorithm"},
    {NS_ALG_PRIVATE_OID, "PRIVATE", "Algorithm obtained from OID"},
    {0, NULL, NULL}
};

const struct RES_SYM_TYPE __p_cert_syms[] = {
    {cert_t_pkix, "PKIX", "PKIX (X.509v3) Certificate"},
    {cert_t_spki, "SPKI", "SPKI certificate"},
    {cert_t_pgp, "PGP", "PGP certificate"},
    {cert_t_url, "URL", "URL Private"},
    {cert_t_oid, "OID", "OID Private"},
    {0, NULL, NULL}
};

/*
 * Names of RR types and qtypes.  Types and qtypes are the same, except
 * that T_ANY is a qtype but not a type.  (You can ask for records of type
 * T_ANY, but you can't have any records of that type in the database.)
 */
const struct RES_SYM_TYPE __p_type_sres_syms[] = {
    {ns_t_a, "A", "address"},
    {ns_t_ns, "NS", "name server"},
    {ns_t_md, "MD", "mail destination (deprecated)"},
    {ns_t_mf, "MF", "mail forwarder (deprecated)"},
    {ns_t_cname, "CNAME", "canonical name"},
    {ns_t_soa, "SOA", "start of authority"},
    {ns_t_mb, "MB", "mailbox"},
    {ns_t_mg, "MG", "mail group member"},
    {ns_t_mr, "MR", "mail rename"},
    {ns_t_null, "NULL", "null"},
    {ns_t_wks, "WKS", "well-known service (deprecated)"},
    {ns_t_ptr, "PTR", "domain name pointer"},
    {ns_t_hinfo, "HINFO", "host information"},
    {ns_t_minfo, "MINFO", "mailbox information"},
    {ns_t_mx, "MX", "mail exchanger"},
    {ns_t_txt, "TXT", "text"},
    {ns_t_rp, "RP", "responsible person"},
    {ns_t_afsdb, "AFSDB", "DCE or AFS server"},
    {ns_t_x25, "X25", "X25 address"},
    {ns_t_isdn, "ISDN", "ISDN address"},
    {ns_t_rt, "RT", "router"},
    {ns_t_nsap, "NSAP", "nsap address"},
    {ns_t_nsap_ptr, "NSAP_PTR", "domain name pointer"},
    {ns_t_sig, "SIG", "signature"},
    {ns_t_key, "KEY", "key"},
    {ns_t_px, "PX", "mapping information"},
    {ns_t_gpos, "GPOS", "geographical position (withdrawn)"},
    {ns_t_aaaa, "AAAA", "IPv6 address"},
    {ns_t_loc, "LOC", "location"},
    {ns_t_nxt, "NXT", "next valid name (unimplemented)"},
    {ns_t_eid, "EID", "endpoint identifier (unimplemented)"},
    {ns_t_nimloc, "NIMLOC", "NIMROD locator (unimplemented)"},
    {ns_t_srv, "SRV", "server selection"},
    {ns_t_atma, "ATMA", "ATM address (unimplemented)"},
    {ns_t_naptr, "NAPTR", "URN Naming Authority"},
    {ns_t_kx, "KX", "Key Exchange"},
    {ns_t_cert, "CERT", "Certificate"},
    {ns_t_a6, "A6", "IPv6 Address"},
    {ns_t_dname, "DNAME", "dname"},
    {ns_t_sink, "SINK", "Kitchen Sink (experimental)"},
    {ns_t_opt, "OPT", "EDNS Options"},
    //      {ns_t_tkey,     "TKEY",         "tkey"},
    {ns_t_tsig, "TSIG", "transaction signature"},
    {ns_t_ixfr, "IXFR", "incremental zone transfer"},
    {ns_t_axfr, "AXFR", "zone transfer"},
    {ns_t_mailb, "MAILB", "mailbox-related data (deprecated)"},
    {ns_t_maila, "MAILA", "mail agent (deprecated)"},
    {ns_t_any, "ANY", "\"any\""},
    {ns_t_zxfr, "ZXFR", "compressed zone transfer"},
    {ns_t_dnskey, "DNSKEY", "DNS keys"},
    {ns_t_rrsig, "RRSIG", "signatures"},
    {ns_t_nsec, "NSEC", "next record"},
#ifdef LIBVAL_NSEC3
    {ns_t_nsec3, "NSEC3", "NSEC3 next record"},
#endif
#ifdef LIBVAL_DLV
    {ns_t_dlv, "DLV", "DLV record"},
#endif
    {ns_t_ds, "DS", "delegation signer"},
    {0, NULL, NULL}
};


/*
 * Names of DNS rcodes.
 */
const struct RES_SYM_TYPE __p_rcode_syms[] = {
    {ns_r_noerror, "NOERROR", "no error"},
    {ns_r_formerr, "FORMERR", "format error"},
    {ns_r_servfail, "SERVFAIL", "server failed"},
    {ns_r_nxdomain, "NXDOMAIN", "no such domain name"},
    {ns_r_notimpl, "NOTIMP", "not implemented"},
    {ns_r_refused, "REFUSED", "refused"},
    {ns_r_yxdomain, "YXDOMAIN", "domain name exists"},
    {ns_r_yxrrset, "YXRRSET", "rrset exists"},
    {ns_r_nxrrset, "NXRRSET", "rrset doesn't exist"},
    {ns_r_notauth, "NOTAUTH", "not authoritative"},
    {ns_r_notzone, "NOTZONE", "Not in zone"},
    {ns_r_max, "", ""},
    {ns_r_badsig, "BADSIG", "bad signature"},
    {ns_r_badkey, "BADKEY", "bad key"},
    {ns_r_badtime, "BADTIME", "bad time"},
    {0, NULL, NULL}
};

int
sym_ston(const struct RES_SYM_TYPE *syms, const char *name, int *success)
{
    size_t namelen = 0;
    if (name)
        namelen = strlen(name);

    for ((void) NULL; syms->name != 0; syms++) {
        if ((namelen == strlen(syms->name)) &&
            strncasecmp(name, syms->name, namelen) == 0) {
            if (success)
                *success = 1;
            return (syms->number);
        }
    }
    if (success)
        *success = 0;
    return (syms->number);      /* The default value. */
}

const char     *
sym_ntos(const struct RES_SYM_TYPE *syms, int number, int *success)
{
    static char     unname[20];
    
    if (success)
      *success = 0;
    for ((void) NULL; syms->name != 0; syms++) {
        if (number == syms->number) {
            if (success)
                *success = 1;
            return (syms->name);
        }
    }

    snprintf(unname, sizeof(unname), "%d", number);     /* XXX nonreentrant */
    if (success)
        *success = 0;
    return (unname);
}

const char     *
sym_ntop(const struct RES_SYM_TYPE *syms, int number, int *success)
{
    static char     unname[20];

    for ((void) NULL; syms->name != 0; syms++) {
        if (number == syms->number) {
            if (success)
                *success = 1;
            return (syms->humanname);
        }
    }
    snprintf(unname, sizeof(unname), "%d", number);     /* XXX nonreentrant */
    if (success)
        *success = 0;
    return (unname);
}

/*
 * Return a string for the type.
 */
const char     *
p_sres_type(int type)
{
    int             success;
    const char     *result;
    static char     typebuf[20];

    result =
        sym_ntos((const struct RES_SYM_TYPE *) __p_type_sres_syms, type, &success);

    if (success)
        return (result);
    if (type < 0 || type > 0xfff)
        return ("BADTYPE");
    snprintf(typebuf, sizeof(typebuf), "TYPE%d", type);
    return (typebuf);
}

/*
 * Return a string for the type.
 */
const char     *
p_section(int section, int opcode)
{
    const struct RES_SYM_TYPE *symbols;

    switch (opcode) {
    case ns_o_update:
        symbols = (const struct RES_SYM_TYPE *) __p_update_section_syms;
        break;
    default:
        symbols = (const struct RES_SYM_TYPE *) __p_default_section_syms;
        break;
    }
    return (sym_ntos(symbols, section, (int *) 0));
}

/*
 * Return a mnemonic for class.
 */
const char     *
p_class(int class_h)
{
    int             success;
    const char     *result;
    static char     classbuf[20];

    result =
        sym_ntos((const struct RES_SYM_TYPE *) __p_res_class_syms, class_h, &success);
    if (success)
        return (result);
    if (class_h < 0 || class_h > 0xfff)
        return ("BADCLASS");
    snprintf(classbuf, sizeof(classbuf), "CLASS%d", class_h);
    return (classbuf);
}

/*
 * Return a mnemonic for an option
 */
const char     *
p_option(P_OPTION_ARG_TYPE option)
{
    static char     nbuf[40];

    switch (option) {
    case SR_QUERY_SET_DO:
        return "DO-bit";
    case SR_QUERY_SET_CD:
        return "CD-bit";
    case SR_QUERY_RECURSE:
        return "recurs";
    case SR_QUERY_DEBUG:
        return "debug";
        /*
         * XXX nonreentrant 
         */
    default:
        snprintf(nbuf, sizeof(nbuf), "?0x%lx?", (u_long) option);
        return (nbuf);
    }
}

/*
 * Return a mnemonic for a time to live.
 */
const char     *
p_time(u_int32_t value)
{
    static char     nbuf[40];   /* XXX nonreentrant */

    if (ns_format_ttl(value, nbuf, sizeof(nbuf)) < 0)
        snprintf(nbuf, sizeof(nbuf), "%u", value);
    return (nbuf);
}

/*
 * Return a string for the rcode.
 */
const char     *
p_rcode(int rcode)
{
    return (sym_ntos
            ((const struct RES_SYM_TYPE *) __p_rcode_syms, rcode, (int *) 0));
}

/*
 * Return a string for a res_sockaddr_union.
 */
/*
 * const char *
 * p_sockun(union res_sockaddr_union u, char *buf, size_t size) {
 * char ret[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:123.123.123.123")];
 * 
 * switch (u.sin.sin_family) {
 * case AF_INET:
 * inet_ntop(AF_INET, &u.sin.sin_addr, ret, sizeof(ret));
 * break;
 * #ifdef HAS_INET6_STRUCTS
 * case AF_INET6:
 * inet_ntop(AF_INET6, &u.sin6.sin6_addr, ret, sizeof(ret));
 * break;
 * #endif
 * default:
 * snprintf(ret, sizeof(ret), "[af%d]", u.sin.sin_family);
 * break;
 * }
 * if (size > 0U) {
 * strncpy(buf, ret, size - 1);
 * buf[size - 1] = '0';
 * }
 * return (buf);
 * }
 */

/*
 * routines to convert between on-the-wire RR format and zone file format.
 * Does not contain conversion to/from decimal degrees; divide or multiply
 * by 60*60*1000 for that.
 */

static const unsigned int poweroften[10] =
    { 1, 10, 100, 1000, 10000, 100000,
    1000000, 10000000, 100000000, 1000000000
};

/*
 * takes an XeY precision/size value, returns a string representation. 
 */
static const char *
precsize_ntoa(u_char prec)
{
    static char     retbuf[sizeof("90000000.00")];      /* XXX nonreentrant */
    unsigned long   val;
    int             mantissa, exponent;

    mantissa = (int) ((prec >> 4) & 0x0f) % 10;
    exponent = (int) ((prec >> 0) & 0x0f) % 10;

    val = mantissa * poweroften[exponent];

    (void) snprintf(retbuf, sizeof(retbuf), "%lu.%.2lu", val / 100,
                    val % 100);
    return (retbuf);
}

/*
 * converts ascii size/precision X * 10**Y(cm) to 0xXY.  moves pointer. 
 */
static          u_char
precsize_aton(const char **strptr)
{
    unsigned int    mval = 0, cmval = 0;
    u_char        retval = 0;
    const char     *cp;
    int             exponent;
    int             mantissa;

    cp = *strptr;

    while (isdigit((unsigned char) *cp))
        mval = mval * 10 + (*cp++ - '0');

    if (*cp == '.') {           /* centimeters */
        cp++;
        if (isdigit((unsigned char) *cp)) {
            cmval = (*cp++ - '0') * 10;
            if (isdigit((unsigned char) *cp)) {
                cmval += (*cp++ - '0');
            }
        }
    }
    cmval = (mval * 100) + cmval;

    for (exponent = 0; exponent < 9; exponent++)
        if (cmval < poweroften[exponent + 1])
            break;

    mantissa = cmval / poweroften[exponent];
    if (mantissa > 9)
        mantissa = 9;

    retval = (mantissa << 4) | exponent;

    *strptr = cp;

    return (retval);
}

/*
 * converts ascii lat/lon to unsigned encoded 32-bit number.  moves pointer. 
 */
static          u_int32_t
latlon2ul(const char **latlonstrptr, int *which)
{
    const char     *cp;
    u_int32_t       retval;
    int             deg = 0, min = 0, secs = 0, secsfrac = 0;

    cp = *latlonstrptr;

    while (isdigit((unsigned char) *cp))
        deg = deg * 10 + (*cp++ - '0');

    while (isspace((unsigned char) *cp))
        cp++;

    if (!(isdigit((unsigned char) *cp)))
        goto fndhemi;

    while (isdigit((unsigned char) *cp))
        min = min * 10 + (*cp++ - '0');

    while (isspace((unsigned char) *cp))
        cp++;

    if (!(isdigit((unsigned char) *cp)))
        goto fndhemi;

    while (isdigit((unsigned char) *cp))
        secs = secs * 10 + (*cp++ - '0');

    if (*cp == '.') {           /* decimal seconds */
        cp++;
        if (isdigit((unsigned char) *cp)) {
            secsfrac = (*cp++ - '0') * 100;
            if (isdigit((unsigned char) *cp)) {
                secsfrac += (*cp++ - '0') * 10;
                if (isdigit((unsigned char) *cp)) {
                    secsfrac += (*cp++ - '0');
                }
            }
        }
    }

    while (!isspace((unsigned char) *cp))       /* if any trailing garbage */
        cp++;

    while (isspace((unsigned char) *cp))
        cp++;

  fndhemi:
    switch (*cp) {
    case 'N':
    case 'n':
    case 'E':
    case 'e':
        retval = ((unsigned) 1 << 31)
            + (((((deg * 60) + min) * 60) + secs) * 1000)
            + secsfrac;
        break;
    case 'S':
    case 's':
    case 'W':
    case 'w':
        retval = ((unsigned) 1 << 31)
            - (((((deg * 60) + min) * 60) + secs) * 1000)
            - secsfrac;
        break;
    default:
        retval = 0;             /* invalid value -- indicates error */
        break;
    }

    switch (*cp) {
    case 'N':
    case 'n':
    case 'S':
    case 's':
        *which = 1;             /* latitude */
        break;
    case 'E':
    case 'e':
    case 'W':
    case 'w':
        *which = 2;             /* longitude */
        break;
    default:
        *which = 0;             /* error */
        break;
    }

    cp++;                       /* skip the hemisphere */

    while (!isspace((unsigned char) *cp))       /* if any trailing garbage */
        cp++;

    while (isspace((unsigned char) *cp))        /* move to next field */
        cp++;

    *latlonstrptr = cp;

    return (retval);
}

/*
 * converts a zone file representation in a string to an RDATA on-the-wire
 * * representation. 
 */
int
loc_aton(const char *ascii, u_char *binary)
{
    const char     *cp, *maxcp;
    u_char         *bcp;

    u_int32_t       latit = 0, longit = 0, alt = 0;
    u_int32_t       lltemp1 = 0, lltemp2 = 0;
    int             altmeters = 0, altfrac = 0, altsign = 1;
    u_char          hp = 0x16;  /* default = 1e6 cm = 10000.00m = 10km */
    u_char          vp = 0x13;  /* default = 1e3 cm = 10.00m */
    u_char          siz = 0x12; /* default = 1e2 cm = 1.00m */
    int             which1 = 0, which2 = 0;

    cp = ascii;
    maxcp = cp + strlen(ascii);

    lltemp1 = latlon2ul(&cp, &which1);

    lltemp2 = latlon2ul(&cp, &which2);

    switch (which1 + which2) {
    case 3:                    /* 1 + 2, the only valid combination */
        if ((which1 == 1) && (which2 == 2)) {   /* normal case */
            latit = lltemp1;
            longit = lltemp2;
        } else if ((which1 == 2) && (which2 == 1)) {    /* reversed */
            longit = lltemp1;
            latit = lltemp2;
        } else {                /* some kind of brokenness */
            return (0);
        }
        break;
    default:                   /* we didn't get one of each */
        return (0);
    }

    /*
     * altitude 
     */
    if (*cp == '-') {
        altsign = -1;
        cp++;
    }

    if (*cp == '+')
        cp++;

    while (isdigit((unsigned char) *cp))
        altmeters = altmeters * 10 + (*cp++ - '0');

    if (*cp == '.') {           /* decimal meters */
        cp++;
        if (isdigit((unsigned char) *cp)) {
            altfrac = (*cp++ - '0') * 10;
            if (isdigit((unsigned char) *cp)) {
                altfrac += (*cp++ - '0');
            }
        }
    }

    alt = (10000000 + (altsign * (altmeters * 100 + altfrac)));

    while (!isspace((unsigned char) *cp) && (cp < maxcp))       /* if trailing garbage or m */
        cp++;

    while (isspace((unsigned char) *cp) && (cp < maxcp))
        cp++;

    if (cp >= maxcp)
        goto defaults;

    siz = precsize_aton(&cp);

    while (!isspace((unsigned char) *cp) && (cp < maxcp))       /* if trailing garbage or m */
        cp++;

    while (isspace((unsigned char) *cp) && (cp < maxcp))
        cp++;

    if (cp >= maxcp)
        goto defaults;

    hp = precsize_aton(&cp);

    while (!isspace((unsigned char) *cp) && (cp < maxcp))       /* if trailing garbage or m */
        cp++;

    while (isspace((unsigned char) *cp) && (cp < maxcp))
        cp++;

    if (cp >= maxcp)
        goto defaults;

    vp = precsize_aton(&cp);

  defaults:

    bcp = binary;
    *bcp++ = (u_char) 0;      /* version byte */
    *bcp++ = siz;
    *bcp++ = hp;
    *bcp++ = vp;
    NS_PUT32(latit, bcp);
    NS_PUT32(longit, bcp);
    NS_PUT32(alt, bcp);

    return (16);                /* size of RR in octets */
}

/*
 * takes an on-the-wire LOC RR and formats it in a human readable format. 
 */
const char     *
loc_ntoa(const u_char *binary, char *ascii)
{
    static const char *error = "?";
    static char     tmpbuf[sizeof
                           "1000 60 60.000 N 1000 60 60.000 W -12345678.00m 90000000.00m 90000000.00m 90000000.00m"];
    const u_char   *cp = binary;

    int             latdeg, latmin, latsec, latsecfrac;
    int             longdeg, longmin, longsec, longsecfrac;
    char            northsouth, eastwest;
    const char     *altsign;
    int             altmeters, altfrac;

    const u_int32_t referencealt = 100000 * 100;

    u_int32_t       latval, longval, altval;
    u_int32_t       templ;
    u_char          sizeval, hpval, vpval, versionval;

    char           *sizestr, *hpstr, *vpstr;

    versionval = *cp++;

    if (ascii == NULL)
        ascii = tmpbuf;

    if (versionval) {
        (void) SPRINTF((ascii, "; error: unknown LOC RR version"));
        return (ascii);
    }

    sizeval = *cp++;

    hpval = *cp++;
    vpval = *cp++;

    RES_GET32(templ, cp);
    latval = (templ - ((unsigned) 1 << 31));

    RES_GET32(templ, cp);
    longval = (templ - ((unsigned) 1 << 31));

    RES_GET32(templ, cp);
    if (templ < referencealt) { /* below WGS 84 spheroid */
        altval = referencealt - templ;
        altsign = "-";
    } else {
        altval = templ - referencealt;
        altsign = "";
    }

    if (latval < 0) {
        northsouth = 'S';
        latval = -latval;
    } else
        northsouth = 'N';

    latsecfrac = latval % 1000;
    latval = latval / 1000;
    latsec = latval % 60;
    latval = latval / 60;
    latmin = latval % 60;
    latval = latval / 60;
    latdeg = latval;

    if (longval < 0) {
        eastwest = 'W';
        longval = -longval;
    } else
        eastwest = 'E';

    longsecfrac = longval % 1000;
    longval = longval / 1000;
    longsec = longval % 60;
    longval = longval / 60;
    longmin = longval % 60;
    longval = longval / 60;
    longdeg = longval;

    altfrac = altval % 100;
    altmeters = (altval / 100);

    sizestr = strdup(precsize_ntoa(sizeval));
    hpstr = strdup(precsize_ntoa(hpval));
    vpstr = strdup(precsize_ntoa(vpval));

    SPRINTF((ascii,
            "%d %.2d %.2d.%.3d %c %d %.2d %.2d.%.3d %c %s%d.%.2dm %sm %sm %sm",
            latdeg, latmin, latsec, latsecfrac, northsouth,
            longdeg, longmin, longsec, longsecfrac, eastwest,
            altsign, altmeters, altfrac,
            (sizestr != NULL) ? sizestr : error,
            (hpstr != NULL) ? hpstr : error,
            (vpstr != NULL) ? vpstr : error));

    if (sizestr != NULL)
        FREE(sizestr);
    if (hpstr != NULL)
        FREE(hpstr);
    if (vpstr != NULL)
        FREE(vpstr);

    return (ascii);
}


/*
 * Return the number of DNS hierarchy levels in the name. 
 */
int
dn_count_labels(const char *name)
{
    int             i, len, count;

    len = strlen(name);
    for (i = 0, count = 0; i < len; i++) {
        /*
         * XXX need to check for \. or use named's nlabels(). 
         */
        if (name[i] == '.')
            count++;
    }

    /*
     * don't count initial wildcard 
     */
    if (name[0] == '*')
        if (count)
            count--;

    /*
     * don't count the null label for root. 
     * if terminating '.' not found, must adjust 
     * count to include last label 
     */
    if (len > 0 && name[len - 1] != '.')
        count++;
    return (count);
}

/*
 * Make dates expressed in seconds-since-Jan-1-1970 easy to read.  
 * SIG records are required to be printed like this, by the Secure DNS RFC.
 */
char           *
p_secstodate(P_SECSTODATE_ARG_TYPE secs)
{
    /*
     * XXX nonreentrant 
     */
    static char     output[15]; /* YYYYMMDDHHMMSS and null */
    time_t          clock = secs;
    struct tm      *time;

#ifdef HAVE_GMTIME_R
    struct tm       res;
    time = gmtime_r(&clock, &res);
#else
    time = gmtime(&clock);
#endif
    time->tm_year += 1900;
    time->tm_mon += 1;
    snprintf(output, sizeof(output), "%04d%02d%02d%02d%02d%02d",
             time->tm_year, time->tm_mon, time->tm_mday,
             time->tm_hour, time->tm_min, time->tm_sec);
    return (output);
}

u_int16_t
res_nametoclass(const char *buf, int *successp)
{
    unsigned long   result;
    char           *endptr;
    int             success;

    result =
        sym_ston((const struct RES_SYM_TYPE *) __p_res_class_syms, buf, &success);
    if (success)
        goto done;

    if (strncasecmp(buf, "CLASS", 5) != 0 ||
        !isdigit((unsigned char) buf[5]))
        goto done;
    errno = 0;
    result = strtoul(buf + 5, &endptr, 10);
    if (errno == 0 && *endptr == '\0' && result <= 0xffffU)
        success = 1;
  done:
    if (successp)
        *successp = success;
    return (result);
}

u_int16_t
res_nametotype(const char *buf, int *successp)
{
    unsigned long   result;
    char           *endptr;
    int             success;

    result =
        sym_ston((const struct RES_SYM_TYPE *) __p_type_sres_syms, buf, &success);
    if (success)
        goto done;

    if (strncasecmp(buf, "type", 4) != 0 ||
        !isdigit((unsigned char) buf[4]))
        goto done;
    errno = 0;
    result = strtoul(buf + 4, &endptr, 10);
    if (errno == 0 && *endptr == '\0' && result <= 0xffffU)
        success = 1;
  done:
    if (successp)
        *successp = success;
    return (result);
}

/*
 * Print the contents of a query.
 * This is intended to be primarily a debugging routine.
 */
void
libsres_pquery(const u_char * msg, size_t len, FILE * file)
{
    ns_msg          handle;
    int             qdcount, ancount, nscount, arcount;
    u_int           opcode, rcode, id;
#ifdef HAVE_STRERROR_R
    char            err_buf[ERRBUFLEN + 1];
#endif
    char            buf[128];

    if (ns_initparse(msg, len, &handle) < 0) {
#ifdef HAVE_STRERROR_R
        if (!strerror_r(errno, err_buf, ERRBUFLEN))
            _print_or_log(file, LOG_DEBUG, ";; ns_initparse: %s\n", err_buf);
        else
            _print_or_log(file, LOG_DEBUG, ";; ns_initparse: Error\n");
#else
        _print_or_log(file, LOG_DEBUG, ";; ns_initparse: %s\n", strerror(errno));
#endif

        return;
    }
    opcode = libsres_msg_getflag(handle, ns_f_opcode);
    rcode = libsres_msg_getflag(handle, ns_f_rcode);
    id = ns_msg_id(handle);
    qdcount = ns_msg_count(handle, ns_s_qd);
    ancount = ns_msg_count(handle, ns_s_an);
    nscount = ns_msg_count(handle, ns_s_ns);
    arcount = ns_msg_count(handle, ns_s_ar);

    /*
     * Print header fields.
     */
    _print_or_log(file, LOG_DEBUG,
            ";; ->>HEADER<<- opcode: %s, status: %s, id: %d\n",
            _libsres_opcodes[opcode], p_rcode(rcode), id);

    strcpy(buf, ";; flags:");
    if (libsres_msg_getflag(handle, ns_f_qr))
        strcat(buf, " qr");
    if (libsres_msg_getflag(handle, ns_f_aa))
        strcat(buf, " aa");
    if (libsres_msg_getflag(handle, ns_f_tc))
        strcat(buf, " tc");
    if (libsres_msg_getflag(handle, ns_f_rd))
        strcat(buf, " rd");
    if (libsres_msg_getflag(handle, ns_f_ra))
        strcat(buf, " ra");
    if (libsres_msg_getflag(handle, ns_f_z))
        strcat(buf, " ??");
    if (libsres_msg_getflag(handle, ns_f_ad))
        strcat(buf, " ad");
    if (libsres_msg_getflag(handle, ns_f_cd))
        strcat(buf, " cd");
    _print_or_log(file, LOG_DEBUG, "%s", buf);

    _print_or_log(file, LOG_DEBUG, "; %s: %d", p_section(ns_s_qd, opcode),
                  qdcount);
    _print_or_log(file, LOG_DEBUG, ", %s: %d", p_section(ns_s_an, opcode),
                  ancount);
    _print_or_log(file, LOG_DEBUG, ", %s: %d", p_section(ns_s_ns, opcode),
                  nscount);
    _print_or_log(file, LOG_DEBUG, ", %s: %d\n", p_section(ns_s_ar, opcode),
                  arcount);
    do_section(&handle, ns_s_qd, RES_PRF_QUES, file);
    do_section(&handle, ns_s_an, RES_PRF_ANS, file);
    do_section(&handle, ns_s_ns, RES_PRF_AUTH, file);
    do_section(&handle, ns_s_ar, RES_PRF_ADD, file);
}
