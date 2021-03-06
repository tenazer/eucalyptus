// -*- mode: C; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*-
// vim: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:

/*************************************************************************
 * Copyright 2009-2012 Eucalyptus Systems, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 * Please contact Eucalyptus Systems, Inc., 6755 Hollister Ave., Goleta
 * CA 93117, USA or visit http://www.eucalyptus.com/licenses/ if you need
 * additional information or have any questions.
 *
 * This file may incorporate work covered under the following copyright
 * and permission notice:
 *
 *   Software License Agreement (BSD License)
 *
 *   Copyright (c) 2008, Regents of the University of California
 *   All rights reserved.
 *
 *   Redistribution and use of this software in source and binary forms,
 *   with or without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE. USERS OF THIS SOFTWARE ACKNOWLEDGE
 *   THE POSSIBLE PRESENCE OF OTHER OPEN SOURCE LICENSED MATERIAL,
 *   COPYRIGHTED MATERIAL OR PATENTED MATERIAL IN THIS SOFTWARE,
 *   AND IF ANY SUCH MATERIAL IS DISCOVERED THE PARTY DISCOVERING
 *   IT MAY INFORM DR. RICH WOLSKI AT THE UNIVERSITY OF CALIFORNIA,
 *   SANTA BARBARA WHO WILL THEN ASCERTAIN THE MOST APPROPRIATE REMEDY,
 *   WHICH IN THE REGENTS' DISCRETION MAY INCLUDE, WITHOUT LIMITATION,
 *   REPLACEMENT OF THE CODE SO IDENTIFIED, LICENSING OF THE CODE SO
 *   IDENTIFIED, OR WITHDRAWAL OF THE CODE CAPABILITY TO THE EXTENT
 *   NEEDED TO COMPLY WITH ANY SUCH LICENSES OR RIGHTS.
 ************************************************************************/

//!
//! @file net/euca_gni.c
//! Implementation of the global network interface
//!

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  INCLUDES                                  |
 |                                                                            |
\*----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <eucalyptus.h>
#include <misc.h>
#include <hash.h>
#include <euca_string.h>
#include <euca_network.h>
#include <atomic_file.h>

#include "ipt_handler.h"
#include "ips_handler.h"
#include "ebt_handler.h"
#include "dev_handler.h"
#include "euca_gni.h"
#include "euca_lni.h"

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  DEFINES                                   |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  TYPEDEFS                                  |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                ENUMERATIONS                                |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                 STRUCTURES                                 |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                             EXTERNAL VARIABLES                             |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/* Should preferably be handled in header file */

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              GLOBAL VARIABLES                              |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              STATIC VARIABLES                              |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              STATIC PROTOTYPES                             |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                   MACROS                                   |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                               IMPLEMENTATION                               |
 |                                                                            |
\*----------------------------------------------------------------------------*/

//!
//! Creates a unique IP table chain name for a given security group. This name, if successful
//! will have the form of EU_[hash] where [hash] is the 64 bit encoding of the resulting
//! "[account id]-[group name]" string from the given security group information.
//!
//! @param[in] gni a pointer to the global network information structure
//! @param[in] secgroup a pointer to the security group for which we compute the chain name
//! @param[out] outchainname a pointer to the string that will contain the computed name
//!
//! @return 0 on success or 1 on failure
//!
//! @see
//!
//! @pre
//!     The outchainname parameter Must not be NULL but it should point to a NULL value. If
//!     this does not point to NULL, the memory will be lost when replaced with the out value.
//!
//! @post
//!     On success, the outchainname will point to the resulting string. If a failure occur, any
//!     value pointed by outchainname is non-deterministic.
//!
//! @note
//!
int gni_secgroup_get_chainname(globalNetworkInfo * gni, gni_secgroup * secgroup, char **outchainname)
{
    char hashtok[16 + 128 + 1];
    char chainname[48];
    char *chainhash = NULL;

    if (!gni || !secgroup || !outchainname) {
        LOGERROR("invalid input\n");
        return (1);
    }

    snprintf(hashtok, 16 + 128 + 1, "%s-%s", secgroup->accountId, secgroup->name);
    hash_b64enc_string(hashtok, &chainhash);
    if (chainhash) {
        snprintf(chainname, 48, "EU_%s", chainhash);
        *outchainname = strdup(chainname);
        EUCA_FREE(chainhash);
        return (0);
    }
    LOGERROR("could not create iptables compatible chain name for sec. group (%s)\n", secgroup->name);
    return (1);
}

//!
//! Looks up for the cluster for which we are assigned within a configured cluster list. We can
//! be the cluster itself or one of its node.
//!
//! @param[in] gni a pointer to the global network information structure
//! @param[out] outclusterptr a pointer to the associated cluster structure pointer
//!
//! @return 0 if a matching cluster structure is found or 1 if not found or a failure occured
//!
//! @see gni_is_self()
//!
//! @pre
//!
//! @post
//!     On success the value pointed by outclusterptr is valid. On failure, this value
//!     is non-deterministic.
//!
//! @note
//!
int gni_find_self_cluster(globalNetworkInfo * gni, gni_cluster ** outclusterptr)
{
    int i, j;
    char *strptra = NULL;

    if (!gni || !outclusterptr) {
        LOGERROR("invalid input\n");
        return (1);
    }

    *outclusterptr = NULL;

    for (i = 0; i < gni->max_clusters; i++) {
        // check to see if local host is the enabled cluster controller
        strptra = hex2dot(gni->clusters[i].enabledCCIp);
        if (strptra) {
            if (!gni_is_self(strptra)) {
                EUCA_FREE(strptra);
                *outclusterptr = &(gni->clusters[i]);
                return (0);
            }
            EUCA_FREE(strptra);
        }
        // otherwise, check to see if local host is a node in the cluster
        for (j = 0; j < gni->clusters[i].max_nodes; j++) {
            //      if (!strcmp(gni->clusters[i].nodes[j].name, outnodeptr->name)) {
            if (!gni_is_self(gni->clusters[i].nodes[j].name)) {
                *outclusterptr = &(gni->clusters[i]);
                return (0);
            }
        }
    }
    return (1);
}

//!
//! Looks up for the cluster for which we are assigned within a configured cluster list. We can
//! be the cluster itself or one of its node.
//!
//! @param[in] gni a pointer to the global network information structure
//! @param[in] psGroupId a pointer to a constant string containing the Security-Group ID we're looking for
//! @param[out] pSecGroup a pointer to the associated security group structure pointer
//!
//! @return 0 if a matching security group structure is found or 1 if not found or a failure occured
//!
//! @see
//!
//! @pre
//!     All the provided parameter must be valid and non-NULL.
//!
//! @post
//!     On success the value pointed by pSecGroup is valid. On failure, this value
//!     is non-deterministic.
//!
//! @note
//!
int gni_find_secgroup(globalNetworkInfo * gni, const char *psGroupId, gni_secgroup ** pSecGroup)
{
    int i = 0;

    if (!gni || !psGroupId || !pSecGroup) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // Initialize to NULL
    (*pSecGroup) = NULL;

    // Go through our security group list and look for that group
    for (i = 0; i < gni->max_secgroups; i++) {
        if (!strcmp(psGroupId, gni->secgroups[i].name)) {
            (*pSecGroup) = &(gni->secgroups[i]);
            return (0);
        }
    }
    return (1);
}

//!
//! Looks up through a list of configured node for the one that is associated with
//! this currently running instance.
//!
//! @param[in]  gni a pointer to the global network information structure
//! @param[out] outnodeptr a pointer to the associated node structure pointer
//!
//! @return 0 if a matching node structure is found or 1 if not found or a failure occured
//!
//! @see gni_is_self()
//!
//! @pre
//!
//! @post
//!     On success the value pointed by outnodeptr is valid. On failure, this value
//!     is non-deterministic.
//!
//! @note
//!
int gni_find_self_node(globalNetworkInfo * gni, gni_node ** outnodeptr)
{
    int i, j;

    if (!gni || !outnodeptr) {
        LOGERROR("invalid input\n");
        return (1);
    }

    *outnodeptr = NULL;

    for (i = 0; i < gni->max_clusters; i++) {
        for (j = 0; j < gni->clusters[i].max_nodes; j++) {
            if (!gni_is_self(gni->clusters[i].nodes[j].name)) {
                *outnodeptr = &(gni->clusters[i].nodes[j]);
                return (0);
            }
        }
    }

    return (1);
}

//!
//! Validates if the given test_ip is a local IP address on this system
//!
//! @param[in] test_ip a string containing the IP to validate
//!
//! @return 0 if the test_ip is a local IP or 1 on failure or if not found locally
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_is_self(const char *test_ip)
{
    DIR *DH = NULL;
    struct dirent dent, *result = NULL;
    int max, rc, i;
    u32 *outips = NULL, *outnms = NULL;
    char *strptra = NULL;

    if (!test_ip) {
        LOGERROR("invalid input\n");
        return (1);
    }

    DH = opendir("/sys/class/net/");
    if (!DH) {
        LOGERROR("could not open directory /sys/class/net/ for read: check permissions\n");
        return (1);
    }

    rc = readdir_r(DH, &dent, &result);
    while (!rc && result) {
        if (strcmp(dent.d_name, ".") && strcmp(dent.d_name, "..")) {
            rc = getdevinfo(dent.d_name, &outips, &outnms, &max);
            for (i = 0; i < max; i++) {
                strptra = hex2dot(outips[i]);
                if (strptra) {
                    if (!strcmp(strptra, test_ip)) {
                        EUCA_FREE(strptra);
                        EUCA_FREE(outips);
                        EUCA_FREE(outnms);
                        closedir(DH);
                        return (0);
                    }
                    EUCA_FREE(strptra);
                }
            }
            EUCA_FREE(outips);
            EUCA_FREE(outnms);
        }
        rc = readdir_r(DH, &dent, &result);
    }
    closedir(DH);

    return (1);
}

//!
//! TODO: Function description.
//!
//! @param[in]  gni a pointer to the global network information structure
//! @param[in]  cluster_names
//! @param[in]  max_cluster_names
//! @param[out] out_cluster_names
//! @param[out] out_max_cluster_names
//! @param[out] out_clusters
//! @param[out] out_max_clusters
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_cloud_get_clusters(globalNetworkInfo * gni, char **cluster_names, int max_cluster_names, char ***out_cluster_names, int *out_max_cluster_names, gni_cluster ** out_clusters,
                           int *out_max_clusters)
{
    int ret = 0, getall = 0, i = 0, j = 0, retcount = 0, do_outnames = 0, do_outstructs = 0;
    gni_cluster *ret_clusters = NULL;
    char **ret_cluster_names = NULL;

    if (!cluster_names || max_cluster_names <= 0) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (out_cluster_names && out_max_cluster_names) {
        do_outnames = 1;
    }
    if (out_clusters && out_max_clusters) {
        do_outstructs = 1;
    }

    if (!do_outnames && !do_outstructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }

    if (do_outnames) {
        *out_cluster_names = NULL;
        *out_max_cluster_names = 0;
    }
    if (do_outstructs) {
        *out_clusters = NULL;
        *out_max_clusters = 0;
    }

    if (!strcmp(cluster_names[0], "*")) {
        getall = 1;
        if (do_outnames)
            *out_cluster_names = EUCA_ZALLOC(gni->max_clusters, sizeof(char *));
        if (do_outstructs)
            *out_clusters = EUCA_ZALLOC(gni->max_clusters, sizeof(gni_cluster));
    }

    if (do_outnames)
        ret_cluster_names = *out_cluster_names;
    if (do_outstructs)
        ret_clusters = *out_clusters;

    retcount = 0;
    for (i = 0; i < gni->max_clusters; i++) {
        if (getall) {
            if (do_outnames)
                ret_cluster_names[i] = strdup(gni->clusters[i].name);
            if (do_outstructs)
                memcpy(&(ret_clusters[i]), &(gni->clusters[i]), sizeof(gni_cluster));
            retcount++;
        } else {
            for (j = 0; j < max_cluster_names; j++) {
                if (!strcmp(cluster_names[j], gni->clusters[i].name)) {
                    if (do_outnames) {
                        *out_cluster_names = realloc(*out_cluster_names, sizeof(char *) * (retcount + 1));
                        ret_cluster_names = *out_cluster_names;
                        ret_cluster_names[retcount] = strdup(gni->clusters[i].name);
                    }
                    if (do_outstructs) {
                        *out_clusters = realloc(*out_clusters, sizeof(gni_cluster) * (retcount + 1));
                        ret_clusters = *out_clusters;
                        memcpy(&(ret_clusters[retcount]), &(gni->clusters[i]), sizeof(gni_cluster));
                    }
                    retcount++;
                }
            }
        }
    }
    if (do_outnames)
        *out_max_cluster_names = retcount;
    if (do_outstructs)
        *out_max_clusters = retcount;

    return (ret);
}

