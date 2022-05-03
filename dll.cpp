#include <ShlObj.h>		// for SHChangeNotify
#include "provider.h"

#define WAD_EXT						L"wad"
#define SZ_CLSID_WADTHUMBHANDLER	L"{95b6f654-fce0-4dc6-afbc-f33df295f12b}"
#define REG_CLSID_PATH				L"Software\\Classes\\CLSID\\" SZ_CLSID_WADTHUMBHANDLER
#define REG_ASSOCIATION(ext)		(L"Software\\Classes\\." ext L"\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}")

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID iid, void** object);

struct CLASS_OBJECT_INIT {
	const CLSID*		clsid;
	PFNCREATEINSTANCE	create;
};

const CLSID				CLSID_WADThumbHandler	= { 0x95b6f654, 0xfce0, 0x4dc6, { 0xaf, 0xbc, 0xf3, 0x3d, 0xf2, 0x95, 0xf1, 0x2b }};
const CLASS_OBJECT_INIT	providerClasses[]		= {
	{ &CLSID_WADThumbHandler, WADThumbnailProvider_CreateInstance }
};

// Handle to the DLL's module
HINSTANCE				g_hInst					= nullptr;
std::atomic<int>		g_moduleRefs			= 0;

STDAPI_(BOOL) DllMain(HINSTANCE hInst, DWORD reason, void*) {
	if(reason == DLL_PROCESS_ATTACH) {
		g_hInst = hInst;
		DisableThreadLibraryCalls(hInst);
	}
	return TRUE;
}

STDAPI DllCanUnloadNow() {
	return (g_moduleRefs == 0) ? S_OK : S_FALSE;
}

inline void DllAddRef() {
	++g_moduleRefs;
}

inline void DllRelease() {
	--g_moduleRefs;
}

class ClassFactory : public IClassFactory {
public:
	static HRESULT CreateInstance(REFCLSID clsid, const CLASS_OBJECT_INIT* classInitializers, size_t classInitializersCount, REFIID iid, void** object) {
		*object = nullptr;
		HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
		for(size_t i = 0; i < classInitializersCount; ++i) {
			if(clsid == *classInitializers[i].clsid) {
				IClassFactory* factory = new (std::nothrow) ClassFactory(classInitializers[i].create);
				hr = factory ? S_OK : E_OUTOFMEMORY;
				if(SUCCEEDED(hr)) {
					hr = factory->QueryInterface(iid, object);
					factory->Release();
				}
			}
		}
		return hr;
	}
private:
	std::atomic<ULONG>	references;
	PFNCREATEINSTANCE	create;
public:
	ClassFactory(PFNCREATEINSTANCE create) : references(1), create(create) {
		DllAddRef();
	}
private:
	~ClassFactory() {
		DllRelease();
	}
public:
	IFACEMETHODIMP QueryInterface(REFIID iid, void** object) {
		static const QITAB tabs[] = {
			QITABENT(ClassFactory, IClassFactory),
			{ 0 }
		};
		return QISearch(this, tabs, iid, object);
	}
	IFACEMETHODIMP_(ULONG) AddRef() {
		return ++references;
	}
	IFACEMETHODIMP_(ULONG) Release() {
		ULONG count = --references;
		if(count == 0) {
			delete this;
		}
		return count;
	}
	IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** object) {
		return outer ? CLASS_E_NOAGGREGATION : create(iid, object);
	}
	IFACEMETHODIMP LockServer(BOOL lock) {
		if(lock) DllAddRef();
		else DllRelease();
		return S_OK;
	}
};

// names that say nothing about the data other than whats already included in the type. so useful
STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
	return ClassFactory::CreateInstance(clsid, providerClasses, ARRAYSIZE(providerClasses), iid, object);
}

struct REGISTRY_ENTRY {
	HKEY	keyRoot;
	PCWSTR	keyName;
	PCWSTR	valueName;
	PCWSTR	data;
};

HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY* entry) {
	HKEY key;
	HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(entry->keyRoot, entry->keyName, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr));
	if(SUCCEEDED(hr)) {
		hr = HRESULT_FROM_WIN32(RegSetValueExW(key, entry->valueName, 0, REG_SZ, (LPBYTE) entry->data, ((DWORD) wcslen(entry->data) + 1) * sizeof(WCHAR)));
		RegCloseKey(key);
	}
	return hr;
}

STDAPI DllRegisterServer() {
	HRESULT	hr;
	WCHAR	moduleName[MAX_PATH];

	if(!GetModuleFileNameW(g_hInst, moduleName, ARRAYSIZE(moduleName))) {
		hr = HRESULT_FROM_WIN32(GetLastError());
	} else {
		const REGISTRY_ENTRY registryEntries[] = {
			// RootKey				KeyName									ValueName			Data
			{ HKEY_CURRENT_USER,	REG_CLSID_PATH,							nullptr,			L"WAD Thumbnail Handler" },
			{ HKEY_CURRENT_USER,	REG_CLSID_PATH L"\\InProcServer32",		nullptr,			moduleName				 },
			{ HKEY_CURRENT_USER,	REG_CLSID_PATH L"\\InProcServer32",		L"ThreadingModel",	L"Apartment"			 },
			{ HKEY_CURRENT_USER,	REG_ASSOCIATION(WAD_EXT),				nullptr,			SZ_CLSID_WADTHUMBHANDLER },
		};
		hr = S_OK;
		for(int i = 0; i < ARRAYSIZE(registryEntries) && SUCCEEDED(hr); ++i) {
			hr = CreateRegKeyAndSetValue(&registryEntries[i]);
		}
		if(SUCCEEDED(hr)) {
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
		}
	}
	return hr;
}

STDAPI DllUnregisterServer() {
	HRESULT hr = S_OK;

	const PCWSTR keys[] = {
		REG_CLSID_PATH,
		REG_ASSOCIATION(WAD_EXT)
	};

	// not using foreach because of SUCCEEDED check
	for(int i = 0; i < ARRAYSIZE(keys) && SUCCEEDED(hr); ++i) {
		hr = HRESULT_FROM_WIN32(RegDeleteTreeW(HKEY_CURRENT_USER, keys[i]));
		if(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			hr = S_OK;
		}
	}

	return hr;
}