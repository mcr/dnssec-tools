/*
 * Copyright 2005 SPARTA, Inc.  All rights reserved.
 * See the COPYING file distributed with this software for details.
 *
 * Author: Abhijit Hayatnagarkar
 *
 * This is the implementation file for a validating getaddrinfo function.
 * Applications should be able to use this in place of getaddrinfo with
 * minimal change.
 */

#include <stdio.h>
#include <stdlib.h>
#include <resolver.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <arpa/nameser.h>

#include "val_errors.h"
#include "val_query.h"
#include "val_log.h"
#include "validator.h"
#include "val_assertion.h"
#include "val_support.h"
#include "val_context.h"
#include "val_getaddrinfo.h"

static struct addrinfo *append_addrinfo (struct addrinfo *a1,
					 struct addrinfo *a2)
{
	struct addrinfo *a;
	if (a1 == NULL) {
		return a2;
	}

	a = a1;
	while (a->ai_next != NULL) {
		a = a->ai_next;
	}

	a->ai_next = a2;
	return a1;
}

/* duplicates just the current addrinfo struct
 * does not duplicate the entire chain
 * sets the ai_next pointer of the new addrinfo to NULL
 */
static struct addrinfo *duplicate_addrinfo (const struct addrinfo *a)
{
	struct addrinfo_dnssec_wrapper *new_aw;

	if (a == NULL) {
		return NULL;
	}

	new_aw = (struct addrinfo_dnssec_wrapper *) malloc (sizeof (struct addrinfo_dnssec_wrapper));
	bzero(new_aw, sizeof(struct addrinfo_dnssec_wrapper));
	new_aw->ainfo.ai_flags = a->ai_flags;
	new_aw->ainfo.ai_family = a->ai_family;
	new_aw->ainfo.ai_socktype = a->ai_socktype;
	new_aw->ainfo.ai_protocol = a->ai_protocol;
	new_aw->ainfo.ai_addrlen = a->ai_addrlen;
	new_aw->ainfo.ai_addr = (struct sockaddr *) malloc (a->ai_addrlen);
	memcpy(new_aw->ainfo.ai_addr, a->ai_addr, a->ai_addrlen);

	if (a->ai_canonname != NULL) {
		new_aw->ainfo.ai_canonname = strdup(a->ai_canonname);
	}
	else {
		new_aw->ainfo.ai_canonname = NULL;
	}
	new_aw->ainfo.ai_next = NULL;

	new_aw->dnssec_status = ADDRINFO_DNSSEC_STATUS(a);
	return &(new_aw->ainfo);
}

/*
 * 
 */
static struct addrinfo *process_service_and_hints(struct addrinfo_dnssec_wrapper *ainfo_wrapper,
						  const char *servname,
						  const struct addrinfo *hints)
{
	struct addrinfo *a1 = NULL;
	struct addrinfo *retval = NULL;
	struct addrinfo *a2 = NULL;
	int dup_needed = 0;

	if (ainfo_wrapper == NULL) {
		return NULL;
	}

	a1 = &(ainfo_wrapper->ainfo);
	retval = a1;

	/* Flags */
	a1->ai_flags = (hints == NULL) ? (AI_V4MAPPED | AI_ADDRCONFIG) : hints->ai_flags; /* ??? */

	if ((hints == NULL || hints->ai_socktype == 0 || hints->ai_socktype == SOCK_STREAM) &&
	    (servname == NULL || getservbyname(servname, "tcp") != NULL)) {
		
		a1->ai_socktype = SOCK_STREAM;
		a1->ai_protocol = IPPROTO_TCP;
		a1->ai_next = NULL;
		dup_needed = 1;
	}
	
	if ((hints == NULL || hints->ai_socktype == 0 || hints->ai_socktype == SOCK_DGRAM) &&
	    (servname == NULL || getservbyname(servname, "udp") != NULL)) {
		
		if (dup_needed) {
			a2 = duplicate_addrinfo (a1);
			a1->ai_next = a2;
			a1 = a2;
		}
		a1->ai_socktype = SOCK_DGRAM;
		a1->ai_protocol = IPPROTO_UDP;
		dup_needed = 1;
	}
	
	if ((hints == NULL || hints->ai_socktype == 0 || hints->ai_socktype == SOCK_RAW) &&
	    (servname == NULL || getservbyname(servname, "ip") != NULL)) {
		
		if (dup_needed) {
			a2 = duplicate_addrinfo (a1);
			a1->ai_next = a2;
			a1 = a2;
		}
		a1->ai_socktype = SOCK_RAW;
		a1->ai_protocol = IPPROTO_IP;
		dup_needed = 1;
	}
	
	if (!dup_needed) {
		/* no valid protocol found */
		freeaddrinfo (retval);
		retval = NULL;
	}

	return retval;
}