//!
//! Retrives the list of security groups configured under a cloud
//!
//! @param[in]  pGni a pointer to our global network view structure
//! @param[in]  psSecGroupNames a string pointer to the name of groups we're looking for
//! @param[in]  nbSecGroupNames the number of groups in the psSecGroupNames list
//! @param[out] psOutSecGroupNames a string pointer that will contain the list of group names we found (if non NULL)
//! @param[out] pOutNbSecGroupNames a pointer to the number of groups that matched in the psOutSecGroupNames list
//! @param[out] pOutSecGroups a pointer to the list of security group structures that match what we're looking for
//! @param[out] pOutNbSecGroups a pointer to the number of structures in the psOutSecGroups list
//!
//! @return 0 on success or 1 if any failure occured
//!
//! @see
//!
//! @pre  TODO:
//!
//! @post TODO:
//!
//! @note
//!
int gni_cloud_get_secgroup(globalNetworkInfo * pGni, char **psSecGroupNames, int nbSecGroupNames, char ***psOutSecGroupNames, int *pOutNbSecGroupNames,
                           gni_secgroup ** pOutSecGroups, int *pOutNbSecGroups)
{
    int ret = 0;
    int i = 0;
    int x = 0;
    int retCount = 0;
    char **psRetSecGroupNames = NULL;
    boolean getAll = FALSE;
    boolean doOutNames = FALSE;
    boolean doOutStructs = FALSE;
    gni_secgroup *pRetSecGroup = NULL;

    // Make sure our GNI pointer isn't NULL
    if (!pGni) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // Are we building the name list?
    if (psOutSecGroupNames && pOutNbSecGroupNames) {
        doOutNames = TRUE;
        *psOutSecGroupNames = NULL;
        *pOutNbSecGroupNames = 0;
    }
    // Are we building the structure list?
    if (pOutSecGroups && pOutNbSecGroups) {
        doOutStructs = TRUE;
        *pOutSecGroups = NULL;
        *pOutNbSecGroups = 0;
    }
    // Are we doing anything?
    if (!doOutNames && !doOutStructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }
    // Do we have any groups?
    if (pGni->max_secgroups == 0)
        return (0);

    // If we do it all, allocate the memory now
    if (psSecGroupNames == NULL || !strcmp(psSecGroupNames[0], "*")) {
        getAll = TRUE;
        if (doOutNames)
            *psOutSecGroupNames = EUCA_ZALLOC(pGni->max_secgroups, sizeof(char *));
        if (doOutStructs)
            *pOutSecGroups = EUCA_ZALLOC(pGni->max_secgroups, sizeof(gni_secgroup));
    }
    // Setup our returning name list pointer
    if (doOutNames)
        psRetSecGroupNames = *psOutSecGroupNames;

    // Setup our returning structure list pointer
    if (doOutStructs)
        pRetSecGroup = *pOutSecGroups;

    // Go through the group list
    for (i = 0, retCount = 0; i < pGni->max_secgroups; i++) {
        if (getAll) {
            if (doOutNames)
                psRetSecGroupNames[retCount] = strdup(pGni->secgroups[i].name);

            if (doOutStructs)
                memcpy(&(pRetSecGroup[retCount]), &(pGni->secgroups[i]), sizeof(gni_secgroup));
            retCount++;
        } else {
            if (!strcmp(psSecGroupNames[x], pGni->secgroups[i].name)) {
                if (doOutNames) {
                    *psOutSecGroupNames = EUCA_REALLOC(*psOutSecGroupNames, (retCount + 1), sizeof(char *));
                    psRetSecGroupNames = *psOutSecGroupNames;
                    psRetSecGroupNames[retCount] = strdup(pGni->secgroups[i].name);
                }

                if (doOutStructs) {
                    *pOutSecGroups = EUCA_REALLOC(*pOutSecGroups, (retCount + 1), sizeof(gni_instance));
                    pRetSecGroup = *pOutSecGroups;
                    memcpy(&(pRetSecGroup[retCount]), &(pGni->secgroups[i]), sizeof(gni_secgroup));
                }
                retCount++;
            }
        }
    }

    if (doOutNames)
        *pOutNbSecGroupNames = retCount;

    if (doOutStructs)
        *pOutNbSecGroups = retCount;

    return (ret);
}

//!
//! TODO: Function description.
//!
//! @param[in]  gni a pointer to the global network information structure
//! @param[in]  cluster
//! @param[in]  node_names
//! @param[in]  max_node_names
//! @param[out] out_node_names
//! @param[out] out_max_node_names
//! @param[out] out_nodes
//! @param[out] out_max_nodes
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_cluster_get_nodes(globalNetworkInfo * gni, gni_cluster * cluster, char **node_names, int max_node_names, char ***out_node_names, int *out_max_node_names,
                          gni_node ** out_nodes, int *out_max_nodes)
{
    int ret = 0, rc = 0, getall = 0, i = 0, j = 0, retcount = 0, do_outnames = 0, do_outstructs = 0, out_max_clusters = 0;
    gni_node *ret_nodes = NULL;
    gni_cluster *out_clusters = NULL;
    char **ret_node_names = NULL, **cluster_names = NULL;

    if (!gni) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (out_node_names && out_max_node_names) {
        do_outnames = 1;
    }
    if (out_nodes && out_max_nodes) {
        do_outstructs = 1;
    }

    if (!do_outnames && !do_outstructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }

    if (do_outnames) {
        *out_node_names = NULL;
        *out_max_node_names = 0;
    }
    if (do_outstructs) {
        *out_nodes = NULL;
        *out_max_nodes = 0;
    }

    cluster_names = EUCA_ZALLOC(1, sizeof(char *));
    cluster_names[0] = cluster->name;
    rc = gni_cloud_get_clusters(gni, cluster_names, 1, NULL, NULL, &out_clusters, &out_max_clusters);
    if (rc || out_max_clusters <= 0) {
        LOGWARN("nothing to do, no matching cluster named '%s' found\n", cluster->name);
        EUCA_FREE(cluster_names);
        EUCA_FREE(out_clusters);
        return (0);
    }

    if ((node_names == NULL) || !strcmp(node_names[0], "*")) {
        getall = 1;
        if (do_outnames)
            *out_node_names = EUCA_ZALLOC(cluster->max_nodes, sizeof(char *));
        if (do_outstructs)
            *out_nodes = EUCA_ZALLOC(cluster->max_nodes, sizeof(gni_node));
    }

    if (do_outnames)
        ret_node_names = *out_node_names;

    if (do_outstructs)
        ret_nodes = *out_nodes;

    retcount = 0;
    for (i = 0; i < cluster->max_nodes; i++) {
        if (getall) {
            if (do_outnames)
                ret_node_names[i] = strdup(out_clusters[0].nodes[i].name);

            if (do_outstructs)
                memcpy(&(ret_nodes[i]), &(out_clusters[0].nodes[i]), sizeof(gni_node));

            retcount++;
        } else {
            for (j = 0; j < max_node_names; j++) {
                if (!strcmp(node_names[j], out_clusters[0].nodes[i].name)) {
                    if (do_outnames) {
                        *out_node_names = realloc(*out_node_names, sizeof(char *) * (retcount + 1));
                        ret_node_names = *out_node_names;
                        ret_node_names[retcount] = strdup(out_clusters[0].nodes[i].name);
                    }
                    if (do_outstructs) {
                        *out_nodes = realloc(*out_nodes, sizeof(gni_node) * (retcount + 1));
                        ret_nodes = *out_nodes;
                        memcpy(&(ret_nodes[retcount]), &(out_clusters[0].nodes[i]), sizeof(gni_node));
                    }
                    retcount++;
                }
            }
        }
    }

    if (do_outnames)
        *out_max_node_names = retcount;

    if (do_outstructs)
        *out_max_nodes = retcount;

    EUCA_FREE(cluster_names);
    EUCA_FREE(out_clusters);
    return (ret);
}

//!
//! TODO: Function description.
//!
//! @param[in]  pGni a pointer to the global network information structure
//! @param[in]  pCluster
//! @param[in]  psInstanceNames
//! @param[in]  nbInstanceNames
//! @param[out] psOutInstanceNames
//! @param[out] pOutNbInstanceNames
//! @param[out] pOutInstances
//! @param[out] pOutNbInstances
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_cluster_get_instances(globalNetworkInfo * pGni, gni_cluster * pCluster, char **psInstanceNames, int nbInstanceNames,
                              char ***psOutInstanceNames, int *pOutNbInstanceNames, gni_instance ** pOutInstances, int *pOutNbInstances)
{
    int ret = 0;
    int i = 0;
    int k = 0;
    int x = 0;
    int y = 0;
    int retCount = 0;
    int nbInstances = 0;
    char **psRetInstanceNames = NULL;
    boolean getAll = FALSE;
    boolean doOutNames = FALSE;
    boolean doOutStructs = FALSE;
    gni_instance *pRetInstances = NULL;

    if (!pGni || !pCluster) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (psOutInstanceNames && pOutNbInstanceNames) {
        doOutNames = TRUE;
    }
    if (pOutInstances && pOutNbInstances) {
        doOutStructs = TRUE;
    }

    if (!doOutNames && !doOutStructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }

    if (doOutNames) {
        *psOutInstanceNames = NULL;
        *pOutNbInstanceNames = 0;
    }
    if (doOutStructs) {
        *pOutInstances = NULL;
        *pOutNbInstances = 0;
    }
    // Do we have any nodes?
    if (pCluster->max_nodes == 0)
        return (0);

    for (i = 0, nbInstances = 0; i < pCluster->max_nodes; i++) {
        nbInstances += pCluster->nodes[i].max_instance_names;
    }

    // Do we have any instances?
    if (nbInstances == 0)
        return (0);

    if (psInstanceNames == NULL || !strcmp(psInstanceNames[0], "*")) {
        getAll = TRUE;
        if (doOutNames)
            *psOutInstanceNames = EUCA_ZALLOC(nbInstances, sizeof(char *));
        if (doOutStructs)
            *pOutInstances = EUCA_ZALLOC(nbInstances, sizeof(gni_instance));
    }

    if (doOutNames)
        psRetInstanceNames = *psOutInstanceNames;

    if (doOutStructs)
        pRetInstances = *pOutInstances;

    for (i = 0, retCount = 0; i < pCluster->max_nodes; i++) {
        for (k = 0; k < pCluster->nodes[i].max_instance_names; k++) {
            if (getAll) {
                if (doOutNames)
                    psRetInstanceNames[retCount] = strdup(pCluster->nodes[i].instance_names[k].name);

                if (doOutStructs) {
                    for (x = 0; x < pGni->max_instances; x++) {
                        if (!strcmp(pGni->instances[x].name, pCluster->nodes[i].instance_names[k].name)) {
                            memcpy(&(pRetInstances[retCount]), &(pGni->instances[x]), sizeof(gni_instance));
                            break;
                        }
                    }
                }
                retCount++;
            } else {
                for (x = 0; x < nbInstanceNames; x++) {
                    if (!strcmp(psInstanceNames[x], pCluster->nodes[i].instance_names[k].name)) {
                        if (doOutNames) {
                            *psOutInstanceNames = EUCA_REALLOC(*psOutInstanceNames, (retCount + 1), sizeof(char *));
                            psRetInstanceNames = *psOutInstanceNames;
                            psRetInstanceNames[retCount] = strdup(pCluster->nodes[i].instance_names[k].name);
                        }

                        if (doOutStructs) {
                            for (y = 0; y < pGni->max_instances; y++) {
                                if (!strcmp(pGni->instances[y].name, pCluster->nodes[i].instance_names[k].name)) {
                                    *pOutInstances = EUCA_REALLOC(*pOutInstances, (retCount + 1), sizeof(gni_instance));
                                    pRetInstances = *pOutInstances;
                                    memcpy(&(pRetInstances[retCount]), &(pGni->instances[y]), sizeof(gni_instance));
                                    break;
                                }
                            }
                        }
                        retCount++;
                    }
                }
            }
        }
    }

    if (doOutNames)
        *pOutNbInstanceNames = retCount;

    if (doOutStructs)
        *pOutNbInstances = retCount;

    return (ret);
}

