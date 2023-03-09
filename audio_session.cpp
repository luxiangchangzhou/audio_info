#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Devicetopology.h>
#include <Endpointvolume.h>
#include <spatialaudioclient.h>

#include <winternl.h>

#include <tlhelp32.h>

#include <iostream>
#include <stdio.h>

#include <wrl.h>
#include <propkey.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <vector>

using namespace Microsoft::WRL;

using namespace std;











#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);





NTSTATUS (*LXNtQueryInformationProcess)(
    HANDLE           ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID            ProcessInformation,
    ULONG            ProcessInformationLength,
    PULONG           ReturnLength
);




NTSTATUS(* LXNtReadVirtualMemory)(
    IN HANDLE               ProcessHandle,
    IN PVOID                BaseAddress,
    OUT PVOID               Buffer,
    IN ULONG                NumberOfBytesToRead,
    OUT PULONG              NumberOfBytesReaded OPTIONAL);



struct SessionInfo
{
    wchar_t deviceName[256] = {0};
    unsigned long pid = 0;
    float volume = 0;
    wchar_t imagePath[256] = {0};
    wchar_t cmdLine[256] = {0};

};


BOOL GetProcessFullPathByProcessID(ULONG32 ProcessID, WCHAR* BufferData, ULONG BufferLegnth) {
    HANDLE	ProcessHandle = NULL;
    ProcessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ProcessID);

    if (ProcessHandle == NULL) {
        return FALSE;
    }


    PROCESS_BASIC_INFORMATION	pbi = { 0 };
    SIZE_T	ReturnLength = 0;
    NTSTATUS status = LXNtQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION),
        (PULONG)&ReturnLength);

    if (!NT_SUCCESS(status)) {
        CloseHandle(ProcessHandle);
        ProcessHandle = NULL;
        return FALSE;
    }

    PEB	Peb = { 0 };
    status = LXNtReadVirtualMemory(ProcessHandle, pbi.PebBaseAddress, &Peb, sizeof(PEB), (PULONG)&ReturnLength);

    if (!NT_SUCCESS(status)) {
        CloseHandle(ProcessHandle);
        ProcessHandle = NULL;
        return FALSE;
    }

    RTL_USER_PROCESS_PARAMETERS RtlUserProcessParameters = { 0 };
    status = LXNtReadVirtualMemory(ProcessHandle, Peb.ProcessParameters, &RtlUserProcessParameters,
        sizeof(RTL_USER_PROCESS_PARAMETERS), (PULONG)&ReturnLength);

    if (!NT_SUCCESS(status)) {
        CloseHandle(ProcessHandle);
        ProcessHandle = NULL;
        return FALSE;
    }

    if (RtlUserProcessParameters.ImagePathName.Buffer != NULL)
    {
        ULONG v1 = 0;
        if (RtlUserProcessParameters.ImagePathName.Length < BufferLegnth)
        {
            v1 = RtlUserProcessParameters.ImagePathName.Length;
        }
        else
        {
            v1 = BufferLegnth - 10;
        }
        status = ReadProcessMemory(ProcessHandle, RtlUserProcessParameters.ImagePathName.Buffer,
            BufferData, v1, (SIZE_T*)&ReturnLength);
        if (!NT_SUCCESS(status))
        {
            CloseHandle(ProcessHandle);
            ProcessHandle = NULL;
            return FALSE;
        }
    }

    CloseHandle(ProcessHandle);
    return TRUE;
}



BOOL GetCmdLineByProcessID(ULONG32 ProcessID, WCHAR* BufferData, ULONG BufferLegnth) {
    HANDLE	ProcessHandle = NULL;
    ProcessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ProcessID);

    if (ProcessHandle == NULL) {
        return FALSE;
    }


    PROCESS_BASIC_INFORMATION	pbi = { 0 };
    SIZE_T	ReturnLength = 0;
    NTSTATUS status = LXNtQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION),
        (PULONG)&ReturnLength);

    if (!NT_SUCCESS(status)) {
        CloseHandle(ProcessHandle);
        ProcessHandle = NULL;
        return FALSE;
    }

    PEB	Peb = { 0 };
    status = LXNtReadVirtualMemory(ProcessHandle, pbi.PebBaseAddress, &Peb, sizeof(PEB), (PULONG)&ReturnLength);

    if (!NT_SUCCESS(status)) {
        CloseHandle(ProcessHandle);
        ProcessHandle = NULL;
        return FALSE;
    }

    RTL_USER_PROCESS_PARAMETERS RtlUserProcessParameters = { 0 };
    status = LXNtReadVirtualMemory(ProcessHandle, Peb.ProcessParameters, &RtlUserProcessParameters,
        sizeof(RTL_USER_PROCESS_PARAMETERS), (PULONG)&ReturnLength);

    if (!NT_SUCCESS(status)) {
        CloseHandle(ProcessHandle);
        ProcessHandle = NULL;
        return FALSE;
    }

    if (RtlUserProcessParameters.CommandLine.Buffer != NULL)
    {
        ULONG v1 = 0;
        if (RtlUserProcessParameters.CommandLine.Length < BufferLegnth)
        {
            v1 = RtlUserProcessParameters.CommandLine.Length;
        }
        else
        {
            v1 = BufferLegnth - 10;
        }
        status = ReadProcessMemory(ProcessHandle, RtlUserProcessParameters.CommandLine.Buffer,
            BufferData, v1, (SIZE_T*)&ReturnLength);
        if (!NT_SUCCESS(status))
        {
            CloseHandle(ProcessHandle);
            ProcessHandle = NULL;
            return FALSE;
        }
    }

    CloseHandle(ProcessHandle);
    return TRUE;
}