/* Read the ETC_HOSTS file and check if it contains the given name
 */
static struct addrinfo *get_addrinfo_from_etc_hosts (const char *nodename,
						     const char *servname,
						     const struct addrinfo *hints)
{
	return NULL; /* XXX for now */
} /* get_addrinfo_from_etc_hosts() */


static struct addrinfo *get_addrinfo_from_rrset (struct rrset_rec *rrset,
						 int dnssec_status,
						 const char *servname,
						 const struct addrinfo *hints)
{
	struct addrinfo *ainfo_head = NULL;
	struct addrinfo *ainfo_tail = NULL;

	val_log("get_addrinfo_from_rrset called with dnssec_status = %s\n", p_val_error(dnssec_status));
	if (!rrset) {
		val_log("rrset is null\n");
	}

	while (rrset != NULL) {
		struct rr_rec *rr = rrset->rrs_data;

		while (rr != NULL) {
			struct addrinfo_dnssec_wrapper *ainfo_wrapper = 
				(struct addrinfo_dnssec_wrapper *) malloc (sizeof (struct addrinfo_dnssec_wrapper));
			struct addrinfo *ainfo = (struct addrinfo *) (&(ainfo_wrapper->ainfo));
			bzero(ainfo_wrapper, sizeof(struct addrinfo_dnssec_wrapper));

			if (rrset->rrs_type_h == ns_t_cname) {
				if (hints && (hints->ai_flags & AI_CANONNAME) && (!ainfo_head)) {
					char dname[MAXDNAME];
					bzero(dname, MAXDNAME);
					if (ns_name_ntop(rr->rr_rdata, dname, MAXDNAME) < 0) {
						/* error */
						val_log("error in ns_name_ntop");
					}
					else {
						ainfo->ai_canonname = (char *) malloc ((strlen(dname) + 1) * sizeof(char));
						memcpy(ainfo->ai_canonname, dname, strlen(dname) + 1);
					}
				}
			}
			else if (rrset->rrs_type_h == ns_t_a) {
				struct sockaddr_in *saddr4 = (struct sockaddr_in *) malloc (sizeof (struct sockaddr_in));
				ainfo->ai_family = AF_INET;
				ainfo->ai_addrlen = sizeof (struct sockaddr_in);
				memcpy(&(saddr4->sin_addr.s_addr), rr->rr_rdata, rr->rr_rdata_length_h);
				ainfo->ai_addr = (struct sockaddr *) saddr4;
			}
			else if (rrset->rrs_type_h == ns_t_aaaa) {
				struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *) malloc (sizeof (struct sockaddr_in6));
				ainfo->ai_family = AF_INET6;
				ainfo->ai_addrlen = sizeof (struct sockaddr_in6);
				memcpy(&(saddr6->sin6_addr.s6_addr), rr->rr_rdata, rr->rr_rdata_length_h);
				ainfo->ai_addr = (struct sockaddr *) saddr6;
			}
			else {
				free (ainfo_wrapper);
				rr = rr->rr_next;
				continue;
			}
				
			ainfo_wrapper = (struct addrinfo_dnssec_wrapper *)ainfo;
			ainfo_wrapper->dnssec_status = dnssec_status;
			
			ainfo = process_service_and_hints (ainfo_wrapper, servname, hints);

			if (ainfo_head == NULL) {
				ainfo_head = ainfo;
			}
			else {
				ainfo_tail->ai_next = ainfo;
			}

			if (ainfo)
				ainfo_tail = ainfo;

			rr = rr->rr_next;
		}

		rrset = rrset->rrs_next;
	}

	return ainfo_head;
}