//!
//! Retrives the list of security groups configured and active on a given cluster
//!
//! @param[in]  pGni a pointer to our global network view structure
//! @param[in]  pCluster a pointer to the cluster we're building the security group list for
//! @param[in]  psSecGroupNames a string pointer to the name of groups we're looking for
//! @param[in]  nbSecGroupNames the number of groups in the psSecGroupNames list
//! @param[out] psOutSecGroupNames a string pointer that will contain the list of group names we found (if non NULL)
//! @param[out] pOutNbSecGroupNames a pointer to the number of groups that matched in the psOutSecGroupNames list
//! @param[out] pOutSecGroups a pointer to the list of security group structures that match what we're looking for
//! @param[out] pOutNbSecGroups a pointer to the number of structures in the psOutSecGroups list
//!
//! @return 0 on success or 1 if any failure occured
//!
//! @see
//!
//! @pre  TODO:
//!
//! @post TODO:
//!
//! @note
//!
int gni_cluster_get_secgroup(globalNetworkInfo * pGni, gni_cluster * pCluster, char **psSecGroupNames, int nbSecGroupNames, char ***psOutSecGroupNames, int *pOutNbSecGroupNames,
                             gni_secgroup ** pOutSecGroups, int *pOutNbSecGroups)
{
    int ret = 0;
    int i = 0;
    int k = 0;
    int x = 0;
    int retCount = 0;
    int nbInstances = 0;
    char **psRetSecGroupNames = NULL;
    boolean found = FALSE;
    boolean getAll = FALSE;
    boolean doOutNames = FALSE;
    boolean doOutStructs = FALSE;
    gni_instance *pInstances = NULL;
    gni_secgroup *pRetSecGroup = NULL;

    // Make sure our GNI and Cluster pointers are valid
    if (!pGni || !pCluster) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // We will need to get the instances that are running under this cluster
    if (gni_cluster_get_instances(pGni, pCluster, NULL, 0, NULL, NULL, &pInstances, &nbInstances)) {
        LOGERROR("Failed to retrieve instances for cluster '%s'\n", pCluster->name);
        return (1);
    }
    // Are we building the name list?
    if (psOutSecGroupNames && pOutNbSecGroupNames) {
        doOutNames = TRUE;
        *psOutSecGroupNames = NULL;
        *pOutNbSecGroupNames = 0;
    }
    // Are we building the structure list?
    if (pOutSecGroups && pOutNbSecGroups) {
        doOutStructs = TRUE;
        *pOutSecGroups = NULL;
        *pOutNbSecGroups = 0;
    }
    // Are we doing anything?
    if (!doOutNames && !doOutStructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        EUCA_FREE(pInstances);
        return (0);
    }
    // Do we have any instances?
    if (nbInstances == 0) {
        EUCA_FREE(pInstances);
        return (0);
    }

    // Do we have any groups?
    if (pGni->max_secgroups == 0) {
        EUCA_FREE(pInstances);
        return (0);
    }
    // Allocate memory for all the groups if there is no search criterias
    if ((psSecGroupNames == NULL) || !strcmp(psSecGroupNames[0], "*")) {
        getAll = TRUE;
        if (doOutNames)
            *psOutSecGroupNames = EUCA_ZALLOC(pGni->max_secgroups, sizeof(char *));

        if (doOutStructs)
            *pOutSecGroups = EUCA_ZALLOC(pGni->max_secgroups, sizeof(gni_secgroup));
    }
    // Setup our returning name pointer
    if (doOutNames)
        psRetSecGroupNames = *psOutSecGroupNames;

    // Setup our returning structure pointer
    if (doOutStructs)
        pRetSecGroup = *pOutSecGroups;

    // Scan all our groups
    for (i = 0, retCount = 0; i < pGni->max_secgroups; i++) {
        if (getAll) {
            // Check if this we have any instance using this group
            for (k = 0, found = FALSE; ((k < nbInstances) && !found); k++) {
                for (x = 0; ((x < pInstances[k].max_secgroup_names) && !found); x++) {
                    if (!strcmp(pGni->secgroups[i].name, pInstances[k].secgroup_names[x].name)) {
                        found = TRUE;
                    }
                }
            }

            // If we have any instance using this group, then copy it
            if (found) {
                if (doOutNames)
                    psRetSecGroupNames[retCount] = strdup(pGni->secgroups[i].name);

                if (doOutStructs)
                    memcpy(&(pRetSecGroup[retCount]), &(pGni->secgroups[i]), sizeof(gni_secgroup));
                retCount++;
            }
        } else {
            if (!strcmp(psSecGroupNames[x], pGni->secgroups[i].name)) {
                // Check if this we have any instance using this group
                for (k = 0, found = FALSE; ((k < nbInstances) && !found); k++) {
                    for (x = 0; ((x < pInstances[k].max_secgroup_names) && !found); x++) {
                        if (!strcmp(pGni->secgroups[i].name, pInstances[k].secgroup_names[x].name)) {
                            found = TRUE;
                        }
                    }
                }

                // If we have any instance using this group, then copy it
                if (found) {
                    if (doOutNames) {
                        *psOutSecGroupNames = EUCA_REALLOC(*psOutSecGroupNames, (retCount + 1), sizeof(char *));
                        psRetSecGroupNames = *psOutSecGroupNames;
                        psRetSecGroupNames[retCount] = strdup(pGni->secgroups[i].name);
                    }

                    if (doOutStructs) {
                        *pOutSecGroups = EUCA_REALLOC(*pOutSecGroups, (retCount + 1), sizeof(gni_instance));
                        pRetSecGroup = *pOutSecGroups;
                        memcpy(&(pRetSecGroup[retCount]), &(pGni->secgroups[i]), sizeof(gni_secgroup));
                    }
                    retCount++;
                }
            }
        }
    }

    if (doOutNames)
        *pOutNbSecGroupNames = retCount;

    if (doOutStructs)
        *pOutNbSecGroups = retCount;

    EUCA_FREE(pInstances);
    return (ret);
}

//!
//! TODO: Function description.
//!
//! @param[in]  gni a pointer to the global network information structure
//! @param[in]  node
//! @param[in]  instance_names
//! @param[in]  max_instance_names
//! @param[out] out_instance_names
//! @param[out] out_max_instance_names
//! @param[out] out_instances
//! @param[out] out_max_instances
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_node_get_instances(globalNetworkInfo * gni, gni_node * node, char **instance_names, int max_instance_names, char ***out_instance_names, int *out_max_instance_names,
                           gni_instance ** out_instances, int *out_max_instances)
{
    int ret = 0, getall = 0, i = 0, j = 0, k = 0, retcount = 0, do_outnames = 0, do_outstructs = 0;
    gni_instance *ret_instances = NULL;
    char **ret_instance_names = NULL;

    if (!gni) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (out_instance_names && out_max_instance_names) {
        do_outnames = 1;
    }
    if (out_instances && out_max_instances) {
        do_outstructs = 1;
    }

    if (!do_outnames && !do_outstructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }

    if (do_outnames) {
        *out_instance_names = NULL;
        *out_max_instance_names = 0;
    }
    if (do_outstructs) {
        *out_instances = NULL;
        *out_max_instances = 0;
    }

    if (instance_names == NULL || !strcmp(instance_names[0], "*")) {
        getall = 1;
        if (do_outnames)
            *out_instance_names = EUCA_ZALLOC(node->max_instance_names, sizeof(char *));
        if (do_outstructs)
            *out_instances = EUCA_ZALLOC(node->max_instance_names, sizeof(gni_instance));
    }

    if (do_outnames)
        ret_instance_names = *out_instance_names;
    if (do_outstructs)
        ret_instances = *out_instances;

    retcount = 0;
    for (i = 0; i < node->max_instance_names; i++) {
        if (getall) {
            if (do_outnames)
                ret_instance_names[i] = strdup(node->instance_names[i].name);
            if (do_outstructs) {
                for (k = 0; k < gni->max_instances; k++) {
                    if (!strcmp(gni->instances[k].name, node->instance_names[i].name)) {
                        memcpy(&(ret_instances[i]), &(gni->instances[k]), sizeof(gni_instance));
                        break;
                    }
                }
            }
            retcount++;
        } else {
            for (j = 0; j < max_instance_names; j++) {
                if (!strcmp(instance_names[j], node->instance_names[i].name)) {
                    if (do_outnames) {
                        *out_instance_names = realloc(*out_instance_names, sizeof(char *) * (retcount + 1));
                        ret_instance_names = *out_instance_names;
                        ret_instance_names[retcount] = strdup(node->instance_names[i].name);
                    }
                    if (do_outstructs) {
                        for (k = 0; k < gni->max_instances; k++) {
                            if (!strcmp(gni->instances[k].name, node->instance_names[i].name)) {
                                *out_instances = realloc(*out_instances, sizeof(gni_instance) * (retcount + 1));
                                ret_instances = *out_instances;
                                memcpy(&(ret_instances[retcount]), &(gni->instances[k]), sizeof(gni_instance));
                                break;
                            }
                        }
                    }
                    retcount++;
                }
            }
        }
    }
    if (do_outnames)
        *out_max_instance_names = retcount;
    if (do_outstructs)
        *out_max_instances = retcount;

    return (ret);
}

//!
//! Retrives the list of security groups configured and active on a given cluster
//!
//! @param[in]  pGni a pointer to our global network view structure
//! @param[in]  pNode a pointer to the node we're building the security group list for
//! @param[in]  psSecGroupNames a string pointer to the name of groups we're looking for
//! @param[in]  nbSecGroupNames the number of groups in the psSecGroupNames list
//! @param[out] psOutSecGroupNames a string pointer that will contain the list of group names we found (if non NULL)
//! @param[out] pOutNbSecGroupNames a pointer to the number of groups that matched in the psOutSecGroupNames list
//! @param[out] pOutSecGroups a pointer to the list of security group structures that match what we're looking for
//! @param[out] pOutNbSecGroups a pointer to the number of structures in the psOutSecGroups list
//!
//! @return 0 on success or 1 if any failure occured
//!
//! @see
//!
//! @pre  TODO:
//!
//! @post TODO:
//!
//! @note
//!
int gni_node_get_secgroup(globalNetworkInfo * pGni, gni_node * pNode, char **psSecGroupNames, int nbSecGroupNames, char ***psOutSecGroupNames, int *pOutNbSecGroupNames,
                          gni_secgroup ** pOutSecGroups, int *pOutNbSecGroups)
{
    int ret = 0;
    int i = 0;
    int k = 0;
    int x = 0;
    int retCount = 0;
    int nbInstances = 0;
    char **psRetSecGroupNames = NULL;
    boolean found = FALSE;
    boolean getAll = FALSE;
    boolean doOutNames = FALSE;
    boolean doOutStructs = FALSE;
    gni_instance *pInstances = NULL;
    gni_secgroup *pRetSecGroup = NULL;

    // Make sure our GNI and Cluster pointers are valid
    if (!pGni || !pNode) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // We will need to get the instances that are running under this cluster
    if (gni_node_get_instances(pGni, pNode, NULL, 0, NULL, NULL, &pInstances, &nbInstances)) {
        LOGERROR("Failed to retrieve instances for node '%s'\n", pNode->name);
        return (1);
    }
    // Are we building the name list?
    if (psOutSecGroupNames && pOutNbSecGroupNames) {
        doOutNames = TRUE;
        *psOutSecGroupNames = NULL;
        *pOutNbSecGroupNames = 0;
    }
    // Are we building the structure list?
    if (pOutSecGroups && pOutNbSecGroups) {
        doOutStructs = TRUE;
        *pOutSecGroups = NULL;
        *pOutNbSecGroups = 0;
    }
    // Are we doing anything?
    if (!doOutNames && !doOutStructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        EUCA_FREE(pInstances);
        return (0);
    }
    // Do we have any instances?
    if (nbInstances == 0) {
        EUCA_FREE(pInstances);
        return (0);
    }
    // Do we have any groups?
    if (pGni->max_secgroups == 0) {
        EUCA_FREE(pInstances);
        return (0);
    }
    // Allocate memory for all the groups if there is no search criterias
    if ((psSecGroupNames == NULL) || !strcmp(psSecGroupNames[0], "*")) {
        getAll = TRUE;
        if (doOutNames)
            *psOutSecGroupNames = EUCA_ZALLOC(pGni->max_secgroups, sizeof(char *));

        if (doOutStructs)
            *pOutSecGroups = EUCA_ZALLOC(pGni->max_secgroups, sizeof(gni_secgroup));
    }
    // Setup our returning name pointer
    if (doOutNames)
        psRetSecGroupNames = *psOutSecGroupNames;

    // Setup our returning structure pointer
    if (doOutStructs)
        pRetSecGroup = *pOutSecGroups;

    // Scan all our groups
    for (i = 0, retCount = 0; i < pGni->max_secgroups; i++) {
        if (getAll) {
            // Check if this we have any instance using this group
            for (k = 0, found = FALSE; ((k < nbInstances) && !found); k++) {
                for (x = 0; ((x < pInstances[k].max_secgroup_names) && !found); x++) {
                    if (!strcmp(pGni->secgroups[i].name, pInstances[k].secgroup_names[x].name)) {
                        found = TRUE;
                    }
                }
            }

            // If we have any instance using this group, then copy it
            if (found) {
                if (doOutNames)
                    psRetSecGroupNames[retCount] = strdup(pGni->secgroups[i].name);

                if (doOutStructs)
                    memcpy(&(pRetSecGroup[retCount]), &(pGni->secgroups[i]), sizeof(gni_secgroup));
                retCount++;
            }
        } else {
            if (!strcmp(psSecGroupNames[x], pGni->secgroups[i].name)) {
                // Check if this we have any instance using this group
                for (k = 0, found = FALSE; ((k < nbInstances) && !found); k++) {
                    for (x = 0; ((x < pInstances[k].max_secgroup_names) && !found); x++) {
                        if (!strcmp(pGni->secgroups[i].name, pInstances[k].secgroup_names[x].name)) {
                            found = TRUE;
                        }
                    }
                }

                // If we have any instance using this group, then copy it
                if (found) {
                    if (doOutNames) {
                        *psOutSecGroupNames = EUCA_REALLOC(*psOutSecGroupNames, (retCount + 1), sizeof(char *));
                        psRetSecGroupNames = *psOutSecGroupNames;
                        psRetSecGroupNames[retCount] = strdup(pGni->secgroups[i].name);
                    }

                    if (doOutStructs) {
                        *pOutSecGroups = EUCA_REALLOC(*pOutSecGroups, (retCount + 1), sizeof(gni_instance));
                        pRetSecGroup = *pOutSecGroups;
                        memcpy(&(pRetSecGroup[retCount]), &(pGni->secgroups[i]), sizeof(gni_secgroup));
                    }
                    retCount++;
                }
            }
        }
    }

    if (doOutNames)
        *pOutNbSecGroupNames = retCount;

    if (doOutStructs)
        *pOutNbSecGroups = retCount;

    EUCA_FREE(pInstances);
    return (ret);
}

