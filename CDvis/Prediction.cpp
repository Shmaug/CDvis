//#define PY_SSIZE_T_CLEAN
//#ifdef _DEBUG
//#undef _DEBUG
//#pragma comment (lib, "python36.lib")
//#include <Python.h>
//#define _DEBUG
//#else
//#include <Python.h>
//#endif

#include "Prediction.hpp"
#include "ImageLoader.hpp"

#include <shellapi.h>

using namespace std;

void Prediction::RunPrediction(const jwstring& folder, shared_ptr<Texture>& texture) {
	HANDLE inPipe_Read;
	HANDLE inPipe_Write;
	HANDLE outPipe_Read;
	HANDLE outPipe_Write;

	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	CreatePipe(&outPipe_Read, &outPipe_Write, &saAttr, 0);
	SetHandleInformation(outPipe_Read, HANDLE_FLAG_INHERIT, 0);
	CreatePipe(&inPipe_Read, &inPipe_Write, &saAttr, 0);
	SetHandleInformation(inPipe_Write, HANDLE_FLAG_INHERIT, 0);

	char cmd[512];
	sprintf_s(cmd, 512, "python prediction.py \"%S\" output", folder.c_str());

	PROCESS_INFORMATION piProcInfo;

	STARTUPINFOA siStartInfo;
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
	siStartInfo.cb = sizeof(STARTUPINFOA);
	siStartInfo.hStdError = outPipe_Write;
	siStartInfo.hStdOutput = outPipe_Write;
	siStartInfo.hStdInput = inPipe_Read;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
		DWORD dwRead;
		CHAR chBuf[4096];
		BOOL bSuccess = FALSE;
		HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

		for (;;) {
			bSuccess = ReadFile(outPipe_Read, chBuf, 4096, &dwRead, NULL);
			if (!bSuccess || dwRead == 0) break;

			OutputDebugStringA(chBuf);
			//bSuccess = WriteFile(hParentStdOut, chBuf, dwRead, &dwWritten, NULL);
			//if (!bSuccess) break;
			// TODO: loop doesn't exit
		}

		jwstring output = GetFullPathW(L"output");
		ImageLoader::LoadMask(output, texture);
	}
}