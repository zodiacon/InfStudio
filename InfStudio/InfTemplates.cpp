#include "pch.h"
#include "InfTemplates.h"

#include <format>
#include <algorithm>

//
// Every template must pass "infverif /w" (Windows Driver requirements) once generated; InfStudio.Tests checks it when the
// WDK is installed. Driver files are copied to the driver store (DIRID 13) and the services run from there.
//
namespace {
	constexpr char PciKmdf[] = R"(;
; @NAME@.inf - a KMDF driver for a PCI device
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the device's hardware ID: its vendor and device IDs (and perhaps SUBSYS_ and REV_)
[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyDevice_Install, @HWID@

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyDevice_Install, @HWID@

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
@NAME@.sys

[MyDevice_Install.NT.HW]
AddReg = MyDevice_HW_AddReg

[MyDevice_HW_AddReg]
; message-signaled interrupts (MSI/MSI-X)
HKR, "Interrupt Management",, 0x00000010
HKR, "Interrupt Management\MessageSignaledInterruptProperties",, 0x00000010
HKR, "Interrupt Management\MessageSignaledInterruptProperties", MSISupported, 0x00010001, 1

[MyDevice_Install.NT.Services]
AddService = @NAME@, %SPSVCINST_ASSOCSERVICE%, MyDevice_Service_Install

[MyDevice_Service_Install]
DisplayName   = %ServiceName%
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL
ServiceBinary = %13%\@NAME@.sys

[MyDevice_Install.NT.Wdf]
KmdfService = @NAME@, MyDevice_Wdf

[MyDevice_Wdf]
KmdfLibraryVersion = 1.15

[Strings]
SPSVCINST_ASSOCSERVICE = 0x00000002
ManufacturerName       = "@MFG@"
DiskName               = "@NAME@ Installation Disk"
DeviceDesc             = "@DESC@"
ServiceName            = "@NAME@ Service"
)";

	constexpr char UsbWinUsb[] = R"(;
; @NAME@.inf - a USB device that uses WinUSB (no driver of its own; an application talks to it with WinUsb_* functions)
;

[Version]
Signature   = "$Windows NT$"
Class       = USBDevice
ClassGuid   = {88bae032-5a81-49f0-bc3d-a4ff138216d6}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the device's hardware ID: its vendor and product IDs (and perhaps &MI_xx for an interface of a composite device)
[Standard.NTamd64.10.0...16299]
%DeviceName% = MyDevice_Install, @HWID@

[Standard.NTarm64.10.0...16299]
%DeviceName% = MyDevice_Install, @HWID@

[MyDevice_Install.NT]
Include = winusb.inf
Needs   = WINUSB.NT

[MyDevice_Install.NT.Services]
Include = winusb.inf
Needs   = WINUSB.NT.Services

[MyDevice_Install.NT.HW]
AddReg = MyDevice_HW_AddReg

[MyDevice_HW_AddReg]
; the interface an application opens the device with (SetupDiGetClassDevs, CM_Get_Device_Interface_List)
HKR,, DeviceInterfaceGUIDs, 0x00010000, "@GUID@"

[Strings]
ManufacturerName = "@MFG@"
DeviceName       = "@DESC@"
)";

	constexpr char UsbKmdf[] = R"(;
; @NAME@.inf - a KMDF driver for a USB device
;

[Version]
Signature   = "$Windows NT$"
Class       = USBDevice
ClassGuid   = {88bae032-5a81-49f0-bc3d-a4ff138216d6}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the device's hardware ID: its vendor and product IDs (and perhaps &MI_xx for an interface of a composite device)
[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyDevice_Install, @HWID@

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyDevice_Install, @HWID@

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
@NAME@.sys

[MyDevice_Install.NT.Services]
AddService = @NAME@, %SPSVCINST_ASSOCSERVICE%, MyDevice_Service_Install

[MyDevice_Service_Install]
DisplayName   = %ServiceName%
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL
ServiceBinary = %13%\@NAME@.sys

[MyDevice_Install.NT.Wdf]
KmdfService = @NAME@, MyDevice_Wdf

[MyDevice_Wdf]
KmdfLibraryVersion = 1.15

[Strings]
SPSVCINST_ASSOCSERVICE = 0x00000002
ManufacturerName       = "@MFG@"
DiskName               = "@NAME@ Installation Disk"
DeviceDesc             = "@DESC@"
ServiceName            = "@NAME@ Service"
)";

	constexpr char RootKmdf[] = R"(;