/* Converts data in the rrset_rec structure into a addrinfo structure */
static struct addrinfo *get_addrinfo_from_dns (const char *nodename,
					       const char *servname,
					       const struct addrinfo *hints)
{
	struct query_chain *queries = NULL;
	struct assertion_chain *assertions = NULL;
	struct val_result *results = NULL;
	val_context_t *context = NULL;
	struct addrinfo *ainfo = NULL;
	u_char name_n[MAXCDNAME];
	int retval;
	
	val_log("get_addrinfo_from_dns() called\n");
	context = get_context(NULL);

	if (hints == NULL || hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET) {

		val_log("checking for A records\n");

		if ((retval = ns_name_pton(nodename, name_n, MAXCDNAME - 1)) != -1) {
			if ((retval = add_to_query_chain(&queries, name_n, ns_t_a, ns_c_in)) == NO_ERROR) {
				if ((retval = resolve_n_check(context, name_n, ns_t_a, ns_c_in, 0,
							      &queries, &assertions, &results)) != NO_ERROR) {
					val_log("resolve_n_check failed");
				}
			}
			else {
				val_log("add_to_query_chain failed");
			}
		}
		else {
			val_log("ns_name_pton failed");
		}
	
		if (results && results->as) {
			struct addrinfo *ainfo_new = NULL;
			ainfo_new = get_addrinfo_from_rrset (results->as->ac_data, results->status,
							     servname, hints);
			if (ainfo_new) {
				val_log("A records found\n");
				ainfo = append_addrinfo(ainfo, ainfo_new);
			}
		}
		free_query_chain(&queries); queries = NULL;
		free_assertion_chain(&assertions); assertions = NULL;
		free_result_chain(&results); results = NULL;
	}

	if (hints == NULL || hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET6) {

		val_log("checking for AAAA records\n");

		if ((retval = ns_name_pton(nodename, name_n, MAXCDNAME - 1)) != -1) {
			if ((retval = add_to_query_chain(&queries, name_n, ns_t_aaaa, ns_c_in)) == NO_ERROR) {
				if ((retval = resolve_n_check(context, name_n, ns_t_aaaa, ns_c_in, 0,
							      &queries, &assertions, &results)) != NO_ERROR) {
					val_log("resolve_n_check failed");
				}
			}
			else {
				val_log("add_to_query_chain failed");
			}
		}
		else {
			val_log("ns_name_pton failed");
		}
	
		if (results && results->as) {
			struct addrinfo *ainfo_new = NULL;
			ainfo_new = get_addrinfo_from_rrset (results->as->ac_data, results->status,
							     servname, hints);
			if (ainfo_new) {
				val_log("AAAA records found\n");
				ainfo = append_addrinfo(ainfo, ainfo_new);
			}
		}
		free_query_chain(&queries); queries = NULL;
		free_assertion_chain(&assertions); assertions = NULL;
		free_result_chain(&results); results = NULL;
	}

	if (context != NULL) {
		destroy_context(context);
	}
	
	return ainfo;

} /* get_addrinfo_from_dns() */


int val_getaddrinfo (const char *nodename, const char *servname,
		     const struct addrinfo *hints,
		     struct addrinfo **res)
{
	struct in_addr ip4_addr;
	struct in6_addr ip6_addr;
	struct addrinfo *ainfo4 = NULL;
	struct addrinfo *ainfo6 = NULL;
	int is_ip4 = 0;
	int is_ip6 = 0;

	val_log("val_getaddrinfo called with nodename = %s, servname = %s\n",
		nodename == NULL? "(null)":nodename,
		servname == NULL? "(null)": servname);

	if ((nodename == NULL) && (servname == NULL)) {
		return EAI_NONAME;
	}

	bzero(&ip4_addr, sizeof(struct in_addr));
	bzero(&ip6_addr, sizeof(struct in6_addr));

