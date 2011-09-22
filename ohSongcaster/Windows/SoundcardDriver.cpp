#include "SoundcardDriver.h"
#include "PolicyConfig.h"

#include <OpenHome/Private/Arch.h>
#include <OpenHome/Private/Debug.h>

#include <Setupapi.h>
#include <ks.h>
#include <ksmedia.h>
#include <initguid.h>
#include <Shellapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <wchar.h>

EXCEPTION(SongcasterError);

using namespace OpenHome;
using namespace OpenHome::Net;

// {2685C863-5E57-4D9A-86EC-2EC9B7BBBCFD}
DEFINE_GUID(OHSOUNDCARD_GUID, 0x2685C863, 0x5E57, 0x4D9A, 0x86, 0xEC, 0x2E, 0xC9, 0xB7, 0xBB, 0xBC, 0xFD);

// C interface

THandle STDCALL SongcasterCreate(const char* aDomain, uint32_t aSubnet, uint32_t aChannel, uint32_t aTtl, uint32_t aMulticast, uint32_t aEnabled, uint32_t aPreset, ReceiverCallback aReceiverCallback, void* aReceiverPtr, SubnetCallback aSubnetCallback, void* aSubnetPtr, ConfigurationChangedCallback aConfigurationChangedCallback, void* aConfigurationChangedPtr, const char* aManufacturer, const char* aManufacturerUrl, const char* aModelUrl)
{
	try {
        printf("%s\n", aDomain);
        
        // get the computer name
        Bws<Songcaster::kMaxUdnBytes> computer;
        TUint bytes = computer.MaxBytes();

        if (!GetComputerName((LPSTR)computer.Ptr(), (LPDWORD)&bytes)) {
            THROW(SongcasterError);
        }
        
        computer.SetBytes(bytes);
        
		TBool enabled = (aEnabled == 0) ? false : true;
		TBool multicast = (aMulticast == 0) ? false : true;

        // create the sender driver
        OhmSenderDriverWindows* driver = new OhmSenderDriverWindows(aDomain, aManufacturer, enabled);

        // create the soundcard
		Songcaster* songcaster = new Songcaster(aSubnet, aChannel, aTtl, multicast, enabled, aPreset, aReceiverCallback, aReceiverPtr, aSubnetCallback, aSubnetPtr, aConfigurationChangedCallback, aConfigurationChangedPtr, computer, driver, aManufacturer, aManufacturerUrl, aModelUrl);

		driver->SetSongcaster(*songcaster);

		return (songcaster);
	}
	catch (SongcasterError)
    {
	}

	return (0);
}

// OhmSenderDriverWindows

static const TUint KSPROPERTY_OHSOUNDCARD_VERSION = 0;
static const TUint KSPROPERTY_OHSOUNDCARD_ENABLED = 1;
static const TUint KSPROPERTY_OHSOUNDCARD_ACTIVE = 2;
static const TUint KSPROPERTY_OHSOUNDCARD_ENDPOINT = 3;
static const TUint KSPROPERTY_OHSOUNDCARD_TTL = 4;

OhmSenderDriverWindows::OhmSenderDriverWindows(const char* aDomain, const char* aManufacturer, TBool aEnabled)
	: iEnabled(aEnabled)
	, iRefCount(1)
	, iSongcaster(NULL)
	, iDeviceEnumerator(NULL)
{
	if (!FindDriver(aDomain)) {
		THROW(SongcasterError);
	}

	if (!FindEndpoint(aManufacturer)) {
		THROW(SongcasterError);
	}
}

OhmSenderDriverWindows::~OhmSenderDriverWindows()
{
	iDeviceEnumerator->UnregisterEndpointNotificationCallback(this);
	iDeviceEnumerator->Release();
}

void OhmSenderDriverWindows::SetSongcaster(Songcaster& aSongcaster)
{
	iSongcaster = &aSongcaster;
	iDeviceEnumerator->RegisterEndpointNotificationCallback(this);
}