; @NAME@.inf - a KMDF driver for a software (root-enumerated) device, which has no hardware
;
; Create the device and install the driver with:
;   devgen /add /hardwareid "@HWID@"
;   pnputil /add-driver @NAME@.inf /install
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyDevice_Install, @HWID@

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyDevice_Install, @HWID@

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
@NAME@.sys

[MyDevice_Install.NT.Services]
AddService = @NAME@, %SPSVCINST_ASSOCSERVICE%, MyDevice_Service_Install

[MyDevice_Service_Install]
DisplayName   = %ServiceName%
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL
ServiceBinary = %13%\@NAME@.sys

[MyDevice_Install.NT.Wdf]
KmdfService = @NAME@, MyDevice_Wdf

[MyDevice_Wdf]
KmdfLibraryVersion = 1.15

[Strings]
SPSVCINST_ASSOCSERVICE = 0x00000002
ManufacturerName       = "@MFG@"
DiskName               = "@NAME@ Installation Disk"
DeviceDesc             = "@DESC@"
ServiceName            = "@NAME@ Service"
)";

	constexpr char Umdf[] = R"(;
; @NAME@.inf - a UMDF 2 (user mode) driver (running from the driver store needs Windows 10 1803: 10.0.17134)
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.dll = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...17134,NTarm64.10.0...17134

; the device's hardware ID (Root\... for a software device)
[Standard.NTamd64.10.0...17134]
%DeviceDesc% = MyDevice_Install, @HWID@

[Standard.NTarm64.10.0...17134]
%DeviceDesc% = MyDevice_Install, @HWID@

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
@NAME@.dll

; the reflector (WUDFRd), which passes the device's requests to the driver in its host process, from Windows' own INF
[MyDevice_Install.NT.HW]
Include = wudfrd.inf
Needs   = WUDFRD.NT.HW

[MyDevice_Install.NT.Services]
Include = wudfrd.inf
Needs   = WUDFRD.NT.Services

[MyDevice_Install.NT.Wdf]
UmdfService      = @NAME@, @NAME@_Install
UmdfServiceOrder = @NAME@

[@NAME@_Install]
UmdfLibraryVersion = 2.15.0
ServiceBinary      = %13%\@NAME@.dll

[Strings]
ManufacturerName  = "@MFG@"
DiskName          = "@NAME@ Installation Disk"
DeviceDesc        = "@DESC@"
)";

	constexpr char Extension[] = R"(;
; @NAME@.inf - an extension INF: settings added to a device that another INF installs
;

[Version]
Signature   = "$Windows NT$"
Class       = Extension
ClassGuid   = {e2f84ce7-8efa-411c-aa69-97454ca4cb57}
ExtensionId = @GUID@        ; the same in every version of this extension
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the hardware ID of the device to extend
[Standard.NTamd64.10.0...16299]
%DeviceDesc% = @NAME@_Install, @HWID@

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = @NAME@_Install, @HWID@

[@NAME@_Install.NT]

[@NAME@_Install.NT.HW]
AddReg = @NAME@_HW_AddReg

[@NAME@_HW_AddReg]
; under the device's hardware key
HKR,, MySetting, 0x00010001, 1

[Strings]
ManufacturerName = "@MFG@"
DeviceDesc       = "@DESC@"
)";

	constexpr char Minifilter[] = R"(;
; @NAME@.inf - a file system minifilter driver
;
; Install with right-click > Install, or: pnputil /add-driver @NAME@.inf /install
; Then: fltmc load @NAME@
;

[Version]
Signature   = "$Windows NT$"
Class       = ActivityMonitor   ; the class of the altitude range (see "File System Filter Driver Classes")
ClassGuid   = {b86dff51-a31e-4bac-b3cf-e8cfe75c9fc2}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
@NAME@_DriverFiles = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.sys = 1

[DefaultInstall.NTamd64]
OptionDesc = %ServiceDescription%
CopyFiles  = @NAME@_DriverFiles

[DefaultInstall.NTamd64.Services]
AddService = %ServiceName%,, @NAME@_Service

[DefaultInstall.NTarm64]
OptionDesc = %ServiceDescription%
CopyFiles  = @NAME@_DriverFiles

