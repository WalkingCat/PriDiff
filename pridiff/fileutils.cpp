#include "stdafx.h"
#include "fileutils.h"
#include <io.h>
#include <fcntl.h>

void setmode_to_utf16(FILE * file)
{
	_setmode(_fileno(file), _O_U16TEXT);
}