TBool OhmSenderDriverWindows::FindEndpoint(const char* aManufacturer)
{
	bool uninitialise = true;

	HRESULT hr = CoInitialize(NULL);

	printf("CoInitialize %d\n", hr);

	if (hr == 0x80010106)
	{
		uninitialise = false;
	}
	else if (!SUCCEEDED(hr))
	{
		return (false);
	}

	// Create a multimedia device enumerator.
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&iDeviceEnumerator);
		
	printf("CoCreateInstance %d\n", hr);
	
	if (SUCCEEDED(hr))
	{
		IMMDeviceCollection *pDevices;
		
		// Enumerate the output devices.
			
		hr = iDeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATEMASK_ALL | 0x10000000, &pDevices);
			
		printf("EnumAudioEndpoints %d\n", hr);

		if (SUCCEEDED(hr))
		{
			UINT count;
			
			pDevices->GetCount(&count);
            printf("count: %d\n", count);
				
			if (SUCCEEDED(hr))
			{
				for (unsigned i = 0; i < count; i++)
				{
					IMMDevice *pDevice;
				
					hr = pDevices->Item(i, &pDevice);
						
					if (SUCCEEDED(hr))
					{
						LPWSTR wstrID = NULL;
						
						hr = pDevice->GetId(&wstrID);
                            
                        wprintf(L"id: %s\n", wstrID);
							
						if (SUCCEEDED(hr))
						{
							IPropertyStore *pStore;
							
							hr = pDevice->OpenPropertyStore(STGM_READ, &pStore);
								
							if (SUCCEEDED(hr))
							{
                                PROPVARIANT var;
                                PropVariantInit(&var);
                                hr = pStore->GetValue(PKEY_AudioEndpoint_GUID, &var);
                                wprintf(L"%s\n", var.pwszVal);
                                
								PROPVARIANT friendlyName;
								
								PropVariantInit(&friendlyName);
									
								hr = pStore->GetValue(PKEY_DeviceInterface_FriendlyName, &friendlyName);
                                    
                                wprintf(L"friendlyName: %s\n", friendlyName.pwszVal);
									
								if (SUCCEEDED(hr))
								{
									wchar_t model[200];

									MultiByteToWideChar(CP_ACP, 0, aManufacturer, -1, model, sizeof(model));

									wcscpy(model + wcslen(model), L" Songcaster");

									if (!wcscmp(friendlyName.pwszVal, model))
									{
										wcscpy(iEndpointId, wstrID);
										PropVariantClear(&friendlyName);
										pStore->Release();
										pDevice->Release();
										pDevices->Release();
										if (uninitialise) {
											CoUninitialize();
										}
										return (true);
									}

									PropVariantClear(&friendlyName);
								}

								pStore->Release();
							}
                                
                            CoTaskMemFree(wstrID);
						}
						pDevice->Release();
					}
				}
			}
			pDevices->Release();
		}
		iDeviceEnumerator->Release();
	}

	if (uninitialise) {
		CoUninitialize();
	}

	return (false);
}

void OhmSenderDriverWindows::SetDefaultAudioPlaybackDevice()
{	
	IPolicyConfigVista *pPolicyConfig;
	
    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);

	if (SUCCEEDED(hr))
	{
		hr = pPolicyConfig->SetDefaultEndpoint(iEndpointId, eConsole);
		pPolicyConfig->Release();
	}
}

void OhmSenderDriverWindows::SetEndpointEnabled(TBool aValue)
{	
	HRESULT hr;
	
	// The following works if we are on vista ...

	IPolicyConfigVista *pPolicyConfig;
	
    hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);

	if (SUCCEEDED(hr))
	{
		pPolicyConfig->SetEndpointVisibility(iEndpointId, aValue ? 1 : 0);
		pPolicyConfig->Release();
	}

	// ... and the following works if we are on Windows 7

	IPolicyConfig *pPolicyConfig2;
	
    hr = CoCreateInstance(__uuidof(CPolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfig), (LPVOID *)&pPolicyConfig2);

	if (SUCCEEDED(hr))
	{
		pPolicyConfig2->SetEndpointVisibility(iEndpointId, aValue ? 1 : 0);
		pPolicyConfig2->Release();
	}
}