[DefaultInstall.NTarm64.Services]
AddService = %ServiceName%,, @NAME@_Service

[@NAME@_DriverFiles]
@NAME@.sys

[@NAME@_Service]
DisplayName    = %ServiceName%
Description    = %ServiceDescription%
ServiceBinary  = %13%\@NAME@.sys
Dependencies   = "FltMgr"
ServiceType    = 2              ; SERVICE_FILE_SYSTEM_DRIVER
StartType      = 3              ; SERVICE_DEMAND_START
ErrorControl   = 1              ; SERVICE_ERROR_NORMAL
LoadOrderGroup = "FSFilter Activity Monitor"
AddReg         = @NAME@_AddReg

[@NAME@_AddReg]
HKR, "Parameters", "SupportedFeatures", 0x00010001, 0x3
HKR, "Parameters\Instances", "DefaultInstance", 0x00000000, %DefaultInstance%
HKR, "Parameters\Instances\"%Instance1.Name%, "Altitude", 0x00000000, %Instance1.Altitude%
HKR, "Parameters\Instances\"%Instance1.Name%, "Flags", 0x00010001, %Instance1.Flags%

[Strings]
ManufacturerName   = "@MFG@"
DiskName           = "@NAME@ Installation Disk"
ServiceName        = "@NAME@"
ServiceDescription = "@DESC@"
DefaultInstance    = "@NAME@ Instance"
Instance1.Name     = "@NAME@ Instance"
Instance1.Altitude = "370030"   ; in the Activity Monitor range: request an altitude of your own from Microsoft
Instance1.Flags    = 0x0        ; attach to new volumes automatically
)";

	constexpr char NdisLwf[] = R"(;
; @NAME@.inf - an NDIS lightweight filter driver
;
; Install with: netcfg -l @NAME@.inf -c s -i MS_@NAME@
;

[Version]
Signature   = "$Windows NT$"
Class       = NetService
ClassGuid   = {4d36e974-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

[Standard.NTamd64.10.0...16299]
%FilterDesc% = @NAME@_Install, MS_@NAME@

[Standard.NTarm64.10.0...16299]
%FilterDesc% = @NAME@_Install, MS_@NAME@

[@NAME@_Install]
AddReg           = @NAME@_Ndi
Characteristics  = 0x40000      ; NCF_LW_FILTER
NetCfgInstanceId = "@GUID@"     ; unique to this filter: the same in every version of it
CopyFiles        = @NAME@_CopyFiles

[@NAME@_CopyFiles]
@NAME@.sys

[@NAME@_Ndi]
HKR, Ndi,            Service,          , "@NAME@"
HKR, Ndi,            CoServices,       0x00010000, "@NAME@"
HKR, Ndi,            HelpText,         , %FilterHelp%
HKR, Ndi,            FilterClass,      , custom      ; where the filter goes in the stack (compression, encryption, scheduler...)
HKR, Ndi,            FilterType,       0x00010001, 2 ; 1 monitoring, 2 modifying
HKR, Ndi,            FilterRunType,    0x00010001, 2 ; 1 mandatory, 2 optional
HKR, Ndi\Interfaces, UpperRange,       , "noupper"
HKR, Ndi\Interfaces, LowerRange,       , "nolower"
HKR, Ndi\Interfaces, FilterMediaTypes, , "ethernet"

[@NAME@_Install.Services]
AddService = @NAME@,, @NAME@_Service_Install

[@NAME@_Service_Install]
DisplayName    = %ServiceName%
Description    = %FilterDesc%
ServiceType    = 1              ; SERVICE_KERNEL_DRIVER
StartType      = 1              ; SERVICE_SYSTEM_START
ErrorControl   = 1              ; SERVICE_ERROR_NORMAL
ServiceBinary  = %13%\@NAME@.sys
LoadOrderGroup = NDIS
AddReg         = @NAME@_Service_AddReg

[@NAME@_Service_AddReg]
HKR, Parameters, NdisImPlatformBindingOptions, 0x00010001, 0   ; default binding with NIC teaming (LBFO)

[@NAME@_Install.Remove.Services]
DelService = @NAME@, 0x200       ; SPSVCINST_STOPSERVICE

[Strings]
ManufacturerName = "@MFG@"
DiskName         = "@NAME@ Installation Disk"
FilterDesc       = "@DESC@"
FilterHelp       = "@DESC@"
ServiceName      = "@NAME@"
)";

	constexpr char KernelService[] = R"(;