//!
//! TODO: Function description.
//!
//! @param[in]  gni a pointer to the global network information structure
//! @param[in]  instance
//! @param[in]  secgroup_names
//! @param[in]  max_secgroup_names
//! @param[out] out_secgroup_names
//! @param[out] out_max_secgroup_names
//! @param[out] out_secgroups
//! @param[out] out_max_secgroups
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_instance_get_secgroups(globalNetworkInfo * gni, gni_instance * instance, char **secgroup_names, int max_secgroup_names, char ***out_secgroup_names,
                               int *out_max_secgroup_names, gni_secgroup ** out_secgroups, int *out_max_secgroups)
{
    int ret = 0, getall = 0, i = 0, j = 0, k = 0, retcount = 0, do_outnames = 0, do_outstructs = 0;
    gni_secgroup *ret_secgroups = NULL;
    char **ret_secgroup_names = NULL;

    if (!gni || !instance) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (out_secgroup_names && out_max_secgroup_names) {
        do_outnames = 1;
    }
    if (out_secgroups && out_max_secgroups) {
        do_outstructs = 1;
    }

    if (!do_outnames && !do_outstructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }

    if (do_outnames) {
        *out_secgroup_names = NULL;
        *out_max_secgroup_names = 0;
    }
    if (do_outstructs) {
        *out_secgroups = NULL;
        *out_max_secgroups = 0;
    }

    if ((secgroup_names == NULL) || !strcmp(secgroup_names[0], "*")) {
        getall = 1;
        if (do_outnames)
            *out_secgroup_names = EUCA_ZALLOC(instance->max_secgroup_names, sizeof(char *));
        if (do_outstructs)
            *out_secgroups = EUCA_ZALLOC(instance->max_secgroup_names, sizeof(gni_secgroup));
    }

    if (do_outnames)
        ret_secgroup_names = *out_secgroup_names;
    if (do_outstructs)
        ret_secgroups = *out_secgroups;

    retcount = 0;
    for (i = 0; i < instance->max_secgroup_names; i++) {
        if (getall) {
            if (do_outnames)
                ret_secgroup_names[i] = strdup(instance->secgroup_names[i].name);
            if (do_outstructs) {
                for (k = 0; k < gni->max_secgroups; k++) {
                    if (!strcmp(gni->secgroups[k].name, instance->secgroup_names[i].name)) {
                        memcpy(&(ret_secgroups[i]), &(gni->secgroups[k]), sizeof(gni_secgroup));
                        break;
                    }
                }
            }
            retcount++;
        } else {
            for (j = 0; j < max_secgroup_names; j++) {
                if (!strcmp(secgroup_names[j], instance->secgroup_names[i].name)) {
                    if (do_outnames) {
                        *out_secgroup_names = realloc(*out_secgroup_names, sizeof(char *) * (retcount + 1));
                        ret_secgroup_names = *out_secgroup_names;
                        ret_secgroup_names[retcount] = strdup(instance->secgroup_names[i].name);
                    }
                    if (do_outstructs) {
                        for (k = 0; k < gni->max_secgroups; k++) {
                            if (!strcmp(gni->secgroups[k].name, instance->secgroup_names[i].name)) {
                                *out_secgroups = realloc(*out_secgroups, sizeof(gni_secgroup) * (retcount + 1));
                                ret_secgroups = *out_secgroups;
                                memcpy(&(ret_secgroups[retcount]), &(gni->secgroups[k]), sizeof(gni_secgroup));
                                break;
                            }
                        }
                    }
                    retcount++;
                }
            }
        }
    }
    if (do_outnames)
        *out_max_secgroup_names = retcount;
    if (do_outstructs)
        *out_max_secgroups = retcount;

    return (ret);

}

//!
//! TODO: Function description.
//!
//! @param[in]  gni a pointer to the global network information structure
//! @param[in]  secgroup
//! @param[in]  instance_names
//! @param[in]  max_instance_names
//! @param[out] out_instance_names
//! @param[out] out_max_instance_names
//! @param[out] out_instances
//! @param[out] out_max_instances
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_secgroup_get_instances(globalNetworkInfo * gni, gni_secgroup * secgroup, char **instance_names, int max_instance_names, char ***out_instance_names,
                               int *out_max_instance_names, gni_instance ** out_instances, int *out_max_instances)
{
    int ret = 0, getall = 0, i = 0, j = 0, k = 0, retcount = 0, do_outnames = 0, do_outstructs = 0;
    gni_instance *ret_instances = NULL;
    char **ret_instance_names = NULL;

    if (!gni || !secgroup) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (out_instance_names && out_max_instance_names) {
        do_outnames = 1;
    }
    if (out_instances && out_max_instances) {
        do_outstructs = 1;
    }

    if (!do_outnames && !do_outstructs) {
        LOGDEBUG("nothing to do, both output variables are NULL\n");
        return (0);
    }

    if (do_outnames) {
        *out_instance_names = NULL;
        *out_max_instance_names = 0;
    }
    if (do_outstructs) {
        *out_instances = NULL;
        *out_max_instances = 0;
    }

    if ((instance_names == NULL) || (!strcmp(instance_names[0], "*"))) {
        getall = 1;
        if (do_outnames)
            *out_instance_names = EUCA_ZALLOC(secgroup->max_instance_names, sizeof(char *));
        if (do_outstructs)
            *out_instances = EUCA_ZALLOC(secgroup->max_instance_names, sizeof(gni_instance));
    }

    if (do_outnames)
        ret_instance_names = *out_instance_names;
    if (do_outstructs)
        ret_instances = *out_instances;

    retcount = 0;
    for (i = 0; i < secgroup->max_instance_names; i++) {
        if (getall) {
            if (do_outnames)
                ret_instance_names[i] = strdup(secgroup->instance_names[i].name);
            if (do_outstructs) {
                for (k = 0; k < gni->max_instances; k++) {
                    if (!strcmp(gni->instances[k].name, secgroup->instance_names[i].name)) {
                        memcpy(&(ret_instances[i]), &(gni->instances[k]), sizeof(gni_instance));
                        break;
                    }
                }
            }
            retcount++;
        } else {
            for (j = 0; j < max_instance_names; j++) {
                if (!strcmp(instance_names[j], secgroup->instance_names[i].name)) {
                    if (do_outnames) {
                        *out_instance_names = realloc(*out_instance_names, sizeof(char *) * (retcount + 1));
                        ret_instance_names = *out_instance_names;
                        ret_instance_names[retcount] = strdup(secgroup->instance_names[i].name);
                    }
                    if (do_outstructs) {
                        for (k = 0; k < gni->max_instances; k++) {
                            if (!strcmp(gni->instances[k].name, secgroup->instance_names[i].name)) {
                                *out_instances = realloc(*out_instances, sizeof(gni_instance) * (retcount + 1));
                                ret_instances = *out_instances;
                                memcpy(&(ret_instances[retcount]), &(gni->instances[k]), sizeof(gni_instance));
                                break;
                            }
                        }
                    }
                    retcount++;
                }
            }
        }
    }
    if (do_outnames)
        *out_max_instance_names = retcount;
    if (do_outstructs)
        *out_max_instances = retcount;

    return (ret);
}

//!
//! Function description.
//!
//! @param[in]  ctxptr
//! @param[in]  expression
//! @param[out] results
//! @param[out] max_results
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int evaluate_xpath_property(xmlXPathContextPtr ctxptr, char *expression, char ***results, int *max_results)
{
    int i, max_nodes = 0, result_count = 0;
    xmlXPathObjectPtr objptr;
    char **retresults;

    *max_results = 0;

    objptr = xmlXPathEvalExpression((unsigned char *)expression, ctxptr);
    if (objptr == NULL) {
        LOGERROR("unable to evaluate xpath expression '%s': check network config XML format\n", expression);
        return (1);
    } else {
        if (objptr->nodesetval) {
            max_nodes = (int)objptr->nodesetval->nodeNr;
            *results = EUCA_ZALLOC(max_nodes, sizeof(char *));
            retresults = *results;
            for (i = 0; i < max_nodes; i++) {
                if (objptr->nodesetval->nodeTab[i] && objptr->nodesetval->nodeTab[i]->children && objptr->nodesetval->nodeTab[i]->children->content) {

                    retresults[result_count] = strdup((char *)objptr->nodesetval->nodeTab[i]->children->content);
                    result_count++;
                }
            }
            *max_results = result_count;

            LOGTRACE("%d results after evaluated expression %s\n", *max_results, expression);
            for (i = 0; i < *max_results; i++) {
                LOGTRACE("\tRESULT %d: %s\n", i, retresults[i]);
            }
        }
    }
    xmlXPathFreeObject(objptr);
    return (0);
}

//!
//! TODO: Describe
//!
//! @param[in]  ctxptr a pointer to the XML context
//! @param[in]  expression a string pointer to the expression we want to evaluate
//! @param[out] results the results of the expression evaluation
//! @param[out] max_results the number of results returned
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int evaluate_xpath_element(xmlXPathContextPtr ctxptr, char *expression, char ***results, int *max_results)
{
    int i, max_nodes = 0, result_count = 0;
    xmlXPathObjectPtr objptr;
    char **retresults;

    *max_results = 0;

    objptr = xmlXPathEvalExpression((unsigned char *)expression, ctxptr);
    if (objptr == NULL) {
        LOGERROR("unable to evaluate xpath expression '%s': check network config XML format\n", expression);
        return (1);
    } else {
        if (objptr->nodesetval) {
            max_nodes = (int)objptr->nodesetval->nodeNr;
            *results = EUCA_ZALLOC(max_nodes, sizeof(char *));
            retresults = *results;
            for (i = 0; i < max_nodes; i++) {
                if (objptr->nodesetval->nodeTab[i] && objptr->nodesetval->nodeTab[i]->properties && objptr->nodesetval->nodeTab[i]->properties->children
                    && objptr->nodesetval->nodeTab[i]->properties->children->content) {
                    retresults[result_count] = strdup((char *)objptr->nodesetval->nodeTab[i]->properties->children->content);
                    result_count++;
                }
            }
            *max_results = result_count;

            LOGTRACE("%d results after evaluated expression %s\n", *max_results, expression);
            for (i = 0; i < *max_results; i++) {
                LOGTRACE("\tRESULT %d: %s\n", i, retresults[i]);
            }
        }
    }
    xmlXPathFreeObject(objptr);
    return (0);
}

//!
//! Allocates an initialize a new globalNetworkInfo structure.
//!
//! @return A pointer to the newly allocated structure or NULL if any memory failure occured
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
globalNetworkInfo *gni_init()
{
    globalNetworkInfo *gni = NULL;
    gni = EUCA_ZALLOC(1, sizeof(globalNetworkInfo));
    if (!gni) {

    } else {
        bzero(gni, sizeof(globalNetworkInfo));
        gni->init = 1;
    }
    return (gni);
}

