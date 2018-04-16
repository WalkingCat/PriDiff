#include "stdafx.h"
#include "fileutils.h"
#include <io.h>
#include <fcntl.h>

// this enables unicode output in console or redirected to file, 
// but still doesn't work when piping to other process
void prepare_unicode_output()
{
//	SetConsoleOutputCP(CP_UTF8);
	_setmode(_fileno(stdout), _O_U8TEXT);
	fwide(stdout, 1);
//	fwprintf_s(stdout, L"+ TEST (¨s¡ã¡õ¡ã£©¨s¦à ©ß©¥©ß");
}