; @NAME@.inf - a kernel driver that isn't for a device (a primitive driver: a service only)
;
; Install with right-click > Install, or: pnputil /add-driver @NAME@.inf /install
; Then: sc start @NAME@
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,@VERSION@
CatalogFile = @NAME@.cat
PnpLockdown = 1

[DestinationDirs]
@NAME@_DriverFiles = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
@NAME@.sys = 1

[DefaultInstall.NTamd64]
CopyFiles = @NAME@_DriverFiles

[DefaultInstall.NTamd64.Services]
AddService = @NAME@,, @NAME@_Service

[DefaultInstall.NTarm64]
CopyFiles = @NAME@_DriverFiles

[DefaultInstall.NTarm64.Services]
AddService = @NAME@,, @NAME@_Service

[@NAME@_DriverFiles]
@NAME@.sys

[@NAME@_Service]
DisplayName   = %ServiceName%
Description   = %ServiceDescription%
ServiceBinary = %13%\@NAME@.sys
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL

[Strings]
ManufacturerName   = "@MFG@"
DiskName           = "@NAME@ Installation Disk"
ServiceName        = "@NAME@"
ServiceDescription = "@DESC@"
)";

	constexpr InfTemplate Templates[] = {
		{ L"&PCI Device (KMDF)", 0, PciKmdf, L"MyPciDriver", L"Contoso PCI Device", LR"(PCI\VEN_1234&DEV_5678)" },
		{ L"&USB Device (WinUSB)", 0, UsbWinUsb, L"MyUsbDevice", L"Contoso USB Device", LR"(USB\VID_1234&PID_5678)" },
		{ L"USB Device (&KMDF)", 0, UsbKmdf, L"MyUsbDriver", L"Contoso USB Device", LR"(USB\VID_1234&PID_5678)" },
		{ L"&Software Device (KMDF, Root Enumerated)", 0, RootKmdf, L"MyVirtualDriver", L"Contoso Virtual Device", LR"(Root\MyVirtualDevice)" },
		{ L"User Mode Driver (UM&DF 2)", 0, Umdf, L"MyUmdfDriver", L"Contoso UMDF Device", LR"(Root\MyUmdfDevice)" },
		{ L"&Extension INF", 0, Extension, L"MyExtension", L"Contoso Device Extension", LR"(PCI\VEN_1234&DEV_5678)" },
		{ L"&File System Minifilter", 1, Minifilter, L"MyFilter", L"MyFilter File System Minifilter", nullptr },
		{ L"&NDIS Lightweight Filter", 1, NdisLwf, L"MyLwf", L"Contoso NDIS Lightweight Filter", nullptr },
		{ L"Kernel Service (&Primitive Driver)", 2, KernelService, L"MyDriver", L"Contoso Kernel Driver", nullptr },
	};

	void ReplaceAll(std::string& text, std::string_view what, std::string_view with) {
		for (size_t pos = 0; (pos = text.find(what, pos)) != std::string::npos; pos += with.size())
			text.replace(pos, what.size(), with);
	}
}

std::span<InfTemplate const> InfTemplates::All() {
	return Templates;
}

InfTemplates::Values InfTemplates::NewValues() {
	Values values{};
	::GetLocalTime(&values.Date);
	::CoCreateGuid(&values.Guid);
	return values;
}

InfTemplates::Values InfTemplates::DefaultValues(InfTemplate const& infTemplate) {
	auto values = NewValues();
	values.DriverName = infTemplate.DriverName;
	values.Manufacturer = DefaultManufacturer;
	values.Description = infTemplate.Description;
	values.HardwareId = infTemplate.HardwareId ? infTemplate.HardwareId : L"";
	values.Version = DefaultVersion;
	return values;
}

