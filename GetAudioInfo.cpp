// GetAudioInfo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//



#include <wrl.h>

#include <initguid.h>
#include <Mmdeviceapi.h>

#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Devicetopology.h>
#include <Endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>


#include <ObjIdl.h>
#include <SetupAPI.h>
#include <Cfgmgr32.h>
#include <propkey.h>

#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>

#include <vector>

#include <Devpkey.h>


#include <iostream>
#include <Windows.h>




#pragma comment(lib,"Setupapi.lib")
#pragma comment(lib,"onecore.lib")


using namespace std;
using namespace Microsoft::WRL;

#define PARTID_MASK 0x0000ffff

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);


const IID IID_IKsControl = __uuidof(IKsControl);

const IID IID_IDeviceTopology = __uuidof(IDeviceTopology);
const IID IID_IPart = __uuidof(IPart);

const IID IID_IAudioInputSelector = __uuidof(IAudioInputSelector);

const IID IID_IConnector = __uuidof(IConnector);





struct EndPointInfo
{
    wchar_t deviceFriendlyName[256] = {0};
    wchar_t adapterFriendlyName[256] = {0};
    wchar_t driverProvider[256] = {0};
    wchar_t driverDate[256] = {0};
    wchar_t driverBinaryPath[256] = {0};
    wchar_t driverVersion[256] = {0};
    wchar_t driverDesc[256] = {0};
    int numOfChannels;
    int bitsPerSample;
    int samplesPerSec;
    float volume;
};

//-----------------------------------------------------------
// This function enumerates all active (plugged in) audio
// rendering endpoint devices. It prints the friendly name
// and endpoint ID string of each endpoint device.
//-----------------------------------------------------------