//!
//! Populates a given globalNetworkInfo structure from the content of an XML file
//!
//! @param[in] gni a pointer to the global network information structure
//! @param[in] xmlpath path the XML file use to populate the structure
//!
//! @return 0 on success or 1 on failure
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_populate(globalNetworkInfo * gni, gni_hostname_info *host_info, char *xmlpath)
{
    int rc;
    xmlDocPtr docptr;
    xmlXPathContextPtr ctxptr;
    char expression[2048], *strptra = NULL;
    char **results = NULL;
    int max_results = 0, i, j, k, l;
    gni_hostname *gni_hostname_ptr = NULL;
    int hostnames_need_reset = 0;

    if (!gni) {
        LOGERROR("invalid input\n");
        return (1);
    }

    gni_clear(gni);

    xmlInitParser();
    LIBXML_TEST_VERSION docptr = xmlParseFile(xmlpath);
    if (docptr == NULL) {
        LOGERROR("unable to parse XML file (%s)\n", xmlpath);
        return (1);
    }

    ctxptr = xmlXPathNewContext(docptr);
    if (ctxptr == NULL) {
        LOGERROR("unable to get new xml context\n");
        xmlFreeDoc(docptr);
        return (1);
    }

    LOGDEBUG("begin parsing XML into data structures\n");

    // begin instance
    snprintf(expression, 2048, "/network-data/instances/instance");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->instances = EUCA_ZALLOC(max_results, sizeof(gni_instance));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->instances[i].name, INSTANCE_ID_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_instances = max_results;
    EUCA_FREE(results);

    for (j = 0; j < gni->max_instances; j++) {
        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/ownerId", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->instances[j].accountId, 128, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/macAddress", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            mac2hex(results[i], gni->instances[j].macAddress);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/publicIp", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->instances[j].publicIp = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/privateIp", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->instances[j].privateIp = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/vpc", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->instances[j].vpc, 16, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/subnet", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->instances[j].subnet, 16, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/securityGroups/value", gni->instances[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        gni->instances[j].secgroup_names = EUCA_ZALLOC(max_results, sizeof(gni_name));
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->instances[j].secgroup_names[i].name, 1024, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        gni->instances[j].max_secgroup_names = max_results;
        EUCA_FREE(results);
    }

    // end instance, begin secgroup
    snprintf(expression, 2048, "/network-data/securityGroups/securityGroup");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->secgroups = EUCA_ZALLOC(max_results, sizeof(gni_secgroup));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->secgroups[i].name, SECURITY_GROUP_ID_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_secgroups = max_results;
    EUCA_FREE(results);

    for (j = 0; j < gni->max_secgroups; j++) {
        // populate secgroup's instance_names
        gni->secgroups[j].max_instance_names = 0;
        gni->secgroups[j].instance_names = EUCA_ZALLOC(gni->max_instances, sizeof(gni_name));
        for (k = 0; k < gni->max_instances; k++) {
            for (l = 0; l < gni->instances[k].max_secgroup_names; l++) {
                if (!strcmp(gni->instances[k].secgroup_names[l].name, gni->secgroups[j].name)) {
                    //      gni->secgroups[j].instance_names = realloc(gni->secgroups[j].instance_names, sizeof(gni_name) * (gni->secgroups[j].max_instance_names + 1));
                    snprintf(gni->secgroups[j].instance_names[gni->secgroups[j].max_instance_names].name, 1024, "%s", gni->instances[k].name);
                    gni->secgroups[j].max_instance_names++;
                }
            }
        }

        snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ownerId", gni->secgroups[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->secgroups[j].accountId, 128, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/rules/value", gni->secgroups[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        gni->secgroups[j].grouprules = EUCA_ZALLOC(max_results, sizeof(gni_name));
        for (i = 0; i < max_results; i++) {
            char newrule[2048];
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            rc = ruleconvert(results[i], newrule);
            if (!rc) {
                snprintf(gni->secgroups[j].grouprules[i].name, 1024, "%s", newrule);
            }
            EUCA_FREE(results[i]);
        }
        gni->secgroups[j].max_grouprules = max_results;
        EUCA_FREE(results);

        int found = 0;
        int count = 0;
        while (!found) {
            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/protocol", gni->secgroups[j].name, count + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            if (!max_results) {
                found = 1;
            } else {
                count++;
            }
            for (i = 0; i < max_results; i++) {
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);
        }

        gni->secgroups[j].ingress_rules = EUCA_ZALLOC(count, sizeof(gni_rule));
        gni->secgroups[j].max_ingress_rules = count;

        for (k = 0; k < gni->secgroups[j].max_ingress_rules; k++) {

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/protocol", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                gni->secgroups[j].ingress_rules[k].protocol = atoi(results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/groupId", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->secgroups[j].ingress_rules[k].groupId, SECURITY_GROUP_ID_LEN, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/groupOwnerId", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->secgroups[j].ingress_rules[k].groupOwnerId, 16, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/cidr", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->secgroups[j].ingress_rules[k].cidr, INET_ADDR_LEN, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/fromPort", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                gni->secgroups[j].ingress_rules[k].fromPort = atoi(results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/toPort", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                gni->secgroups[j].ingress_rules[k].toPort = atoi(results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/icmpType", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                gni->secgroups[j].ingress_rules[k].icmpType = atoi(results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ingressRules/rule[%d]/icmpCode", gni->secgroups[j].name, k + 1);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                gni->secgroups[j].ingress_rules[k].icmpCode = atoi(results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

        }

        /*  for (k=0; k<gni->secgroups[j].max_ingress_rules; k++) {
           LOGTRACE("HELLO: rule=%d %d %d %d %d %d %s %s %s\n", k, gni->secgroups[j].ingress_rules[k].protocol, gni->secgroups[j].ingress_rules[k].fromPort, gni->secgroups[j].ingress_rules[k].toPort, gni->secgroups[j].ingress_rules[k].icmpType, gni->secgroups[j].ingress_rules[k].icmpCode, gni->secgroups[j].ingress_rules[k].groupId, gni->secgroups[j].ingress_rules[k].groupOwnerId, gni->secgroups[j].ingress_rules[k].cidr);
           }
           exit(0);
         */
    }

    // begin VPC
    snprintf(expression, 2048, "/network-data/vpcs/vpc");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->vpcs = EUCA_ZALLOC(max_results, sizeof(gni_vpc));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->vpcs[i].name, 16, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_vpcs = max_results;
    EUCA_FREE(results);

    for (j = 0; j < gni->max_vpcs; j++) {
        snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/ownerId", gni->vpcs[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->vpcs[j].accountId, 128, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/cidr", gni->vpcs[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->vpcs[j].cidr, 24, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/dhcpOptionSet", gni->vpcs[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->vpcs[j].dhcpOptionSet, 16, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/subnets/*", gni->vpcs[j].name);
        rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);

        gni->vpcs[j].subnets = EUCA_ZALLOC(max_results, sizeof(gni_vpcsubnet));
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->vpcs[j].subnets[i].name, 16, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        gni->vpcs[j].max_subnets = max_results;
        EUCA_FREE(results);

        for (k = 0; k < gni->vpcs[j].max_subnets; k++) {
            snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/subnets/subnet[@name='%s']/ownerId", gni->vpcs[j].name, gni->vpcs[j].subnets[k].name);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->vpcs[j].subnets[k].accountId, 128, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/subnets/subnet[@name='%s']/cidr", gni->vpcs[j].name, gni->vpcs[j].subnets[k].name);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->vpcs[j].subnets[k].cidr, 24, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/subnets/subnet[@name='%s']/cluster", gni->vpcs[j].name, gni->vpcs[j].subnets[k].name);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->vpcs[j].subnets[k].cluster_name, HOSTNAME_LEN, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/subnets/subnet[@name='%s']/networkAcl", gni->vpcs[j].name, gni->vpcs[j].subnets[k].name);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->vpcs[j].subnets[k].networkAcl_name, 16, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

            snprintf(expression, 2048, "/network-data/vpcs/vpc[@name='%s']/subnets/subnet[@name='%s']/routeTable", gni->vpcs[j].name, gni->vpcs[j].subnets[k].name);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->vpcs[j].subnets[k].routeTable_name, 16, "%s", results[i]);
                EUCA_FREE(results[i]);
            }
            EUCA_FREE(results);

        }
        // TODO: networkAcls, routeTables, internetGateways

    }

    // begin configuration

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mode']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->sMode, NETMODE_LEN, results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='enabledCLCIp']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        gni->enabledCLCIp = dot2hex(results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='instanceDNSDomain']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->instanceDNSDomain, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