int enumDevicesSessionInfo(vector<SessionInfo>* vecSessionInfo, EDataFlow RenderOrCapture = eRender)
{

    HRESULT hr = S_OK;
    ComPtr < IMMDeviceEnumerator> pEnumerator = NULL;
    ComPtr < IMMDeviceCollection> pCollection = NULL;
    ComPtr < IMMDevice> pEndpoint = NULL;
    ComPtr < IPropertyStore> pProps = NULL;
    LPWSTR pwszID = NULL;

    HMODULE hNtDll = GetModuleHandle(L"ntdll.dll");

    SessionInfo sessionInfo;

    LXNtReadVirtualMemory = 
        (

            NTSTATUS(*)(
                HANDLE               ProcessHandle,
                PVOID                BaseAddress,
                PVOID               Buffer,
                ULONG                NumberOfBytesToRead,
                PULONG              NumberOfBytesReaded)
            )
        GetProcAddress(hNtDll, "NtReadVirtualMemory");

    LXNtQueryInformationProcess =
        (
            __kernel_entry NTSTATUS(*)(
                HANDLE           ProcessHandle,
                PROCESSINFOCLASS ProcessInformationClass,
                PVOID            ProcessInformation,
                ULONG            ProcessInformationLength,
                PULONG           ReturnLength
                )
            )GetProcAddress(hNtDll, "NtQueryInformationProcess");


    if (LXNtReadVirtualMemory==NULL|| LXNtQueryInformationProcess == NULL)
    {
        return -1;
    }

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

        wcscpy(sessionInfo.deviceName, varName.pwszVal);

        ComPtr<IAudioSessionManager2> pSessionManager2;
        ComPtr<IAudioSessionEnumerator> pSessionList;
        int cbSessionCount = 0;


        hr = pEndpoint->Activate(
            __uuidof(IAudioSessionManager2), CLSCTX_ALL,
            NULL, (void**)&pSessionManager2);



        // Get the current list of sessions.
        hr = pSessionManager2->GetSessionEnumerator(&pSessionList);


        hr = pSessionList->GetCount(&cbSessionCount);


        for (int index = 0; index < cbSessionCount; index++)
        {

            ComPtr<IAudioSessionControl> pSessionControl;
            ComPtr<IAudioSessionControl2> pSessionControl2;
            ComPtr<ISimpleAudioVolume> pSimpleAudioVolume;

            LPWSTR pswSession = NULL;

            // Get the <n>th session.
            hr = pSessionList->GetSession(index, &pSessionControl);
            hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pSimpleAudioVolume);
            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
            pSimpleAudioVolume->GetMasterVolume(&sessionInfo.volume);
            //pSimpleAudioVolume->SetMasterVolume(0.8,0);



            pSessionControl2->GetProcessId(&sessionInfo.pid);
            if (sessionInfo.pid == 0)
            {
                continue;//这边忽略系统声音
            }


            //拿到PID就可以获取cmdline和imagePath了




            GetProcessFullPathByProcessID(sessionInfo.pid, sessionInfo.imagePath, 256);

            GetCmdLineByProcessID(sessionInfo.pid, sessionInfo.cmdLine, 256);



            vecSessionInfo->push_back(sessionInfo);


        }

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
    



    CoInitialize(0);

    setlocale(LC_ALL, "chs");


    vector<SessionInfo> vecSessionInfo;



    int ret = enumDevicesSessionInfo(&vecSessionInfo, eRender);
    ret = enumDevicesSessionInfo(&vecSessionInfo, eCapture);



    return 0;














    //HRESULT hr;
    //int cbSessionCount = 0;
    //
    //float volume = 0;
    //DWORD pid = 0;

    //ComPtr<IMMDeviceEnumerator> pEnumerator;
    //ComPtr<IMMDevice> pDevice;
    //ComPtr<IAudioSessionEnumerator> pSessionList;
    //
    //
    //
    //ComPtr<IAudioSessionManager2> pSessionManager2;


    //


    //hr = CoCreateInstance(
    //    __uuidof(MMDeviceEnumerator), NULL,
    //    CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
    //    (void**)&pEnumerator);
 
    //hr = pEnumerator->GetDefaultAudioEndpoint(
    //    eCapture, eConsole, &pDevice);

    //    //hr = pDevice->Activate(
    //    //    IID_ISpatialAudioClient, CLSCTX_ALL,
    //    //    NULL, (void**)&pAudioClient);


    //hr = pDevice->Activate(
    //    __uuidof(IAudioSessionManager2), CLSCTX_ALL,
    //    NULL, (void**)&pSessionManager2);



    //// Get the current list of sessions.
    //hr = pSessionManager2->GetSessionEnumerator(&pSessionList);


    //hr = pSessionList->GetCount(&cbSessionCount);


    //for (int index = 0; index < cbSessionCount; index++)
    //{

    //    ComPtr<IAudioSessionControl> pSessionControl;
    //    ComPtr<IAudioSessionControl2> pSessionControl2;
    //    ComPtr<ISimpleAudioVolume> pSimpleAudioVolume;

    //    LPWSTR pswSession = NULL;

    //    // Get the <n>th session.
    //    hr = pSessionList->GetSession(index, &pSessionControl);
    //    hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pSimpleAudioVolume);
    //    hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
    //    pSimpleAudioVolume->GetMasterVolume(&volume);
    //    //pSimpleAudioVolume->SetMasterVolume(0.8,0);



    //    pSessionControl2->GetProcessId(&pid);

    //    hr = pSessionControl->GetDisplayName(&pswSession);

    //    wprintf_s(L"pid: %d\n", pid);
    //    wprintf_s(L"volume : %f\n", volume);

    //    CoTaskMemFree(pswSession);

    //}

    //CoUninitialize();


    //std::cout << "Hello World!\n";
}



