std::string InfTemplates::GuidText(GUID const& guid) {
	return std::format("{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
		guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

namespace {
	std::string Utf8(std::wstring_view text) {
		if (text.empty())
			return {};
		auto size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
		std::string utf8(size, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), utf8.data(), size, nullptr, nullptr);
		return utf8;
	}

	// the value, or the default if it's empty
	std::wstring_view Or(std::wstring const& value, PCWSTR defaultValue) {
		return value.empty() ? std::wstring_view(defaultValue ? defaultValue : L"") : std::wstring_view(value);
	}

	// for inside a quoted string in [Strings]: "" is a quote (a % is as it is there: SetupAPI doesn't turn %% into one)
	std::string Quoted(std::wstring_view text) {
		auto utf8 = Utf8(text);
		ReplaceAll(utf8, "\"", "\"\"");
		return utf8;
	}
}

std::string InfTemplates::Generate(InfTemplate const& infTemplate, Values const& values) {
	std::string text = infTemplate.Text;
	ReplaceAll(text, "@DATE@", std::format("{:02}/{:02}/{:04}", values.Date.wMonth, values.Date.wDay, values.Date.wYear));
	ReplaceAll(text, "@GUID@", GuidText(values.Guid));
	ReplaceAll(text, "@NAME@", Utf8(Or(values.DriverName, infTemplate.DriverName)));
	ReplaceAll(text, "@MFG@", Quoted(Or(values.Manufacturer, DefaultManufacturer)));
	ReplaceAll(text, "@DESC@", Quoted(Or(values.Description, infTemplate.Description)));
	ReplaceAll(text, "@HWID@", Utf8(Or(values.HardwareId, infTemplate.HardwareId)));
	ReplaceAll(text, "@VERSION@", Utf8(Or(values.Version, DefaultVersion)));
	ReplaceAll(text, "\n", "\r\n");
	return text;
}

std::wstring InfTemplates::FileName(InfTemplate const& infTemplate, Values const& values) {
	return std::wstring(Or(values.DriverName, infTemplate.DriverName)) + L".inf";
}

std::optional<InfTemplates::Problem> InfTemplates::Check(InfTemplate const& infTemplate, Values const& values) {
	auto letter = [](wchar_t ch) { return (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || ch == L'_'; };
	auto digit = [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; };

	// the name of sections, a service, files and the catalog
	auto const& name = values.DriverName;
	if (name.empty())
		return Problem{ Field::DriverName, L"The driver needs a name." };
	if (!letter(name[0]) || !std::all_of(name.begin(), name.end(), [&](wchar_t ch) { return letter(ch) || digit(ch); }))
		return Problem{ Field::DriverName, L"The driver's name is the name of its service and files: letters, digits and _, not starting with a digit." };
	if (name.size() > 64)
		return Problem{ Field::DriverName, L"The driver's name is too long (64 characters at most)." };

	auto text = [](std::wstring const& value) {
		return std::all_of(value.begin(), value.end(), [](wchar_t ch) { return ch >= L' '; });
	};
	if (values.Manufacturer.empty() || !text(values.Manufacturer))
		return Problem{ Field::Manufacturer, L"The manufacturer (the provider too) needs a name, on one line." };
	if (values.Description.empty() || !text(values.Description))
		return Problem{ Field::Description, L"The description can't be empty, and is one line." };

	if (infTemplate.HardwareId) {
		auto const& id = values.HardwareId;
		if (id.empty())
			return Problem{ Field::HardwareId, L"The device's hardware ID is needed (e.g. PCI\\VEN_1234&DEV_5678)." };
		if (!std::all_of(id.begin(), id.end(), [](wchar_t ch) { return ch > L' ' && ch < 0x7f && !wcschr(L",;\"%=[]", ch); }))
			return Problem{ Field::HardwareId, L"A hardware ID has no blanks, and none of , ; \" % = [ ]." };
		if (id.size() > 200)
			return Problem{ Field::HardwareId, L"A hardware ID is 200 characters at most." };
	}

	// 4 numbers, up to 65535 (InfVerif wants all 4)
	auto const& version = values.Version;
	int parts = 0;
	bool good = !version.empty();
	for (size_t pos = 0; good && pos <= version.size(); parts++) {
		auto end = version.find(L'.', pos);
		if (end == std::wstring::npos)
			end = version.size();
		auto part = std::wstring_view(version).substr(pos, end - pos);
		good = !part.empty() && part.size() <= 5 && std::all_of(part.begin(), part.end(), digit) && std::stoul(std::wstring(part)) <= 65535;
		pos = end + 1;
	}
	if (!good || parts != 4)
		return Problem{ Field::Version, L"The version is 4 numbers (0 to 65535) with dots between them, e.g. 1.0.0.0." };
	return {};
}