#ifdef USE_IP_ROUTE_HANDLER
    snprintf(expression, 2048, "/network-data/configuration/property[@name='publicGateway']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        gni->publicGateway = dot2hex(results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
#endif /* USE_IP_ROUTE_HANDLER */

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mido']/property[@name='eucanetdHost']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->EucanetdHost, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mido']/property[@name='gatewayHost']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->GatewayHost, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mido']/property[@name='gatewayIP']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->GatewayIP, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mido']/property[@name='gatewayInterface']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->GatewayInterface, 32, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mido']/property[@name='publicNetworkCidr']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->PublicNetworkCidr, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='mido']/property[@name='publicGatewayIP']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->PublicGatewayIP, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='instanceDNSServers']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    gni->instanceDNSServers = EUCA_ZALLOC(max_results, sizeof(u32));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        gni->instanceDNSServers[i] = dot2hex(results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_instanceDNSServers = max_results;
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/configuration/property[@name='publicIps']/value");
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);

    if (results && max_results) {
        rc = gni_serialize_iprange_list(results, max_results, &(gni->public_ips), &(gni->max_public_ips));
        //    gni->public_ips = EUCA_ZALLOC(max_results, sizeof(u32));
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            //        gni->public_ips[i] = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        //    gni->max_public_ips = max_results;
        EUCA_FREE(results);
    }
    // Do we have any managed subnets?
    snprintf(expression, 2048, "/network-data/configuration/property[@name='managedSubnet']/managedSubnet");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->managedSubnet = EUCA_ZALLOC(max_results, sizeof(gni_subnet));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        gni->managedSubnet[i].subnet = dot2hex(results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_managedSubnets = max_results;
    EUCA_FREE(results);

    // If we do have any managed subnets, retrieve the rest of the information
    for (j = 0; j < gni->max_managedSubnets; j++) {
        strptra = hex2dot(gni->managedSubnet[j].subnet);

        // Get the netmask
        snprintf(expression, 2048, "/network-data/configuration/property[@name='managedSubnet']/managedSubnet[@name='%s']/property[@name='netmask']/value", SP(strptra));
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->managedSubnet[j].netmask = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        // Now get the minimum VLAN index
        snprintf(expression, 2048, "/network-data/configuration/property[@name='managedSubnet']/managedSubnet[@name='%s']/property[@name='minVlan']/value", SP(strptra));
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->managedSubnet[j].minVlan = atoi(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        // Now get the maximum VLAN index
        snprintf(expression, 2048, "/network-data/configuration/property[@name='managedSubnet']/managedSubnet[@name='%s']/property[@name='maxVlan']/value", SP(strptra));
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->managedSubnet[j].maxVlan = atoi(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        // Now get the minimum VLAN index
        snprintf(expression, 2048, "/network-data/configuration/property[@name='managedSubnet']/managedSubnet[@name='%s']/property[@name='segmentSize']/value", SP(strptra));
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->managedSubnet[j].segmentSize = atoi(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);
        EUCA_FREE(strptra);
    }

    snprintf(expression, 2048, "/network-data/configuration/property[@name='subnets']/subnet");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->subnets = EUCA_ZALLOC(max_results, sizeof(gni_subnet));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        gni->subnets[i].subnet = dot2hex(results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_subnets = max_results;
    EUCA_FREE(results);

    for (j = 0; j < gni->max_subnets; j++) {
        strptra = hex2dot(gni->subnets[j].subnet);
        snprintf(expression, 2048, "/network-data/configuration/property[@name='subnets']/subnet[@name='%s']/property[@name='netmask']/value", SP(strptra));
        EUCA_FREE(strptra);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->subnets[j].netmask = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        strptra = hex2dot(gni->subnets[j].subnet);
        snprintf(expression, 2048, "/network-data/configuration/property[@name='subnets']/subnet[@name='%s']/property[@name='gateway']/value", SP(strptra));
        EUCA_FREE(strptra);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->subnets[j].gateway = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);
    }

    // Process Node Controller names to populate the ip->hostname 'cache'
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/*/property[@name='nodes']/node");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    LOGTRACE("Found %d Nodes in the config\n", max_results);

    //
    // Create a temp list so we can detect if we need to refresh the cached names or not.
    //
    gni_hostname_ptr = EUCA_ZALLOC(max_results, sizeof(gni_hostname));

    for (i = 0; i < max_results; i++) {
	LOGTRACE("after function: %d: %s\n", i, results[i]);
	{
	    struct hostent *hent;
	    struct in_addr addr;
	    char *tmp_hostname;

	    if ( inet_aton(results[i],&addr) ) {
		gni_hostname_ptr[i].ip_address.s_addr = addr.s_addr;

		rc = gni_hostnames_get_hostname(host_info,results[i], &tmp_hostname);
		if (rc) {
		    hostnames_need_reset = 1;
		    if ((hent = gethostbyaddr((char *)&(addr.s_addr), sizeof(addr.s_addr), AF_INET))) {
			LOGTRACE("Found hostname via reverse lookup: %s\n",hent->h_name);
			snprintf(gni_hostname_ptr[i].hostname,HOSTNAME_SIZE, "%s", hent->h_name);
		    } else {
			LOGTRACE("Hostname not found for ip: %s using name: %s\n",results[i],results[i]);
			snprintf(gni_hostname_ptr[i].hostname,HOSTNAME_SIZE, "%s", results[i]);
		    }
		} else {
		    LOGTRACE("Found cached hostname storing: %s\n", tmp_hostname);
		    snprintf(gni_hostname_ptr[i].hostname,HOSTNAME_SIZE, "%s", tmp_hostname); // store the name
		    EUCA_FREE(tmp_hostname);
		}
	    }
	}
	EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    //
    // If we've added an entry that didn't exist previously, we need to set a new
    // hostname cache and free up the orignal, then re-sort.
    //
    if (hostnames_need_reset) {
        LOGTRACE("Hostname cache reset needed\n");

        EUCA_FREE(host_info->hostnames);
        host_info->hostnames = gni_hostname_ptr;
        host_info->max_hostnames = max_results;

        qsort(host_info->hostnames,host_info->max_hostnames, sizeof(gni_hostname),cmpipaddr);
        hostnames_need_reset = 0;
    } else {
        LOGTRACE("No hostname cache change, freeing up temp cache\n");
        EUCA_FREE(gni_hostname_ptr);
    }

    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster");
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->clusters = EUCA_ZALLOC(max_results, sizeof(gni_cluster));
    for (i = 0; i < max_results; i++) {
        LOGTRACE("after function: %d: %s\n", i, results[i]);
        snprintf(gni->clusters[i].name, HOSTNAME_LEN, "%s", results[i]);
        EUCA_FREE(results[i]);
    }
    gni->max_clusters = max_results;
    EUCA_FREE(results);

    for (j = 0; j < gni->max_clusters; j++) {

        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='enabledCCIp']/value", gni->clusters[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->clusters[j].enabledCCIp = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='macPrefix']/value", gni->clusters[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->clusters[j].macPrefix, ENET_MACPREFIX_LEN, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='privateIps']/value", gni->clusters[j].name);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        if (results && max_results) {
            rc = gni_serialize_iprange_list(results, max_results, &(gni->clusters[j].private_ips), &(gni->clusters[j].max_private_ips));
            //        gni->clusters[j].private_ips = EUCA_ZALLOC(max_results, sizeof(u32));
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                //            gni->clusters[j].private_ips[i] = dot2hex(results[i]);
                EUCA_FREE(results[i]);
            }
            //        gni->clusters[j].max_private_ips = max_results;
            EUCA_FREE(results);
        }

        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/subnet", gni->clusters[j].name);
        rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->clusters[j].private_subnet.subnet = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        strptra = hex2dot(gni->clusters[j].private_subnet.subnet);
        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/subnet[@name='%s']/property[@name='netmask']/value",
                 gni->clusters[j].name, SP(strptra));
        EUCA_FREE(strptra);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->clusters[j].private_subnet.netmask = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        strptra = hex2dot(gni->clusters[j].private_subnet.subnet);
        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/subnet[@name='%s']/property[@name='gateway']/value",
                 gni->clusters[j].name, SP(strptra));
        EUCA_FREE(strptra);
        rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            gni->clusters[j].private_subnet.gateway = dot2hex(results[i]);
            EUCA_FREE(results[i]);
        }
        EUCA_FREE(results);

        snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node", gni->clusters[j].name);
        rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
        gni->clusters[j].nodes = EUCA_ZALLOC(max_results, sizeof(gni_node));
        for (i = 0; i < max_results; i++) {
            LOGTRACE("after function: %d: %s\n", i, results[i]);
            snprintf(gni->clusters[j].nodes[i].name, HOSTNAME_LEN, "%s", results[i]);
            EUCA_FREE(results[i]);
        }
        gni->clusters[j].max_nodes = max_results;
        EUCA_FREE(results);

        for (k = 0; k < gni->clusters[j].max_nodes; k++) {

            snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node[@name='%s']/instanceIds/value",
                     gni->clusters[j].name, gni->clusters[j].nodes[k].name);
            rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
            gni->clusters[j].nodes[k].instance_names = EUCA_ZALLOC(max_results, sizeof(gni_name));
            for (i = 0; i < max_results; i++) {
                LOGTRACE("after function: %d: %s\n", i, results[i]);
                snprintf(gni->clusters[j].nodes[k].instance_names[i].name, 1024, "%s", results[i]);
                EUCA_FREE(results[i]);

                for (l = 0; l < gni->max_instances; l++) {
                    if (!strcmp(gni->instances[l].name, gni->clusters[j].nodes[k].instance_names[i].name)) {
                        snprintf(gni->instances[l].node, HOSTNAME_LEN, "%s", gni->clusters[j].nodes[k].name);
                        {
                            char *hostname = NULL;

                            rc = gni_hostnames_get_hostname(host_info,gni->instances[l].node, &hostname);
                            if (rc) {
                                LOGTRACE("Failed to find cached hostname for IP: %s\n",gni->instances[l].node);
                                snprintf(gni->instances[l].nodehostname, HOSTNAME_SIZE, "%s", gni->instances[l].node);
                            } else {
                                LOGTRACE("Found cached hostname: %s for IP: %s\n",hostname,gni->instances[l].node);
                                snprintf(gni->instances[l].nodehostname, HOSTNAME_SIZE, "%s", hostname);
                                EUCA_FREE(hostname);
                            }
                        }
                    }
                }

            }
            gni->clusters[j].nodes[k].max_instance_names = max_results;
            EUCA_FREE(results);

        }
    }

    xmlXPathFreeContext(ctxptr);
    xmlFreeDoc(docptr);
    xmlCleanupParser();

    LOGDEBUG("end parsing XML into data structures\n");

    rc = gni_validate(gni);
    if (rc) {
        LOGERROR("could not validate GNI after XML parse: check network config\n");
        return (1);
    }

    return (0);
}

//!
//! TODO: Describe
//!
//! @param[in]  inlist
//! @param[in]  inmax
//! @param[out] outlist
//! @param[out] outmax
//!
//! @return 0 on success or 1 on failure
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_serialize_iprange_list(char **inlist, int inmax, u32 ** outlist, int *outmax)
{
    int i = 0;
    int ret = 0;
    int outidx = 0;
    u32 *outlistbuf = NULL;
    int max_outlistbuf = 0;

    if (!inlist || inmax < 0 || !outlist || !outmax) {
        LOGERROR("invalid input\n");
        return (1);
    }
    *outlist = NULL;
    *outmax = 0;

    for (i = 0; i < inmax; i++) {
        char *range = NULL;
        char *tok = NULL;
        char *start = NULL;
        char *end = NULL;
        int numi = 0;

        LOGTRACE("parsing input range: %s\n", inlist[i]);

        range = strdup(inlist[i]);
        tok = strchr(range, '-');
        if (tok) {
            *tok = '\0';
            tok++;
            if (strlen(tok)) {
                start = strdup(range);
                end = strdup(tok);
            } else {
                LOGERROR("empty end range from input '%s': check network config\n", inlist[i]);
                start = NULL;
                end = NULL;
            }
        } else {
            start = strdup(range);
            end = strdup(range);
        }
        EUCA_FREE(range);

        if (start && end) {
            uint32_t startb, endb, idxb, localhost;

            LOGTRACE("start=%s end=%s\n", start, end);
            localhost = dot2hex("127.0.0.1");
            startb = dot2hex(start);
            endb = dot2hex(end);
            if ((startb <= endb) && (startb != localhost) && (endb != localhost)) {
                numi = (int)(endb - startb) + 1;
                outlistbuf = realloc(outlistbuf, sizeof(u32) * (max_outlistbuf + numi));
                outidx = max_outlistbuf;
                max_outlistbuf += numi;
                for (idxb = startb; idxb <= endb; idxb++) {
                    outlistbuf[outidx] = idxb;
                    outidx++;
                }
            } else {
                LOGERROR("end range '%s' is smaller than start range '%s' from input '%s': check network config\n", end, start, inlist[i]);
                ret = 1;
            }
        } else {
            LOGERROR("couldn't parse range from input '%s': check network config\n", inlist[i]);
            ret = 1;
        }

        EUCA_FREE(start);
        EUCA_FREE(end);
    }

    if (max_outlistbuf > 0) {
        *outmax = max_outlistbuf;
        *outlist = malloc(sizeof(u32) * *outmax);
        memcpy(*outlist, outlistbuf, sizeof(u32) * max_outlistbuf);
    }
    EUCA_FREE(outlistbuf);

    return (ret);
}

//!
//! Iterates through a given globalNetworkInfo structure and execute the
//! given operation mode.
//!
//! @param[in] gni a pointer to the global network information structure
//! @param[in] mode the iteration mode: GNI_ITERATE_PRINT or GNI_ITERATE_FREE
//!
//! @return Always return 0
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_iterate(globalNetworkInfo * gni, int mode)
{
    int i, j;
    char *strptra = NULL;

    strptra = hex2dot(gni->enabledCLCIp);
    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("enabledCLCIp: %s\n", SP(strptra));
    EUCA_FREE(strptra);

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("instanceDNSDomain: %s\n", gni->instanceDNSDomain);

#ifdef USE_IP_ROUTE_HANDLER
    if (mode == GNI_ITERATE_PRINT) {
        strptra = hex2dot(gni->publicGateway);
        LOGTRACE("publicGateway: %s\n", SP(strptra));
        EUCA_FREE(strptra);
    }
#endif /* USE_IP_ROUTE_HANDLER */

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("instanceDNSServers: \n");
    for (i = 0; i < gni->max_instanceDNSServers; i++) {
        strptra = hex2dot(gni->instanceDNSServers[i]);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\tdnsServer %d: %s\n", i, SP(strptra));
        EUCA_FREE(strptra);
    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->instanceDNSServers);
    }

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("publicIps: \n");
    for (i = 0; i < gni->max_public_ips; i++) {
        strptra = hex2dot(gni->public_ips[i]);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\tip %d: %s\n", i, SP(strptra));
        EUCA_FREE(strptra);
    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->public_ips);
    }

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("subnets: \n");
    for (i = 0; i < gni->max_subnets; i++) {

        strptra = hex2dot(gni->subnets[i].subnet);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\tsubnet %d: %s\n", i, SP(strptra));
        EUCA_FREE(strptra);

        strptra = hex2dot(gni->subnets[i].netmask);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tnetmask: %s\n", SP(strptra));
        EUCA_FREE(strptra);

        strptra = hex2dot(gni->subnets[i].gateway);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tgateway: %s\n", SP(strptra));
        EUCA_FREE(strptra);

    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->subnets);
    }

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("clusters: \n");
    for (i = 0; i < gni->max_clusters; i++) {
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\tcluster %d: %s\n", i, gni->clusters[i].name);
        strptra = hex2dot(gni->clusters[i].enabledCCIp);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tenabledCCIp: %s\n", SP(strptra));
        EUCA_FREE(strptra);

        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tmacPrefix: %s\n", gni->clusters[i].macPrefix);

        strptra = hex2dot(gni->clusters[i].private_subnet.subnet);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tsubnet: %s\n", SP(strptra));
        EUCA_FREE(strptra);

        strptra = hex2dot(gni->clusters[i].private_subnet.netmask);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\t\tnetmask: %s\n", SP(strptra));
        EUCA_FREE(strptra);

        strptra = hex2dot(gni->clusters[i].private_subnet.gateway);
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\t\tgateway: %s\n", SP(strptra));
        EUCA_FREE(strptra);

        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tprivate_ips \n");
        for (j = 0; j < gni->clusters[i].max_private_ips; j++) {
            strptra = hex2dot(gni->clusters[i].private_ips[j]);
            if (mode == GNI_ITERATE_PRINT)
                LOGTRACE("\t\t\tip %d: %s\n", j, SP(strptra));
            EUCA_FREE(strptra);
        }
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\t\tnodes \n");
        for (j = 0; j < gni->clusters[i].max_nodes; j++) {
            if (mode == GNI_ITERATE_PRINT)
                LOGTRACE("\t\t\tnode %d: %s\n", j, gni->clusters[i].nodes[j].name);
            if (mode == GNI_ITERATE_FREE) {
                gni_node_clear(&(gni->clusters[i].nodes[j]));
            }
        }
        if (mode == GNI_ITERATE_FREE) {
            EUCA_FREE(gni->clusters[i].nodes);
            gni_cluster_clear(&(gni->clusters[i]));
        }
    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->clusters);
    }

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("instances: \n");
    for (i = 0; i < gni->max_instances; i++) {
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\tid: %s\n", gni->instances[i].name);
        if (mode == GNI_ITERATE_FREE) {
            gni_instance_clear(&(gni->instances[i]));
        }
    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->instances);
    }

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("secgroups: \n");
    for (i = 0; i < gni->max_secgroups; i++) {
        if (mode == GNI_ITERATE_PRINT)
            LOGTRACE("\tname: %s\n", gni->secgroups[i].name);
        if (mode == GNI_ITERATE_FREE) {
            gni_secgroup_clear(&(gni->secgroups[i]));
        }
    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->secgroups);
    }

    if (mode == GNI_ITERATE_PRINT)
        LOGTRACE("vpcs: \n");
    for (i = 0; i < gni->max_vpcs; i++) {
        if (mode == GNI_ITERATE_PRINT) {
            LOGTRACE("\tname: %s\n", gni->vpcs[i].name);
            LOGTRACE("\taccountId: %s\n", gni->vpcs[i].accountId);
        }
        if (mode == GNI_ITERATE_FREE) {
            gni_vpc_clear(&(gni->vpcs[i]));
        }
    }
    if (mode == GNI_ITERATE_FREE) {
        EUCA_FREE(gni->vpcs);
    }

    if (mode == GNI_ITERATE_FREE) {
        bzero(gni, sizeof(globalNetworkInfo));
        gni->init = 1;
    }

    return (0);
}