	if (nodename == NULL || inet_pton(AF_INET, nodename, &ip4_addr) > 0) {
		struct addrinfo_dnssec_wrapper *ainfo_wrapper = 
			(struct addrinfo_dnssec_wrapper *) malloc (sizeof (struct addrinfo_dnssec_wrapper));
		struct addrinfo *ainfo = (struct addrinfo *) (&(ainfo_wrapper->ainfo));
		struct sockaddr_in *saddr4 = (struct sockaddr_in *) malloc (sizeof (struct sockaddr_in));

		is_ip4 = 1;
		if (nodename == NULL) {
			if (inet_pton(AF_INET, "127.0.0.1", &ip4_addr) < 0) {
				/* ??? */
				;
			}				
		}

		bzero(ainfo_wrapper, sizeof(struct addrinfo_dnssec_wrapper));
		bzero(saddr4, sizeof(struct sockaddr_in));

		ainfo->ai_family = AF_INET;
		ainfo->ai_addrlen = sizeof (struct sockaddr_in);
		memcpy(&(saddr4->sin_addr), &ip4_addr, sizeof(struct in_addr));
		ainfo->ai_addr = (struct sockaddr *) saddr4;
		ainfo->ai_flags = (hints == NULL) ? 40: hints->ai_flags; /* ??? */
		ainfo->ai_socktype = SOCK_DGRAM; /* for now */
		ainfo->ai_protocol = IPPROTO_UDP; /* for now */
		ainfo->ai_canonname = NULL;

		ainfo_wrapper->dnssec_status = VALIDATE_SUCCESS;
		ainfo4 = process_service_and_hints(ainfo_wrapper, servname, hints);

		if (nodename != NULL) {
			*res = ainfo4;
			if (*res != NULL) {
				return 0;
			}
			else {
				return EAI_NONAME;
			}
		}
	}
	
	if (nodename == NULL || inet_pton(AF_INET6, nodename, &ip6_addr) > 0) {
		
		struct addrinfo_dnssec_wrapper *ainfo_wrapper = 
			(struct addrinfo_dnssec_wrapper *) malloc (sizeof (struct addrinfo_dnssec_wrapper));
		struct addrinfo *ainfo = (struct addrinfo *) (&(ainfo_wrapper->ainfo));
		struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *) malloc (sizeof (struct sockaddr_in6));
		
		is_ip6 = 1;

		if (nodename == NULL) {
			if (inet_pton(AF_INET6, "::1", &ip6_addr) < 0) {
				/* ??? */
				;
			}
		}

		bzero(ainfo_wrapper, sizeof(struct addrinfo_dnssec_wrapper));
		bzero(saddr6, sizeof(struct sockaddr_in6));

		ainfo->ai_family = AF_INET6;
		ainfo->ai_addrlen = sizeof (struct sockaddr_in6);
		memcpy(&(saddr6->sin6_addr), &ip6_addr, sizeof(struct in6_addr));
		ainfo->ai_addr = (struct sockaddr *) saddr6;
		ainfo->ai_flags = (hints == NULL) ? 40: hints->ai_flags; /* ??? */
		ainfo->ai_socktype = SOCK_DGRAM; /* for now */
		ainfo->ai_protocol = IPPROTO_UDP; /* for now */
		ainfo->ai_canonname = NULL;

		ainfo_wrapper->dnssec_status = VALIDATE_SUCCESS;
		ainfo6 = process_service_and_hints(ainfo_wrapper, servname, hints);
		if (nodename == NULL) {
			*res = append_addrinfo(ainfo4, ainfo6);
		}
		else {
			*res = ainfo6;
		}

		if (*res != NULL) {
			return 0;
		}
		else {
			return EAI_NONAME;
		}
	}

	if (nodename && !is_ip4 && !is_ip6) {
		/* First check ETC_HOSTS file
		 * XXX: TODO check the order in the ETC_HOST_CONF file
		 */
		*res = get_addrinfo_from_etc_hosts (nodename, servname, hints);

		if (*res != NULL) {
			return 0;
		}

		/*
		 * Try DNS
		 */

		*res = get_addrinfo_from_dns (nodename, servname, hints);

		if (*res != NULL) {
			return 0;
		}
		else {
			return EAI_NONAME;
		}
	}
} /* val_getaddrinfo() */
