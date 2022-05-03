#pragma once

#include <thumbcache.h>
#include <Shlwapi.h>				// for QueryInterface stuff
#include <new>						// for std::nothrow
#include <atomic>					// for reference counters

#define MAGIC_IWAD					MAKEFOURCC('I','W','A','D')
#define MAGIC_PWAD					MAKEFOURCC('P','W','A','D')
#define LUMPNAME(a,b,c,d,e,f,g,h)	((INT64)MAKEFOURCC(a,b,c,d) | ((INT64)MAKEFOURCC(e,f,g,h) << 32))

#define BUFFER_SIZE_DEFAULT			( 24 * 1024 * 1024)
#define BUFFER_SIZE_MAX				(256 * 1024 * 1024)
#define BUFFER_SIZE_INCREMENT		(  4 * 1024 * 1024)
#define BUFFER_BLOCK_SIZE			2048

#define PLAYPAL_TYPE_DOOM			0
#define PLAYPAL_TYPE_HERETIC		1

struct WADFileHeader {
	int			magic;
	int			lumpCount;
	int			dirOffset;
};

union WADLumpName {
	char		str[8];
	INT64		ival;
};

struct WADLumpInfo {
	DWORD		offset;
	int			size;
	WADLumpName	name;
};

struct WADPictureHeader {
	short	width;
	short	height;
	short	left;
	short	top;
};

// Building Thumbnail Handlers
// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144114(v=vs.85).aspx
// IThumbnailProvider::GetThumbnail
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb774612(v=vs.85).aspx
// Thumbnail Handlers
// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144118(v=vs.85).aspx
// https://github.com/Microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/winui/shell/appshellintegration/RecipeThumbnailProvider

class WADThumbnailProvider : public IInitializeWithStream, public IThumbnailProvider {
private:
	ULARGE_INTEGER			wadStart;
	IStream*				stream;
	std::atomic<ULONG>		references;

	DWORD					playpal;
	DWORD					titlepic;
	bool					heretic;

	USHORT					titlepicWidth;
	USHORT					titlepicHeight;

	HBITMAP					bmp;

	HRESULT					createMemoryStream();
	HRESULT					determineFileOffsetsAndGame();
	HRESULT					loadRelevantLumps();
	HRESULT					readTitlepic(byte (&palette)[256][3]);
	HRESULT					readTitle(byte (&palette)[256][3]);
	HRESULT					shrinkBitmap(int width, int height);
public:
	WADThumbnailProvider();
private:
	virtual ~WADThumbnailProvider();
public:
	IFACEMETHODIMP			GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);
	IFACEMETHODIMP			Initialize(IStream* pstream, DWORD grfMode);
	IFACEMETHODIMP			QueryInterface(REFIID iid, void** object);
	IFACEMETHODIMP_(ULONG)	AddRef();
	IFACEMETHODIMP_(ULONG)	Release();
};

extern HRESULT WADThumbnailProvider_CreateInstance(REFIID riid, void **ppv);