//!
//! Clears a given globalNetworkInfo structure. This will free member's allocated memory and zero
//! out the structure itself.
//!
//! @param[in] gni a pointer to the global network information structure
//!
//! @return the result of the gni_iterate() call
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_clear(globalNetworkInfo * gni)
{
    return (gni_iterate(gni, GNI_ITERATE_FREE));
}

//!
//! Logs the content of a given globalNetworkInfo structure
//!
//! @param[in] gni a pointer to the global network information structure
//!
//! @return the result of the gni_iterate() call
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_print(globalNetworkInfo * gni)
{
    return (gni_iterate(gni, GNI_ITERATE_PRINT));
}

//!
//! Clears and free a given globalNetworkInfo structure.
//!
//! @param[in] gni a pointer to the global network information structure
//!
//! @return Always return 0
//!
//! @see gni_clear()
//!
//! @pre
//!
//! @post
//!
//! @note The caller should free the given pointer
//!
int gni_free(globalNetworkInfo * gni)
{
    if (!gni) {
        return (0);
    }
    gni_clear(gni);
    EUCA_FREE(gni);
    return (0);
}

//!
//! TODO: Define
//!
//! @param[in]  rulebuf a string containing the IP table rule to convert
//! @param[out] outrule a string containing the converted rule
//!
//! @return 0 on success or 1 on failure.
//!
//! @see
//!
//! @pre Both rulebuf and outrule MUST not be NULL
//!
//! @post \li uppon success the outrule contains the converted value
//!       \li uppon failure, outrule does not contain any valid data
//!       \li regardless of success or failure case, rulebuf will be modified by a strtok_r() call
//!
//! @note
//!
int ruleconvert(char *rulebuf, char *outrule)
{
    int ret = 0;
    char proto[64], portrange[64], sourcecidr[64], icmptyperange[64], sourceowner[64], sourcegroup[64], newrule[4097], buf[2048];
    char *ptra = NULL, *toka = NULL, *idx = NULL;

    proto[0] = portrange[0] = sourcecidr[0] = icmptyperange[0] = newrule[0] = sourceowner[0] = sourcegroup[0] = '\0';

    if ((idx = strchr(rulebuf, '\n'))) {
        *idx = '\0';
    }

    toka = strtok_r(rulebuf, " ", &ptra);
    while (toka) {
        if (!strcmp(toka, "-P")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(proto, 64, "%s", toka);
        } else if (!strcmp(toka, "-p")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(portrange, 64, "%s", toka);
            if ((idx = strchr(portrange, '-'))) {
                char minport[64], maxport[64];
                sscanf(portrange, "%[0-9]-%[0-9]", minport, maxport);
                if (!strcmp(minport, maxport)) {
                    snprintf(portrange, 64, "%s", minport);
                } else {
                    *idx = ':';
                }
            }
        } else if (!strcmp(toka, "-s")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(sourcecidr, 64, "%s", toka);
            if (!strcmp(sourcecidr, "0.0.0.0/0")) {
                sourcecidr[0] = '\0';
            }
        } else if (!strcmp(toka, "-t")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(icmptyperange, 64, "any");
        } else if (!strcmp(toka, "-o")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(sourcegroup, 64, toka);
        } else if (!strcmp(toka, "-u")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(sourceowner, 64, toka);
        }
        toka = strtok_r(NULL, " ", &ptra);
    }

    LOGTRACE("TOKENIZED RULE: PROTO: %s PORTRANGE: %s SOURCECIDR: %s ICMPTYPERANGE: %s SOURCEOWNER: %s SOURCEGROUP: %s\n", proto, portrange, sourcecidr, icmptyperange, sourceowner,
             sourcegroup);

    // check if enough info is present to construct rule
    if (strlen(proto) && (strlen(portrange) || strlen(icmptyperange))) {
        if (strlen(sourcecidr)) {
            snprintf(buf, 2048, "-s %s ", sourcecidr);
            strncat(newrule, buf, 2048);
        }

        if (strlen(proto)) {
            snprintf(buf, 2048, "-p %s -m %s ", proto, proto);
            strncat(newrule, buf, 2048);
        }

        if (strlen(portrange)) {
            snprintf(buf, 2048, "--dport %s ", portrange);
            strncat(newrule, buf, 2048);
        }

        if (strlen(icmptyperange)) {
            snprintf(buf, 2048, "--icmp-type %s ", icmptyperange);
            strncat(newrule, buf, 2048);
        }

        while (newrule[strlen(newrule) - 1] == ' ') {
            newrule[strlen(newrule) - 1] = '\0';
        }

        snprintf(outrule, 2048, "%s", newrule);
        LOGTRACE("CONVERTED RULE: %s\n", outrule);
    } else {
        LOGWARN("not enough information in source rule to construct iptables rule: skipping\n");
        ret = 1;
    }

    return (ret);
}

//!
//! Clears a gni_cluster structure. This will free member's allocated memory and zero
//! out the structure itself.
//!
//! @param[in] cluster a pointer to the structure to clear
//!
//! @return This function always returns 0
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_cluster_clear(gni_cluster * cluster)
{
    if (!cluster) {
        return (0);
    }

    EUCA_FREE(cluster->private_ips);

    bzero(cluster, sizeof(gni_cluster));

    return (0);
}

//!
//! Clears a gni_node structure. This will free member's allocated memory and zero
//! out the structure itself.
//!
//! @param[in] node a pointer to the structure to clear
//!
//! @return This function always returns 0
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_node_clear(gni_node * node)
{
    if (!node) {
        return (0);
    }

    EUCA_FREE(node->instance_names);

    bzero(node, sizeof(gni_node));

    return (0);
}

//!
//! Clears a gni_instance structure. This will free member's allocated memory and zero
//! out the structure itself.
//!
//! @param[in] instance a pointer to the structure to clear
//!
//! @return This function always returns 0
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_instance_clear(gni_instance * instance)
{
    if (!instance) {
        return (0);
    }

    EUCA_FREE(instance->secgroup_names);

    bzero(instance, sizeof(gni_instance));

    return (0);
}

//!
//! Clears a gni_secgroup structure. This will free member's allocated memory and zero
//! out the structure itself.
//!
//! @param[in] secgroup a pointer to the structure to clear
//!
//! @return This function always returns 0
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_secgroup_clear(gni_secgroup * secgroup)
{
    if (!secgroup) {
        return (0);
    }

    EUCA_FREE(secgroup->ingress_rules);
    EUCA_FREE(secgroup->egress_rules);
    EUCA_FREE(secgroup->grouprules);
    EUCA_FREE(secgroup->instance_names);

    bzero(secgroup, sizeof(gni_secgroup));

    return (0);
}

//!
//! Zero out a VPC structure
//!
//! @param[in] vpc a pointer to the GNI VPC structure to reset
//!
//! @return Always return 0
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_vpc_clear(gni_vpc * vpc)
{
    if (!vpc) {
        return (0);
    }

    EUCA_FREE(vpc->subnets);
    EUCA_FREE(vpc->networkAcls);
    EUCA_FREE(vpc->routeTables);
    EUCA_FREE(vpc->internetGateways);

    bzero(vpc, sizeof(gni_vpc));

    return (0);
}

//!
//! Validates a given globalNetworkInfo structure and its content
//!
//! @param[in] gni a pointer to the Global Network Information structure to validate
//!
//! @return 0 if the structure is valid or 1 if it isn't
//!
//! @see gni_subnet_validate(), gni_cluster_validate(), gni_instance_validate(), gni_secgroup_validate()
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_validate(globalNetworkInfo * gni)
{
    int i = 0;

    // this is going to be messy, until we get XML validation in place
    if (!gni) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // GNI should be initialized... but check just in case
    if (!gni->init) {
        LOGWARN("BUG: gni is not initialized yet\n");
        return (1);
    }
    // Make sure we have a valid mode
    if (gni_netmode_validate(gni->sMode)) {
        LOGWARN("Invalid network mode provided: cannot validate XML\n");
        return (1);
    }

    LOGDEBUG("Validating XML for '%s' networking mode.\n", gni->sMode);

    // We need to know about which CLC is the enabled one. 0.0.0.0 means we don't know
    if (gni->enabledCLCIp == 0) {
        LOGWARN("no enabled CLC IP set: cannot validate XML\n");
        return (1);
    }
    // We should have some instance Domain Name information
    if (!strlen(gni->instanceDNSDomain)) {
        LOGWARN("no instanceDNSDomain set: cannot validate XML\n");
        return (1);
    }
    // We should have some instance Domain Name Server information
    if (!gni->max_instanceDNSServers || !gni->instanceDNSServers) {
        LOGWARN("no instanceDNSServers set: cannot validate XML\n");
        return (1);
    }
    // Validate that we don't have a corrupted list. All 0.0.0.0 addresses are invalid
    for (i = 0; i < gni->max_instanceDNSServers; i++) {
        if (gni->instanceDNSServers[i] == 0) {
            LOGWARN("empty instanceDNSServer set at idx %d: cannot validate XML\n", i);
            return (1);
        }
    }
    // We should have some public IPs set if not, we'll just warn the user
    if (!gni->max_public_ips || !gni->public_ips) {
        LOGTRACE("no public_ips set: cannot validate XML\n");
    } else {
        // Make sure none of the public IPs is 0.0.0.0
        for (i = 0; i < gni->max_public_ips; i++) {
            if (gni->public_ips[i] == 0) {
                LOGWARN("empty public_ip set at idx %d: cannot validate XML\n", i);
                return (1);
            }
        }
    }

    // Now we have different behavior between managed and managed-novlan
    if (!strcmp(gni->sMode, NETMODE_MANAGED) || !strcmp(gni->sMode, NETMODE_MANAGED_NOVLAN)) {
        // We must have 1 managed subnet declaration
        if ((gni->max_managedSubnets != 1) || !gni->subnets) {
            LOGWARN("invalid number of managed subnets set '%d'.\n", gni->max_managedSubnets);
            return (1);
        }
        // Validate our managed subnet
        if (gni_managed_subnet_validate(gni->managedSubnet)) {
            LOGWARN("invalid managed subnet: cannot validate XML\n");
            return (1);
        }
        // Validate the clusters
        if (!gni->max_clusters || !gni->clusters) {
            LOGTRACE("no clusters set\n");
        } else {
            for (i = 0; i < gni->max_clusters; i++) {
                if (gni_cluster_validate(&(gni->clusters[i]), TRUE)) {
                    LOGWARN("invalid clusters set at idx %d: cannot validate XML\n", i);
                    return (1);
                }
            }
        }
    } else {
        //
        // This is for the EDGE case. We should have a valid list of subnets and our clusters
        // should be valid for an EDGE mode
        //
        if (!gni->max_subnets || !gni->subnets) {
            LOGTRACE("no subnets set\n");
        } else {
            for (i = 0; i < gni->max_subnets; i++) {
                if (gni_subnet_validate(&(gni->subnets[i]))) {
                    LOGWARN("invalid subnets set at idx %d: cannot validate XML\n", i);
                    return (1);
                }
            }
        }

        // Validate the clusters
        if (!gni->max_clusters || !gni->clusters) {
            LOGTRACE("no clusters set\n");
        } else {
            for (i = 0; i < gni->max_clusters; i++) {
                if (gni_cluster_validate(&(gni->clusters[i]), FALSE)) {
                    LOGWARN("invalid clusters set at idx %d: cannot validate XML\n", i);
                    return (1);
                }
            }
        }
    }

    // If we have any instance provided, validate them
    if (!gni->max_instances || !gni->instances) {
        LOGTRACE("no instances set\n");
    } else {
        for (i = 0; i < gni->max_instances; i++) {
            if (gni_instance_validate(&(gni->instances[i]))) {
                LOGWARN("invalid instances set at idx %d: cannot validate XML\n", i);
                return (1);
            }
        }
    }

    // If we have any security group provided, we should be able to validate them
    if (!gni->max_secgroups || !gni->secgroups) {
        LOGTRACE("no secgroups set\n");
    } else {
        for (i = 0; i < gni->max_secgroups; i++) {
            if (gni_secgroup_validate(&(gni->secgroups[i]))) {
                LOGWARN("invalid secgroups set at idx %d: cannot validate XML\n", i);
                return (1);
            }
        }
    }

    return (0);
}

