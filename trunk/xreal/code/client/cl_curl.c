/*
===========================================================================
Copyright (C) 2006 Tony J. White (tjw@tjw.org)

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "client.h"

#if USE_CURL

/*
=================
CL_cURL_Init
=================
*/
qboolean CL_cURL_Init()
{
	clc.cURLEnabled = qtrue;
	return qtrue;
}

/*
=================
CL_cURL_Shutdown
=================
*/
void CL_cURL_Shutdown(void)
{
	CL_cURL_Cleanup();
}

void CL_cURL_Cleanup(void)
{
	if(clc.downloadCURLM)
	{
		if(clc.downloadCURL)
		{
			curl_multi_remove_handle(clc.downloadCURLM, clc.downloadCURL);
			curl_easy_cleanup(clc.downloadCURL);
		}
		curl_multi_cleanup(clc.downloadCURLM);
		clc.downloadCURLM = NULL;
		clc.downloadCURL = NULL;
	}
	else if(clc.downloadCURL)
	{
		curl_easy_cleanup(clc.downloadCURL);
		clc.downloadCURL = NULL;
	}
}

static int CL_cURL_CallbackProgress(void *dummy, double dltotal, double dlnow, double ultotal, double ulnow)
{
	clc.downloadSize = (int)dltotal;
	Cvar_SetValue("cl_downloadSize", clc.downloadSize);

	clc.downloadCount = (int)dlnow;
	Cvar_SetValue("cl_downloadCount", clc.downloadCount);

	return 0;
}

static int CL_cURL_CallbackWrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
	FS_Write(buffer, size * nmemb, ((fileHandle_t *) stream)[0]);
	return size * nmemb;
}

void CL_cURL_BeginDownload(const char *localName, const char *remoteURL)
{
	clc.cURLUsed = qtrue;

	Com_Printf("URL: %s\n", remoteURL);
	Com_DPrintf("***** CL_cURL_BeginDownload *****\n"
				"Localname: %s\n" "RemoteURL: %s\n" "****************************\n", localName, remoteURL);
	CL_cURL_Cleanup();
	Q_strncpyz(clc.downloadURL, remoteURL, sizeof(clc.downloadURL));
	Q_strncpyz(clc.downloadName, localName, sizeof(clc.downloadName));
	Com_sprintf(clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName);

	// Set so UI gets access to it
	Cvar_Set("cl_downloadName", localName);
	Cvar_Set("cl_downloadSize", "0");
	Cvar_Set("cl_downloadCount", "0");
	Cvar_SetValue("cl_downloadTime", cls.realtime);

	clc.downloadBlock = 0;		// Starting new file
	clc.downloadCount = 0;

	clc.downloadCURL = curl_easy_init();
	if(!clc.downloadCURL)
	{
		Com_Error(ERR_DROP, "CL_cURL_BeginDownload: curl_easy_init() failed\n");
		return;
	}
	clc.download = FS_SV_FOpenFileWrite(clc.downloadTempName);
	if(!clc.download)
	{
		Com_Error(ERR_DROP, "CL_cURL_BeginDownload: failed to open %s for writing\n", clc.downloadTempName);
		return;
	}
	curl_easy_setopt(clc.downloadCURL, CURLOPT_WRITEDATA, clc.download);
	if(com_developer->integer)
		curl_easy_setopt(clc.downloadCURL, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_URL, clc.downloadURL);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_TRANSFERTEXT, 0);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_REFERER, va("XreaL://%s", NET_AdrToString(clc.serverAddress)));
	curl_easy_setopt(clc.downloadCURL, CURLOPT_USERAGENT, va("%s %s", Q3_VERSION, curl_version()));
	curl_easy_setopt(clc.downloadCURL, CURLOPT_WRITEFUNCTION, CL_cURL_CallbackWrite);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_WRITEDATA, &clc.download);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_PROGRESSFUNCTION, CL_cURL_CallbackProgress);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_PROGRESSDATA, NULL);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(clc.downloadCURL, CURLOPT_MAXREDIRS, 5);
	clc.downloadCURLM = curl_multi_init();
	if(!clc.downloadCURLM)
	{
		curl_easy_cleanup(clc.downloadCURL);
		clc.downloadCURL = NULL;
		Com_Error(ERR_DROP, "CL_cURL_BeginDownload: curl_multi_init() failed\n");
		return;
	}
	curl_multi_add_handle(clc.downloadCURLM, clc.downloadCURL);

	if(!(clc.sv_allowDownload & DLF_NO_DISCONNECT) && !clc.cURLDisconnected)
	{
		CL_AddReliableCommand("disconnect");
		CL_WritePacket();
		CL_WritePacket();
		CL_WritePacket();
		clc.cURLDisconnected = qtrue;
	}
}

void CL_cURL_PerformDownload(void)
{
	CURLMcode       res;
	CURLMsg        *msg;
	int             c;
	int             i = 0;

	res = curl_multi_perform(clc.downloadCURLM, &c);
	while(res == CURLM_CALL_MULTI_PERFORM && i < 100)
	{
		res = curl_multi_perform(clc.downloadCURLM, &c);
		i++;
	}
	if(res == CURLM_CALL_MULTI_PERFORM)
		return;

	msg = curl_multi_info_read(clc.downloadCURLM, &c);
	if(msg == NULL)
		return;

	FS_FCloseFile(clc.download);
	if(msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK)
	{
		FS_SV_Rename(clc.downloadTempName, clc.downloadName);
		clc.downloadRestart = qtrue;
	}
	else
	{
		long            code;

		curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code);
		Com_Error(ERR_DROP, "Download Error: %s Code: %ld URL: %s", curl_easy_strerror(msg->data.result), code, clc.downloadURL);
	}
	*clc.downloadTempName = *clc.downloadName = 0;
	Cvar_Set("cl_downloadName", "");
	CL_NextDownload();
}
#endif							/* USE_CURL */