TBool OhmSenderDriverWindows::FindDriver(const char* aDomain)
{
	char hwid[200];

	strcpy(hwid, aDomain);
	strcpy(hwid + strlen(hwid), "/songcaster");

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&KSCATEGORY_AUDIO, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PROFILE | DIGCF_PRESENT);

	if (deviceInfoSet == 0) {
		return (false);
	}

	SP_DEVICE_INTERFACE_DATA deviceInterfaceData;

	deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	// build a DevInfo Data structure

	SP_DEVINFO_DATA deviceInfoData;

	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		 
	// build a Device Interface Detail Data structure

	TByte* detail = new TByte[1000];

	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)detail;

	deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    for (TUint i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, 0, &KSCATEGORY_AUDIO, i, &deviceInterfaceData); i++)
	{
		// now we can get some more detailed information
        
        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceInterfaceDetailData, 1000, 0, &deviceInfoData))
        {
            char buffer[1024];
            if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, NULL, (LPBYTE)buffer, 1024, NULL)) {
                printf("%s\n", buffer);
                if(strcmp(hwid, buffer) == 0) {
                    try
                    {
                        printf("Found a valid driver: %s\n", deviceInterfaceDetailData->DevicePath);

                        iHandle = CreateFile(deviceInterfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

                        KSPROPERTY prop;
                        
                        prop.Set = OHSOUNDCARD_GUID;
                        prop.Id = KSPROPERTY_OHSOUNDCARD_VERSION;
                        prop.Flags = KSPROPERTY_TYPE_GET;

                        TByte buffer[4];

                        DWORD bytes;

                        if (DeviceIoControl(iHandle, IOCTL_KS_PROPERTY, &prop, sizeof(KSPROPERTY), buffer, sizeof(buffer), &bytes, 0))
                        {
                            TUint version = *(TUint*)buffer;

                            if (version == 1) {
                                delete [] detail;
                                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                                return (true);
                            }

							printf("Invalid driver version %d\n", version);
                        }
						else {
							printf("DeviceIoControl error\n");
						}

                    }
                    catch (...)
                    {
                    }
                }
            }
        }
	}

	delete [] detail;

	SetupDiDestroyDeviceInfoList(deviceInfoSet);

	return (false);
}

void OhmSenderDriverWindows::SetEnabled(TBool aValue)
{
	iEnabled = aValue;

    KSPROPERTY prop;
				
	prop.Set = OHSOUNDCARD_GUID;
    prop.Id = KSPROPERTY_OHSOUNDCARD_ENABLED;
    prop.Flags = KSPROPERTY_TYPE_SET;

    DWORD bytes;

	DWORD value = aValue ? 1 : 0;

    DeviceIoControl(iHandle, IOCTL_KS_PROPERTY, &prop, sizeof(KSPROPERTY), &value, sizeof(value), &bytes, 0);

	SetEndpointEnabled(aValue);

	if (aValue)
	{
		SetDefaultAudioPlaybackDevice();
	}
}

void OhmSenderDriverWindows::SetActive(TBool  aValue)
{
	KSPROPERTY prop;
				
	prop.Set = OHSOUNDCARD_GUID;
    prop.Id = KSPROPERTY_OHSOUNDCARD_ACTIVE;
    prop.Flags = KSPROPERTY_TYPE_SET;

    DWORD bytes;

	DWORD value = aValue ? 1 : 0;

	DeviceIoControl(iHandle, IOCTL_KS_PROPERTY, &prop, sizeof(KSPROPERTY), &value, sizeof(value), &bytes, 0);
}