//!
//! Validate a networking mode provided in the GNI message. The only supported networking
//! mode strings are: EDGE, MANAGED and MANAGED-NOVLAN
//!
//! @param[in] psMode a string pointer to the network mode to validate
//!
//! @return 0 if the mode is valid or 1 if the mode isn't
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_netmode_validate(const char *psMode)
{
    int i = 0;

    //
    // In the globalNetworkInfo structure, the mode is a static string. But just in case
    // some bozo passed us a NULL pointer.
    //
    if (!psMode) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // Do we know anything about this mode?
    for (i = 0; asNetModes[i] != NULL; i++) {
        if (!strcmp(psMode, asNetModes[i])) {
            return (0);
        }
    }

    // Nope, we don't know jack shit
    LOGWARN("invalid network mode '%s'\n", psMode);
    return (1);
}

//!
//! Validate a gni_subnet structure content
//!
//! @param[in] pSubnet a pointer to the subnet structure to validate
//!
//! @return 0 if the structure is valid or 1 if the structure isn't
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_subnet_validate(gni_subnet * pSubnet)
{
    if (!pSubnet) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // If any of the subnet, netmask or gateway is 0.0.0.0, this is invalid
    if ((pSubnet->subnet == 0) || (pSubnet->netmask == 0) || (pSubnet->gateway == 0)) {
        LOGWARN("invalid subnet: subnet=%d netmask=%d gateway=%d\n", pSubnet->subnet, pSubnet->netmask, pSubnet->gateway);
        return (1);
    }

    return (0);
}

//!
//! Validate a gni_subnet structure content
//!
//! @param[in] pSubnet a pointer to the subnet structure to validate
//!
//! @return 0 if the structure is valid or 1 if the structure isn't
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_managed_subnet_validate(gni_managedsubnet * pSubnet)
{
    // Make sure we didn't get a NULL pointer
    if (!pSubnet) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // If any of the subnet or netmask is 0.0.0.0, this is invalid
    if ((pSubnet->subnet == 0) || (pSubnet->netmask == 0)) {
        LOGWARN("invalid managed subnet: subnet=%d netmask=%d\n", pSubnet->subnet, pSubnet->netmask);
        return (1);
    }
    // If the segment size is less than 16 or not a power of 2 than we have a problem
    if ((pSubnet->segmentSize < 16) || ((pSubnet->segmentSize & (pSubnet->segmentSize - 1)) != 0)) {
        LOGWARN("invalid managed subnet: segmentSize=%d\n", pSubnet->segmentSize);
        return (1);
    }
    // If minVlan is less than MIN_VLAN_EUCA or greater than MAX_VLAN_EUCA, we have a problem
    if ((pSubnet->minVlan < MIN_VLAN_EUCA) || (pSubnet->minVlan > MAX_VLAN_EUCA)) {
        LOGWARN("invalid managed subnet: minVlan=%d\n", pSubnet->minVlan);
        return (1);
    }
    // If maxVlan is less than MIN_VLAN_EUCA or greater than MAX_VLAN_EUCA, we have a problem
    if ((pSubnet->maxVlan < MIN_VLAN_EUCA) || (pSubnet->maxVlan > MAX_VLAN_EUCA)) {
        LOGWARN("invalid managed subnet: maxVlan=%d\n", pSubnet->maxVlan);
        return (1);
    }
    // If minVlan is greater than maxVlan, we have a problem too!!
    if (pSubnet->minVlan > pSubnet->maxVlan) {
        LOGWARN("invalid managed subnet: minVlan=%d, maxVlan=%d\n", pSubnet->minVlan, pSubnet->maxVlan);
        return (1);
    }
    return (0);
}

//!
//! Validate a gni_cluster structure content
//!
//! @param[in] cluster a pointer to the cluster structure to validate
//! @param[in] isManaged set to TRUE if this is a MANAGED style cluster or FALSE for EDGE
//!
//! @return 0 if the structure is valid or 1 if it isn't
//!
//! @see gni_node_validate()
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_cluster_validate(gni_cluster * cluster, boolean isManaged)
{
    int i = 0;

    // Make sure our pointer is valid
    if (!cluster) {
        LOGERROR("invalid input\n");
        return (1);
    }
    // We must have a name
    if (!strlen(cluster->name)) {
        LOGWARN("no cluster name\n");
        return (1);
    }
    // The enabled CC IP must not be 0.0.0.0
    if (cluster->enabledCCIp == 0) {
        LOGWARN("cluster %s: no enabledCCIp\n", cluster->name);
        return (1);
    }
    // We must be provided with a MAC prefix
    if (strlen(cluster->macPrefix) == 0) {
        LOGWARN("cluster %s: no macPrefix\n", cluster->name);
        return (1);
    }
    //
    // For non-MANAGED modes, we need to validate the subnet and the private IPs which
    // aren't provided in MANAGED mode
    //
    if (!isManaged) {
        // Validate the given private subnet
        if (gni_subnet_validate(&(cluster->private_subnet))) {
            LOGWARN("cluster %s: invalid cluster private_subnet\n", cluster->name);
            return (1);
        }
        // Validate the list of private IPs. We must have some.
        if (!cluster->max_private_ips || !cluster->private_ips) {
            LOGWARN("cluster %s: no private_ips\n", cluster->name);
            return (1);
        } else {
            // None of our private IPs should be 0.0.0.0
            for (i = 0; i < cluster->max_private_ips; i++) {
                if (cluster->private_ips[i] == 0) {
                    LOGWARN("cluster %s: empty private_ips set at idx %d\n", cluster->name, i);
                    return (1);
                }
            }
        }
    }
    // Do we have some nodes for this cluster?
    if (!cluster->max_nodes || !cluster->nodes) {
        LOGWARN("cluster %s: no nodes set\n", cluster->name);
    } else {
        // Validate each nodes
        for (i = 0; i < cluster->max_nodes; i++) {
            if (gni_node_validate(&(cluster->nodes[i]))) {
                LOGWARN("cluster %s: invalid nodes set at idx %d\n", cluster->name, i);
                return (1);
            }
        }
    }

    return (0);
}

//!
//! Validate a gni_node structure content
//!
//! @param[in] node a pointer to the node structure to validate
//!
//! @return 0 if the structure is valid or 1 if it isn't
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_node_validate(gni_node * node)
{
    int i;

    if (!node) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (!strlen(node->name)) {
        LOGWARN("no node name\n");
        return (1);
    }

    if (!node->max_instance_names || !node->instance_names) {
    } else {
        for (i = 0; i < node->max_instance_names; i++) {
            if (!strlen(node->instance_names[i].name)) {
                LOGWARN("node %s: empty instance_names set at idx %d\n", node->name, i);
                return (1);
            }
        }
    }

    return (0);
}

//!
//! Validates a given gni_secgroup structure content
//!
//! @param[in] instance a pointer to the instance structure to validate
//!
//! @return 0 if the structure is valid or 1 if it isn't
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_instance_validate(gni_instance * instance)
{
    int i;

    if (!instance) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (!strlen(instance->name)) {
        LOGWARN("no instance name\n");
        return (1);
    }

    if (!strlen(instance->accountId)) {
        LOGWARN("instance %s: no accountId\n", instance->name);
        return (1);
    }

    if (!maczero(instance->macAddress)) {
        LOGWARN("instance %s: no macAddress\n", instance->name);
    }

    if (!instance->publicIp) {
        LOGDEBUG("instance %s: no publicIp set (ignore if instance was run with private only addressing)\n", instance->name);
    }

    if (!instance->privateIp) {
        LOGWARN("instance %s: no privateIp\n", instance->name);
        return (1);
    }

    if (!instance->max_secgroup_names || !instance->secgroup_names) {
        LOGWARN("instance %s: no secgroups\n", instance->name);
        return (1);
    } else {
        for (i = 0; i < instance->max_secgroup_names; i++) {
            if (!strlen(instance->secgroup_names[i].name)) {
                LOGWARN("instance %s: empty secgroup_names set at idx %d\n", instance->name, i);
                return (1);
            }
        }
    }

    return (0);
}

//!
//! Validates a given gni_secgroup structure content
//!
//! @param[in] secgroup a pointer to the security group structure to validate
//!
//! @return 0 if the structure is valid and 1 if the structure isn't
//!
//! @see gni_secgroup
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int gni_secgroup_validate(gni_secgroup * secgroup)
{
    int i;

    if (!secgroup) {
        LOGERROR("invalid input\n");
        return (1);
    }

    if (!strlen(secgroup->name)) {
        LOGWARN("no secgroup name\n");
        return (1);
    }

    if (!strlen(secgroup->accountId)) {
        LOGWARN("secgroup %s: no accountId\n", secgroup->name);
        return (1);
    }

    if (!secgroup->max_grouprules || !secgroup->grouprules) {
        LOGTRACE("secgroup %s: no secgroup rules\n", secgroup->name);
    } else {
        for (i = 0; i < secgroup->max_grouprules; i++) {
            if (!strlen(secgroup->grouprules[i].name)) {
                LOGWARN("secgroup %s: empty grouprules set at idx %d\n", secgroup->name, i);
                return (1);
            }
        }
    }

    if (!secgroup->max_instance_names || !secgroup->instance_names) {
        LOGWARN("secgroup %s: no instances\n", secgroup->name);
        return (1);
    } else {
        for (i = 0; i < secgroup->max_instance_names; i++) {
            if (!strlen(secgroup->instance_names[i].name)) {
                LOGWARN("secgroup %s: empty instance_names set at idx %d\n", secgroup->name, i);
                return (1);
            }
        }
    }

    return (0);
}

gni_hostname_info *gni_init_hostname_info(void)
{
    gni_hostname_info *hni = EUCA_ZALLOC(1,sizeof(gni_hostname_info));
    hni->max_hostnames = 0;
    return (hni);
}

int gni_hostnames_print(gni_hostname_info *host_info)
{
    int i;

    LOGTRACE("Cached Hostname Info: \n");
    for (i = 0; i < host_info->max_hostnames; i++) {
        LOGTRACE("IP Address: %s Hostname: %s\n",inet_ntoa(host_info->hostnames[i].ip_address),host_info->hostnames[i].hostname);
    }
    return (0);
}

int gni_hostnames_free(gni_hostname_info *host_info)
{
    if (!host_info) {
        return (0);
    }

    EUCA_FREE(host_info->hostnames);
    EUCA_FREE(host_info);
    return (0);
}

int gni_hostnames_get_hostname(gni_hostname_info  *hostinfo, const char *ip_address, char **hostname)
{
    struct in_addr addr;
    gni_hostname key;
    gni_hostname *bsearch_result;

    if (!hostinfo) {
        return (1);
    }

    if (inet_aton(ip_address, &addr)) {
        key.ip_address.s_addr = addr.s_addr; // search by ip
        bsearch_result = bsearch(&key, hostinfo->hostnames, hostinfo->max_hostnames,sizeof(gni_hostname), cmpipaddr);

        if (bsearch_result) {
            *hostname = strdup(bsearch_result->hostname);
            LOGTRACE("bsearch hit: %s\n",*hostname);
            return (0);
        }
    } else {
        LOGTRACE("INET_ATON FAILED FOR: %s\n",ip_address); // we were passed a hostname
    }
    return (1);
}

//
// Used for qsort and bsearch methods against gni_hostname_info
//
int cmpipaddr(const void *p1, const void *p2)
{
    gni_hostname *hp1 = (gni_hostname *) p1;
    gni_hostname *hp2 = (gni_hostname *) p2;

    if (hp1->ip_address.s_addr == hp2->ip_address.s_addr)
        return 0;
    if (hp1->ip_address.s_addr < hp2->ip_address.s_addr)
        return -1;
    else
        return 1;
}