int GetEndpointInfo(vector<EndPointInfo> * vecEndPointInfo, EDataFlow RenderOrCapture = eRender)
{
    HRESULT hr = S_OK;
    ComPtr<IMMDeviceEnumerator> pEnumerator = NULL;
    ComPtr<IMMDeviceCollection> pCollection = NULL;
    ComPtr<IMMDevice> pEndpoint = NULL;
    ComPtr<IPropertyStore> pProps = NULL;
    ComPtr<IAudioEndpointVolume> pEndpoint_volume = NULL;

    LPWSTR pwszID = NULL;
    EndPointInfo endPointInfo;
    EndPointInfo* pEndPointInfo = &endPointInfo;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->EnumAudioEndpoints(
            RenderOrCapture, DEVICE_STATE_ACTIVE,
            &pCollection);
    EXIT_ON_ERROR(hr)

    UINT  count;
    hr = pCollection->GetCount(&count);
    EXIT_ON_ERROR(hr)

    if (count == 0)
    {
        printf("No endpoints found.\n");
    }



    // Each loop prints the name of an endpoint device.
    for (ULONG i = 0; i < count; i++)
    {

        // Get pointer to endpoint number i.
        hr = pCollection->Item(i, &pEndpoint);
        EXIT_ON_ERROR(hr)

        // Get the endpoint ID string.
        hr = pEndpoint->GetId(&pwszID);
        EXIT_ON_ERROR(hr)

        hr = pEndpoint->OpenPropertyStore(
            STGM_READ, &pProps);
        EXIT_ON_ERROR(hr)

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
            PKEY_Device_FriendlyName, &varName);
        EXIT_ON_ERROR(hr)


        wcscpy(pEndPointInfo->deviceFriendlyName, varName.pwszVal);

        PropVariantClear(&varName);



        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
            PKEY_DeviceInterface_FriendlyName, &varName);
        EXIT_ON_ERROR(hr)

        wcscpy(pEndPointInfo->adapterFriendlyName, varName.pwszVal);



        //通过adapter name检索驱动信息

        {


            HDEVINFO classInfo = SetupDiGetClassDevsEx(&GUID_DEVCLASS_MEDIA, NULL, NULL, DIGCF_PRESENT, NULL, NULL, NULL);

            if (classInfo == INVALID_HANDLE_VALUE) {
                //return ;
                EXIT_ON_ERROR(hr)
            }

            SP_DEVINFO_DATA deviceInfo;
            deviceInfo.cbSize = sizeof(SP_DEVINFO_DATA);
            //bool ret = SetupDiEnumDeviceInfo(classInfo, 0, &deviceInfo);
            //int en = GetLastError();
            for (int devIndex = 0; SetupDiEnumDeviceInfo(classInfo, devIndex, &deviceInfo); devIndex++)
            {

                
                    

                DEVPROPKEY devKey = DEVPKEY_Device_DriverDesc, devKey1 = DEVPKEY_Device_FriendlyName;
                DEVPROPTYPE propertyType, propertyType1;
                BYTE propertyBuffer[256] = { 0 },propertyBuffer1[256] = { 0 };
                unsigned long bufferSize = 256;
                CM_Get_DevNode_Property(deviceInfo.DevInst, &devKey, &propertyType, propertyBuffer, &bufferSize, 0);
                CM_Get_DevNode_Property(deviceInfo.DevInst, &devKey1, &propertyType1, propertyBuffer1, &bufferSize, 0);

                    

                if (
                    0 != wcscmp(varName.pwszVal, (wchar_t*)propertyBuffer) &&
                    0 != wcscmp(varName.pwszVal, (wchar_t*)propertyBuffer1)
                        
                    )
                {
                    continue;
                }
                else
                {
                    bool isExistService = false;
                    ULONG size;
                    HRESULT hr = CM_Get_Device_ID_Size(&size, deviceInfo.DevInst, 0);
                    if (hr == CR_SUCCESS)
                    {
                        std::wstring buffer(size + 1, '\0');
                        hr = CM_Get_Device_ID(deviceInfo.DevInst, (PWSTR)buffer.c_str(), size + 1, 0);

                        // examples:
                        // PX4: USB\VID_26AC&PID_0011\0
                        // FTDI cable: FTDIBUS\VID_0403+PID_6001+FTUAN9UJA\0000"
                        //printf("Found: %S\n", buffer.c_str());

                        DWORD keyCount = 0;
                        if (!SetupDiGetDevicePropertyKeys(classInfo, &deviceInfo, NULL, 0, &keyCount, 0)) {
                            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                                continue;
                            }
                        }

                        DEVPROPKEY* keyArray = new DEVPROPKEY[keyCount];

                        if (SetupDiGetDevicePropertyKeys(classInfo, &deviceInfo, keyArray, keyCount, &keyCount, 0))
                        {

                                
                            //cout << "---------------------------------------------" << endl;
                            for (DWORD j = 0; j < keyCount; j++)
                            {
                                DEVPROPKEY* key = &keyArray[j];
                                if (*key == DEVPKEY_Device_DriverDesc ||
                                    *key == DEVPKEY_Device_Service ||
                                    *key == DEVPKEY_Device_FriendlyName ||
                                    *key == DEVPKEY_Device_DriverVersion ||
                                    *key == DEVPKEY_Device_DriverProvider ||
                                    *key == DEVPKEY_Device_DriverDate
                                    )
                                {
                                    ULONG bufferSize = 0;
                                    DEVPROPTYPE propertyType;
                                    CM_Get_DevNode_Property(deviceInfo.DevInst, &keyArray[j], &propertyType, NULL, &bufferSize, 0);
                                    if (bufferSize > 0) {
                                        BYTE* propertyBuffer = new BYTE[bufferSize];
                                        hr = CM_Get_DevNode_Property(deviceInfo.DevInst, &keyArray[j], &propertyType, propertyBuffer, &bufferSize, 0);
                                        if (hr == CR_SUCCESS) {


                                            if (*key == DEVPKEY_Device_DriverDate)
                                            {
                                                wchar_t str[256] = { 0 };
                                                FILETIME ftUTC = *((FILETIME*)propertyBuffer);
                                                SYSTEMTIME stUTC2;
                                                FileTimeToSystemTime(&ftUTC, &stUTC2);
                                                wsprintf(str,L"%d-%d-%d %d:%d:%d", stUTC2.wYear, stUTC2.wMonth,
                                                    stUTC2.wDay, stUTC2.wHour, stUTC2.wMinute, stUTC2.wSecond);

                                                //pEndPointInfo->driverDate = str;

                                                wcscpy(pEndPointInfo->driverDate, str);

                                            }
                                            else if (*key == DEVPKEY_Device_Service)
                                            {
                                                isExistService = true;
                                                const std::wstring kServiceKeyPath =
                                                    L"SYSTEM\\CurrentControlSet\\Services\\";
                                                //L"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\";

                                                std::wstring serviceName((WCHAR*)propertyBuffer);
                                                //wcout << serviceName << endl;

                                                wstring regPath = kServiceKeyPath + serviceName;


                                                HKEY hKey = NULL;
                                                //打开注册表
                                                long result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey);
                                                if (result != ERROR_SUCCESS) {
                                                    printf("call RegOpenKeyEx failure, result=%d\n", result);
                                                }

                                                //读字符串
                                                std::string ip;
                                                DWORD dataTypeIp = REG_SZ;
                                                BYTE addressValue[256] = { 0 };
                                                DWORD ipAddressLength = sizeof(addressValue);
                                                result = RegQueryValueEx(hKey, L"ImagePath", NULL, &dataTypeIp, addressValue, &ipAddressLength);
                                                if (result != ERROR_SUCCESS) {
                                                    if (hKey != 0) {
                                                        RegCloseKey(hKey);
                                                    }
                                                }

                                                //关闭注册表
                                                if (hKey != 0) {
                                                    RegCloseKey(hKey);
                                                }

                                                wcscpy(pEndPointInfo->driverBinaryPath, (wchar_t*)addressValue);
                                            }
                                            else if (*key == DEVPKEY_Device_DriverVersion)
                                            {
                                                wcscpy(pEndPointInfo->driverVersion, (WCHAR*)propertyBuffer);
                                            }
                                            else if (*key == DEVPKEY_Device_DriverProvider)
                                            {

                                                wcscpy(pEndPointInfo->driverProvider, (WCHAR*)propertyBuffer);
                                            }
                                            else if(*key == DEVPKEY_Device_DriverDesc)
                                            {
                                                wcscpy(pEndPointInfo->driverDesc, (WCHAR*)propertyBuffer);
                                            }
                                        }
                                        delete[] propertyBuffer;
                                    }
                                }
                            }
                        }
                        delete[] keyArray;
                    }

                    if (isExistService == false)
                    {
                        continue;
                    }
                    break;
                }
            }

            SetupDiDestroyDeviceInfoList(classInfo);
        }



        PropVariantClear(&varName);

        //获取share模式下的格式

        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
            PKEY_AudioEngine_DeviceFormat, &varName);
        EXIT_ON_ERROR(hr)


        pEndPointInfo->numOfChannels = ((WAVEFORMATEXTENSIBLE*)(varName.blob.pBlobData))->Format.nChannels;
        pEndPointInfo->bitsPerSample = ((WAVEFORMATEXTENSIBLE*)(varName.blob.pBlobData))->Samples.wValidBitsPerSample;
        pEndPointInfo->samplesPerSec = ((WAVEFORMATEX*)(varName.blob.pBlobData))->nSamplesPerSec;



        PropVariantClear(&varName);

        //获取厂商名称  PKEY_Device_Manufacturer

        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
            PKEY_Device_InstanceId, &varName);
        EXIT_ON_ERROR(hr)

        PropVariantClear(&varName);

        //接下来获取音量

        // 创建IAudioEndpointVolume对象
        hr = pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
            NULL, (void**)&pEndpoint_volume);

        float volume_sacle;
        pEndpoint_volume->GetMasterVolumeLevelScalar(&volume_sacle);
        //return volume_sacle;


        pEndPointInfo->volume = volume_sacle;
        //cout << "音量 ： " << volume_sacle << endl;


        vecEndPointInfo->push_back(endPointInfo);



        CoTaskMemFree(pwszID);
        pwszID = NULL;
        PropVariantClear(&varName);
    }
    return 0;
    

Exit:
    printf("Error!\n");
    CoTaskMemFree(pwszID);
    return -1;

}

int main()
{
    setlocale(LC_ALL, "chs");
    CoInitializeEx(NULL, COINIT_MULTITHREADED);


    vector<EndPointInfo> vecEndPointInfo_render;
    vector<EndPointInfo> vecEndPointInfo_capture;

    int ret = 0;

    ret = GetEndpointInfo(&vecEndPointInfo_render,eRender);
    ret = GetEndpointInfo(&vecEndPointInfo_capture, eCapture);

    std::cout << "Hello World!\n";


    
}

