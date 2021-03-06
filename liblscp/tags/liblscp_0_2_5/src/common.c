// common.c
//
/****************************************************************************
   liblscp - LinuxSampler Control Protocol API
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

#include "common.h"

#include <ctype.h>


// Split chunk size magic:
// LSCP_SPLIT_CHUNK1 := 2 ^ LSCP_SPLIT_CHUNK2
#define LSCP_SPLIT_CHUNK1   4
#define LSCP_SPLIT_CHUNK2   2
// Chunk size legal calculator.
#define LSCP_SPLIT_SIZE(n) ((((n) >> LSCP_SPLIT_CHUNK2) + 1) << LSCP_SPLIT_CHUNK2)


//-------------------------------------------------------------------------
// Local client request executive.

// Result buffer internal settler.
void lscp_client_set_result ( lscp_client_t *pClient, char *pszResult, int iErrno )
{
    if (pClient->pszResult)
        free(pClient->pszResult);
    pClient->pszResult = NULL;

    pClient->iErrno = iErrno;

    if (pszResult)
        pClient->pszResult = strdup(lscp_ltrim(pszResult));
}


// The common client receiver executive.
lscp_status_t lscp_client_recv ( lscp_client_t *pClient, char *pchBuffer, int *pcchBuffer, int iTimeout )
{
    fd_set fds;                         // File descriptor list for select().
    int    fd, fdmax;                   // Maximum file descriptor number.
    struct timeval tv;                  // For specifying a timeout value.
    int    iSelect;                     // Holds select return status.

    lscp_status_t ret = LSCP_FAILED;

    if (pClient == NULL)
        return ret;

    // Prepare for waiting on select...
    fd = (int) pClient->cmd.sock;
    FD_ZERO(&fds);
    FD_SET((unsigned int) fd, &fds);
    fdmax = fd;

    // Use the timeout select feature...
    if (iTimeout < 1)
        iTimeout = pClient->iTimeout;
    if (iTimeout > 1000) {
        tv.tv_sec = iTimeout / 1000;
        iTimeout -= tv.tv_sec * 1000;
    }
    else tv.tv_sec = 0;
    tv.tv_usec = iTimeout * 1000;

    // Wait for event...
    iSelect = select(fdmax + 1, &fds, NULL, NULL, &tv);
    if (iSelect > 0 && FD_ISSET(fd, &fds)) {
        // May recv now...
        *pcchBuffer = recv(pClient->cmd.sock, pchBuffer, *pcchBuffer, 0);
        if (*pcchBuffer > 0)
            ret = LSCP_OK;
        else if (*pcchBuffer < 0)
            lscp_socket_perror("lscp_client_recv: recv");
        else if (*pcchBuffer == 0) {
            // Damn, server probably disconnected,
            // we better free everything down here.
            lscp_socket_agent_free(&(pClient->evt));
            lscp_socket_agent_free(&(pClient->cmd));
            // Fake a result message.
            ret = LSCP_QUIT;
        }
    }   // Check if select has timed out.
    else if (iSelect == 0)
        ret = LSCP_TIMEOUT;
    else
        lscp_socket_perror("lscp_client_recv: select");

    return ret;
}


// The main client requester call executive.
lscp_status_t lscp_client_call ( lscp_client_t *pClient, const char *pszQuery )
{
    int    cchQuery;
    char   achResult[LSCP_BUFSIZ];
    int    cchResult;
    const  char *pszSeps = ":[]";
    char  *pszResult;
    char  *pszToken;
    char  *pch;
    int    iErrno;

    lscp_status_t ret = LSCP_FAILED;

    if (pClient == NULL)
        return ret;

    pszResult = NULL;
    iErrno = -1;

    // Check if command socket socket is still valid.
    if (pClient->cmd.sock == INVALID_SOCKET) {
        pszResult = "Connection closed or no longer valid";
        lscp_client_set_result(pClient, pszResult, iErrno);
        return ret;
    }

    // Send data, and then, wait for the result...
    cchQuery = strlen(pszQuery);
    if (send(pClient->cmd.sock, pszQuery, cchQuery, 0) < cchQuery) {
        lscp_socket_perror("lscp_client_call: send");
        pszResult = "Failure during send operation";
        lscp_client_set_result(pClient, pszResult, iErrno);
        return ret;
    }

    // Wait for receive event...
    cchResult = sizeof(achResult);
    ret = lscp_client_recv(pClient, achResult, &cchResult, pClient->iTimeout);

    switch (ret) {

      case LSCP_OK:
        // Always force the result to be null terminated (and trim trailing CRLFs)!
        while (cchResult > 0 && (achResult[cchResult - 1] == '\n' || achResult[cchResult- 1] == '\r'))
            cchResult--;
        achResult[cchResult] = (char) 0;
        // Check if the response it's an error or warning message.
        if (strncasecmp(achResult, "WRN:", 4) == 0)
            ret = LSCP_WARNING;
        else if (strncasecmp(achResult, "ERR:", 4) == 0)
            ret = LSCP_ERROR;
        // So we got a result...
        if (ret == LSCP_OK) {
            // Reset errno in case of success.
            iErrno = 0;
            // Is it a special successful response?
            if (strncasecmp(achResult, "OK[", 3) == 0) {
                // Parse the OK message, get the return string under brackets...
                pszToken = lscp_strtok(achResult, pszSeps, &(pch));
                if (pszToken)
                    pszResult = lscp_strtok(NULL, pszSeps, &(pch));
            }
            else pszResult = achResult;
            // The result string is now set to the command response, if any.
        } else {
            // Parse the error/warning message, skip first colon...
            pszToken = lscp_strtok(achResult, pszSeps, &(pch));
            if (pszToken) {
                // Get the error number...
                pszToken = lscp_strtok(NULL, pszSeps, &(pch));
                if (pszToken) {
                    iErrno = atoi(pszToken);
                    // And make the message text our final result.
                    pszResult = lscp_strtok(NULL, pszSeps, &(pch));
                }
            }
            // The result string is set to the error/warning message text.
        }
        break;

      case LSCP_TIMEOUT:
        // Fake a result message.
        pszResult = "Timeout during receive operation";
        iErrno = (int) ret;
        break;

      case LSCP_QUIT:
        // Fake a result message.
        pszResult = "Server terminated the connection";
        iErrno = (int) ret;
        break;

      case LSCP_FAILED:
      default:
        // What's down?
        pszResult = "Failure during receive operation";
        break;
    }

    // Make the result official...
    lscp_client_set_result(pClient, pszResult, iErrno);

    return ret;
}


//-------------------------------------------------------------------------
// Other general utility functions.

// Trimming left spaces...
char *lscp_ltrim ( char *psz )
{
    while (isspace(*psz))
        psz++;
    return psz;
}

// Unquote an in-split string.
char *lscp_unquote ( char **ppsz, int dup )
{
    char  chQuote;
    char *psz = *ppsz;

    while (isspace(*psz))
        ++psz;
    if (*psz == '\"' || *psz == '\'') {
        chQuote = *psz++;
        while (isspace(*psz))
            ++psz;
        if (dup)
            psz = strdup(psz);
        *ppsz = psz;
        if (*ppsz) {
            while (**ppsz && **ppsz != chQuote)
                ++(*ppsz);
            if (**ppsz) {
                while (isspace(*(*ppsz - 1)) && *ppsz > psz)
                    --(*ppsz);
                *(*ppsz)++ = (char) 0;
            }
        }
    }
    else if (dup) {
        psz = strdup(psz);
        *ppsz = psz;
    }

    return psz;
}

// Unquote and make a duplicate of an in-split string.
void lscp_unquote_dup ( char **ppszDst, char **ppszSrc )
{
     // Free desteny string, if already there.
     if (*ppszDst)
         free(*ppszDst);
     *ppszDst = NULL;
     // Unquote and duplicate.
     if (*ppszSrc)
         *ppszDst = lscp_unquote(ppszSrc, 1);
}


// Custom tokenizer.
char *lscp_strtok ( char *pchBuffer, const char *pszSeps, char **ppch )
{
    char *pszToken;

    if (pchBuffer == NULL)
        pchBuffer = *ppch;

    pchBuffer += strspn(pchBuffer, pszSeps);
    if (*pchBuffer == '\0')
        return NULL;

    pszToken  = pchBuffer;
    pchBuffer = strpbrk(pszToken, pszSeps);
    if (pchBuffer == NULL) {
        *ppch = strchr(pszToken, '\0');
    } else {
        *pchBuffer = '\0';
        *ppch = pchBuffer + 1;
        while (**ppch && strchr(pszSeps, **ppch))
            (*ppch)++;
    }

    return pszToken;
}


// Split a comma separated string into a null terminated array of strings.
char **lscp_szsplit_create ( const char *pszCsv, const char *pszSeps )
{
    char *pszHead, *pch;
    int iSize, i, j, cchSeps;
    char **ppszSplit, **ppszNewSplit;

    // Initial size is one chunk away.
    iSize = LSCP_SPLIT_CHUNK1;
    // Allocate and split...
    ppszSplit = (char **) malloc(iSize * sizeof(char *));
    if (ppszSplit == NULL)
        return NULL;

    // Make a copy of the original string.
    i = 0;
    pszHead = (char *) pszCsv;
    if ((ppszSplit[i++] = lscp_unquote(&pszHead, 1)) == NULL) {
        free(ppszSplit);
        return NULL;
    }

    // Go on for it...
    cchSeps = strlen(pszSeps);
    while ((pch = strpbrk(pszHead, pszSeps)) != NULL) {
        // Pre-advance to next item.
        pszHead = pch + cchSeps;
        // Trim and null terminate current item.
        while (isspace(*(pch - 1)) && pch > ppszSplit[0])
            --pch;
        *pch = (char) 0;
        // Make it official.
        ppszSplit[i++] = lscp_unquote(&pszHead, 0);
        // Do we need to grow?
        if (i >= iSize) {
            // Yes, but only grow in chunks.
            iSize += LSCP_SPLIT_CHUNK1;
            // Allocate and copy to new split array.
            ppszNewSplit = (char **) malloc(iSize * sizeof(char *));
            if (ppszNewSplit) {
                for (j = 0; j < i; j++)
                    ppszNewSplit[j] = ppszSplit[j];
                free(ppszSplit);
                ppszSplit = ppszNewSplit;
            }
        }
    }

    // NULL terminate split array.
    for ( ; i < iSize; i++)
        ppszSplit[i] = NULL;

    return ppszSplit;
}


// Free allocated memory of a legal null terminated array of strings.
void lscp_szsplit_destroy ( char **ppszSplit )
{
    // Our split string is always the first item, if any.
    if (ppszSplit && ppszSplit[0])
        free(ppszSplit[0]);
    // Now free the array itself.
    if (ppszSplit)
        free(ppszSplit);
}


#ifdef LSCP_SZSPLIT_COUNT

// Return the number of items of a null terminated array of strings.
int lscp_szsplit_count ( char **ppszSplit )
{
    int i = 0;
    while (ppszSplit && ppszSplit[i])
        i++;
    return i;
}

// Return the allocated number of items of a splitted string array.
int lscp_szsplit_size ( char **ppszSplit )
{
    return LSCP_SPLIT_SIZE(lscp_szsplit_count(ppszSplit));
}

#endif // LSCP_SZSPLIT_COUNT


// Split a comma separated string into a -1 terminated array of positive integers.
int *lscp_isplit_create ( const char *pszCsv, const char *pszSeps )
{
    char *pchHead, *pch;
    int iSize, i, j, cchSeps;
    int *piSplit, *piNewSplit;

    // Get it clean first.
    pchHead = lscp_ltrim((char *) pszCsv);
    if (*pchHead == (char) 0)
        return NULL;
    
    // Initial size is one chunk away.
    iSize = LSCP_SPLIT_CHUNK1;
    // Allocate and split...
    piSplit = (int *) malloc(iSize * sizeof(int));
    if (piSplit == NULL)
        return NULL;

    // Make a copy of the original string.
    i = 0;
    if ((piSplit[i++] = atoi(pchHead)) < 0) {
        free(piSplit);
        return NULL;
    }

    // Go on for it...
    cchSeps = strlen(pszSeps);
    while ((pch = strpbrk(pchHead, pszSeps)) != NULL) {
        // Pre-advance to next item.
        pchHead = pch + cchSeps;
        // Make it official.
        piSplit[i++] = atoi(pchHead);
        // Do we need to grow?
        if (i >= iSize) {
            // Yes, but only grow in chunks.
            iSize += LSCP_SPLIT_CHUNK1;
            // Allocate and copy to new split array.
            piNewSplit = (int *) malloc(iSize * sizeof(int));
            if (piNewSplit) {
                for (j = 0; j < i; j++)
                    piNewSplit[j] = piSplit[j];
                free(piSplit);
                piSplit = piNewSplit;
            }
        }
    }

    // NULL terminate split array.
    for ( ; i < iSize; i++)
        piSplit[i] = -1;

    return piSplit;
}


// Destroy a integer splitted array.
void lscp_isplit_destroy ( int *piSplit )
{
    if (piSplit)
        free(piSplit);
}


#ifdef LSCP_ISPLIT_COUNT

// Compute a string list valid item count.
int lscp_isplit_count ( int *piSplit )
{
    int i = 0;
    while (piSplit && piSplit[i] >= 0)
        i++;
    return i;
}

// Compute a string list size.
int lscp_isplit_size ( int *piSplit )
{
    return LSCP_SPLIT_SIZE(lscp_isplit_count(piSplit));
}

#endif // LSCP_ISPLIT_COUNT


// Split a string into a null terminated array of parameter items.
lscp_param_t *lscp_psplit_create ( const char *pszCsv, const char *pszSeps1, const char *pszSeps2 )
{
    char *pszHead, *pch;
    int iSize, i, j, cchSeps1, cchSeps2;
    lscp_param_t *ppSplit, *ppNewSplit;

    pszHead = strdup(pszCsv);
    if (pszHead == NULL)
        return NULL;

    iSize = LSCP_SPLIT_CHUNK1;
    ppSplit = (lscp_param_t *) malloc(iSize * sizeof(lscp_param_t));
    if (ppSplit == NULL) {
        free(pszHead);
        return NULL;
    }

    cchSeps1 = strlen(pszSeps1);
    cchSeps2 = strlen(pszSeps2);

    i = 0;
    while ((pch = strpbrk(pszHead, pszSeps1)) != NULL) {
        ppSplit[i].key = pszHead;
        pszHead = pch + cchSeps1;
        *pch = (char) 0;
        ppSplit[i].value = lscp_unquote(&pszHead, 0);
        if ((pch = strpbrk(pszHead, pszSeps2)) != NULL) {
            pszHead = pch + cchSeps2;
            *pch = (char) 0;
        }
        if (++i >= iSize) {
            iSize += LSCP_SPLIT_CHUNK1;
            ppNewSplit = (lscp_param_t *) malloc(iSize * sizeof(lscp_param_t));
            if (ppNewSplit) {
                for (j = 0; j < i; j++) {
                    ppNewSplit[j].key   = ppSplit[j].key;
                    ppNewSplit[j].value = ppSplit[j].value;
                }
                free(ppSplit);
                ppSplit = ppNewSplit;
            }
        }
    }

    if (i < 1)
        free(pszHead);

    for ( ; i < iSize; i++) {
        ppSplit[i].key   = NULL;
        ppSplit[i].value = NULL;
    }

    return ppSplit;
}


// Destroy a parameter list array.
void lscp_psplit_destroy ( lscp_param_t *ppSplit )
{
    if (ppSplit && ppSplit[0].key)
        free(ppSplit[0].key);
    if (ppSplit)
        free(ppSplit);
}


#ifdef LSCP_PSPLIT_COUNT

// Compute a parameter list valid item count.
int lscp_psplit_count ( lscp_param_t *ppSplit )
{
    int i = 0;
    while (ppSplit && ppSplit[i].key)
        i++;
    return i;
}

// Compute a parameter list size.
int lscp_psplit_size ( lscp_param_t *ppSplit )
{
    return LSCP_SPLIT_SIZE(lscp_psplit_count(ppSplit));
}

#endif // LSCP_PSPLIT_COUNT


// Allocate a parameter list, optionally copying an existing one.
void lscp_plist_alloc (lscp_param_t **ppList)
{
    lscp_param_t *pParams;
    int iSize, i;

    if (ppList) {
        iSize = LSCP_SPLIT_CHUNK1;
        pParams = (lscp_param_t *) malloc(iSize * sizeof(lscp_param_t));
        if (pParams) {
            for (i = 0 ; i < iSize; i++) {
                pParams[i].key   = NULL;
                pParams[i].value = NULL;
            }
        }
        *ppList = pParams;
    }
}


// Destroy a parameter list, including all it's contents.
void lscp_plist_free ( lscp_param_t **ppList )
{
    lscp_param_t *pParams;
    int i;
    
    if (ppList) {
        if (*ppList) {
            pParams = *ppList;
            for (i = 0; pParams && pParams[i].key; i++) {
                free(pParams[i].key);
                free(pParams[i].value);
            }
            free(pParams);
        }
        *ppList = NULL;
    }
}


// Add an item to a parameter list, growing it as fit.
void lscp_plist_append ( lscp_param_t **ppList, const char *pszKey, const char *pszValue )
{
    lscp_param_t *pParams;
    lscp_param_t *pNewParams;
    int iSize, iNewSize;
    int i = 0;
    
    if (ppList && *ppList) {
        pParams = *ppList;
        while (pParams[i].key) {
            if (strcasecmp(pParams[i].key, pszKey) == 0) {
                if (pParams[i].value)
                    free(pParams[i].value);
                pParams[i].value = strdup(pszValue);
                return;
            }
            i++;
        }
        iSize = LSCP_SPLIT_SIZE(i);
        pParams[i].key   = strdup(pszKey);
        pParams[i].value = strdup(pszValue);
        if (++i >= iSize) {
            iNewSize   = iSize + LSCP_SPLIT_CHUNK1;
            pNewParams = (lscp_param_t *) malloc(iNewSize * sizeof(lscp_param_t));
            for (i = 0; i < iSize; i++) {
                pParams[i].key   = pParams[i].key;
                pParams[i].value = pParams[i].value;
            }
            for ( ; i < iNewSize; i++) {
                pNewParams[i].key   = NULL;
                pNewParams[i].value = NULL;
            }
            free(pParams);
            *ppList = pNewParams;
        }
    }
}

#ifdef LSCP_PLIST_COUNT

// Compute a parameter list valid item count.
int lscp_plist_count ( lscp_param_t **ppList )
{
    lscp_param_t *pParams;
    int i = 0;
    if (ppList && *ppList) {
        pParams = *ppList;
        while (pParams[i].key)
            i++;
    }
    return i;
}

// Compute the legal parameter list size.
int lscp_plist_size ( lscp_param_t **ppList )
{
    return LSCP_SPLIT_SIZE(lscp_plist_count(ppList));
}

#endif // LSCP_PLIST_COUNT


//-------------------------------------------------------------------------
// Engine info struct helper functions.

void lscp_engine_info_init ( lscp_engine_info_t *pEngineInfo )
{
    pEngineInfo->description = NULL;
    pEngineInfo->version     = NULL;
}

void lscp_engine_info_free ( lscp_engine_info_t *pEngineInfo )
{
    if (pEngineInfo->description)
        free(pEngineInfo->description);
    if (pEngineInfo->version)
        free(pEngineInfo->version);
}

void lscp_engine_info_reset ( lscp_engine_info_t *pEngineInfo )
{
    lscp_engine_info_free(pEngineInfo);
    lscp_engine_info_init(pEngineInfo);
}


//-------------------------------------------------------------------------
// Channel info struct helper functions.

void lscp_channel_info_init ( lscp_channel_info_t *pChannelInfo )
{
    pChannelInfo->engine_name       = NULL;
    pChannelInfo->audio_device      = 0;
    pChannelInfo->audio_channels    = 0;
    pChannelInfo->audio_routing     = NULL;
    pChannelInfo->instrument_file   = NULL;
    pChannelInfo->instrument_nr     = 0;
    pChannelInfo->instrument_name   = NULL;
    pChannelInfo->instrument_status = 0;
    pChannelInfo->midi_device       = 0;
    pChannelInfo->midi_port         = 0;
    pChannelInfo->midi_channel      = 0;
    pChannelInfo->volume            = 0.0;
}

void lscp_channel_info_free ( lscp_channel_info_t *pChannelInfo )
{
    if (pChannelInfo->engine_name)
        free(pChannelInfo->engine_name);
    if (pChannelInfo->audio_routing)
        lscp_szsplit_destroy(pChannelInfo->audio_routing);
    if (pChannelInfo->instrument_file)
        free(pChannelInfo->instrument_file);
    if (pChannelInfo->instrument_name)
        free(pChannelInfo->instrument_name);
}

void lscp_channel_info_reset ( lscp_channel_info_t *pChannelInfo )
{
    lscp_channel_info_free(pChannelInfo);
    lscp_channel_info_init(pChannelInfo);
}


//-------------------------------------------------------------------------
// Driver info struct functions.

void lscp_driver_info_init ( lscp_driver_info_t *pDriverInfo )
{
    pDriverInfo->description = NULL;
    pDriverInfo->version     = NULL;
    pDriverInfo->parameters  = NULL;
}

void lscp_driver_info_free ( lscp_driver_info_t *pDriverInfo )
{
    if (pDriverInfo->description)
        free(pDriverInfo->description);
    if (pDriverInfo->version)
        free(pDriverInfo->version);
    lscp_szsplit_destroy(pDriverInfo->parameters);
}

void lscp_driver_info_reset ( lscp_driver_info_t *pDriverInfo )
{
    lscp_driver_info_free(pDriverInfo);
    lscp_driver_info_init(pDriverInfo);
}


//-------------------------------------------------------------------------
// Device info struct functions.

void lscp_device_info_init ( lscp_device_info_t *pDeviceInfo )
{
    pDeviceInfo->driver = NULL;
    lscp_plist_alloc(&(pDeviceInfo->params));
}

void lscp_device_info_free ( lscp_device_info_t *pDeviceInfo )
{
    if (pDeviceInfo->driver)
        free(pDeviceInfo->driver);
    lscp_plist_free(&(pDeviceInfo->params));
}

void lscp_device_info_reset ( lscp_device_info_t *pDeviceInfo )
{
    lscp_device_info_free(pDeviceInfo);
    lscp_device_info_init(pDeviceInfo);
}


//-------------------------------------------------------------------------
// Device channel/port info struct functions.

void lscp_device_port_info_init ( lscp_device_port_info_t *pDevicePortInfo )
{
    pDevicePortInfo->name = NULL;
    lscp_plist_alloc(&(pDevicePortInfo->params));
}

void lscp_device_port_info_free ( lscp_device_port_info_t *pDevicePortInfo )
{
    if (pDevicePortInfo->name)
        free(pDevicePortInfo->name);
    lscp_plist_free(&(pDevicePortInfo->params));
}

void lscp_device_port_info_reset ( lscp_device_port_info_t *pDevicePortInfo )
{
    lscp_device_port_info_free(pDevicePortInfo);
    lscp_device_port_info_init(pDevicePortInfo);
}


//-------------------------------------------------------------------------
// Parameter struct helper functions.

void lscp_param_info_init ( lscp_param_info_t *pParamInfo )
{
    pParamInfo->type          = LSCP_TYPE_NONE;
    pParamInfo->description   = NULL;
    pParamInfo->mandatory     = 0;
    pParamInfo->fix           = 0;
    pParamInfo->multiplicity  = 0;
    pParamInfo->depends       = NULL;
    pParamInfo->defaultv      = NULL;
    pParamInfo->range_min     = NULL;
    pParamInfo->range_max     = NULL;
    pParamInfo->possibilities = NULL;
}

void lscp_param_info_free ( lscp_param_info_t *pParamInfo )
{
    if (pParamInfo->description)
        free(pParamInfo->description);
    lscp_szsplit_destroy(pParamInfo->depends);
    if (pParamInfo->defaultv)
        free(pParamInfo->defaultv);
    if (pParamInfo->range_min)
        free(pParamInfo->range_min);
    if (pParamInfo->range_max)
        free(pParamInfo->range_max);
    lscp_szsplit_destroy(pParamInfo->possibilities);
}

void lscp_param_info_reset ( lscp_param_info_t *pParamInfo )
{
    lscp_param_info_free(pParamInfo);
    lscp_param_info_init(pParamInfo);
}


//-------------------------------------------------------------------------
// Concatenate a parameter list (key='value'...) into a string,
// appending a crlf terminator.

int lscp_param_concat ( char *pszBuffer, int cchMaxBuffer, lscp_param_t *pParams )
{
    int cchBuffer, cchParam, i;

    if (pszBuffer == NULL)
        return 0;

    cchBuffer = strlen(pszBuffer);
    for (i = 0; pParams && pParams[i].key && pParams[i].value; i++) {
        cchParam = strlen(pParams[i].key) + strlen(pParams[i].value) + 4;
        if (cchBuffer + cchParam + 2 < cchMaxBuffer) {
            sprintf(pszBuffer + cchBuffer, " %s='%s'", pParams[i].key, pParams[i].value);
            cchBuffer += cchParam;
        }
    }
    
    if (cchBuffer + 2 < cchMaxBuffer) {
        pszBuffer[cchBuffer++] = '\r';
        pszBuffer[cchBuffer++] = '\n';
        pszBuffer[cchBuffer ]  = (char) 0;
    }
    
    return cchBuffer;
}


// end of common.c