void OhmSenderDriverWindows::SetEndpoint(const Endpoint& aEndpoint)
{
	KSPROPERTY prop;
				
	prop.Set = OHSOUNDCARD_GUID;
    prop.Id = KSPROPERTY_OHSOUNDCARD_ENDPOINT;
    prop.Flags = KSPROPERTY_TYPE_SET;

	TByte buffer[8];

	ULONG* ptr = (ULONG*)buffer;
	
	*ptr++ = Arch::BigEndian4(aEndpoint.Address());
	*ptr++ = aEndpoint.Port();

    DWORD bytes;

    DeviceIoControl(iHandle, IOCTL_KS_PROPERTY, &prop, sizeof(KSPROPERTY), &buffer, sizeof(buffer), &bytes, 0);
}


void OhmSenderDriverWindows::SetTtl(TUint aValue)
{
    KSPROPERTY prop;
				
	prop.Set = OHSOUNDCARD_GUID;
    prop.Id = KSPROPERTY_OHSOUNDCARD_TTL;
    prop.Flags = KSPROPERTY_TYPE_SET;

    DWORD bytes;

    DeviceIoControl(iHandle, IOCTL_KS_PROPERTY, &prop, sizeof(KSPROPERTY), &aValue, sizeof(aValue), &bytes, 0);
}

void OhmSenderDriverWindows::SetTrackPosition(TUint64 /*aSamplesTotal*/, TUint64 /*aSampleStart*/)
{
}

ULONG STDCALL OhmSenderDriverWindows::AddRef()
{
    return (InterlockedIncrement(&iRefCount));
}

ULONG STDCALL OhmSenderDriverWindows::Release()
{
	return (InterlockedDecrement(&iRefCount));
}

HRESULT STDCALL OhmSenderDriverWindows::QueryInterface(REFIID aId, VOID** aInterface)
{
    if (aId == IID_IUnknown)
    {
        AddRef();
        *aInterface = (IUnknown*)this;
    }
    else if (aId == __uuidof(IMMNotificationClient))
    {
        AddRef();
        *aInterface = (IMMNotificationClient*)this;
    }
    else
    {
        *aInterface = NULL;
        return E_NOINTERFACE;
    }

    return S_OK;
}

// Callback methods for device-event notifications.

HRESULT STDCALL OhmSenderDriverWindows::OnDefaultDeviceChanged(EDataFlow aFlow, ERole aRole, LPCWSTR aDeviceId)
{
	if (aFlow == eRender && aRole == eMultimedia) {
		TBool enabled = (wcscmp(iEndpointId, aDeviceId) == 0);
		iSongcaster->SetEnabled(enabled);
	}

    return S_OK;
}

HRESULT STDCALL OhmSenderDriverWindows::OnDeviceAdded(LPCWSTR /*aDeviceId*/)
{
    return S_OK;
};

HRESULT STDCALL OhmSenderDriverWindows::OnDeviceRemoved(LPCWSTR /*aDeviceId*/)
{
    return S_OK;
}

HRESULT STDCALL OhmSenderDriverWindows::OnDeviceStateChanged(LPCWSTR aDeviceId, DWORD aNewState)
{
	if ((wcscmp(iEndpointId, aDeviceId) == 0)) {
		switch (aNewState) {
		case DEVICE_STATE_ACTIVE:
			iSongcaster->SetEnabled(true);
			break;
		case DEVICE_STATE_DISABLED:
		case DEVICE_STATE_NOTPRESENT:
		case DEVICE_STATE_UNPLUGGED:
			iSongcaster->SetEnabled(false);
		default:
			break;
		}
	}

	return S_OK;
}

HRESULT STDCALL OhmSenderDriverWindows::OnPropertyValueChanged(LPCWSTR /*aDeviceId*/, const PROPERTYKEY /*aKey*/)
{
    return S_OK;
}