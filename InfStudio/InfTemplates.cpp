#include "pch.h"
#include "InfTemplates.h"

#include <format>

//
// Every template must pass "infverif /w" (Windows Driver requirements) once generated; InfStudio.Tests checks it when the
// WDK is installed. Driver files are copied to the driver store (DIRID 13) and the services run from there.
//
namespace {
	constexpr char PciKmdf[] = R"(;
; MyPciDriver.inf - a KMDF driver for a PCI device
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyPciDriver.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyPciDriver.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the device's hardware ID: its vendor and device IDs (and perhaps SUBSYS_ and REV_)
[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyDevice_Install, PCI\VEN_1234&DEV_5678

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyDevice_Install, PCI\VEN_1234&DEV_5678

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
MyPciDriver.sys

[MyDevice_Install.NT.HW]
AddReg = MyDevice_HW_AddReg

[MyDevice_HW_AddReg]
; message-signaled interrupts (MSI/MSI-X)
HKR, "Interrupt Management",, 0x00000010
HKR, "Interrupt Management\MessageSignaledInterruptProperties",, 0x00000010
HKR, "Interrupt Management\MessageSignaledInterruptProperties", MSISupported, 0x00010001, 1

[MyDevice_Install.NT.Services]
AddService = MyPciDriver, %SPSVCINST_ASSOCSERVICE%, MyDevice_Service_Install

[MyDevice_Service_Install]
DisplayName   = %ServiceName%
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL
ServiceBinary = %13%\MyPciDriver.sys

[MyDevice_Install.NT.Wdf]
KmdfService = MyPciDriver, MyDevice_Wdf

[MyDevice_Wdf]
KmdfLibraryVersion = 1.15

[Strings]
SPSVCINST_ASSOCSERVICE = 0x00000002
ManufacturerName       = "Contoso"
DiskName               = "MyPciDriver Installation Disk"
DeviceDesc             = "Contoso PCI Device"
ServiceName            = "MyPciDriver Service"
)";

	constexpr char UsbWinUsb[] = R"(;
; MyUsbDevice.inf - a USB device that uses WinUSB (no driver of its own; an application talks to it with WinUsb_* functions)
;

[Version]
Signature   = "$Windows NT$"
Class       = USBDevice
ClassGuid   = {88bae032-5a81-49f0-bc3d-a4ff138216d6}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyUsbDevice.cat
PnpLockdown = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the device's hardware ID: its vendor and product IDs (and perhaps &MI_xx for an interface of a composite device)
[Standard.NTamd64.10.0...16299]
%DeviceName% = MyDevice_Install, USB\VID_1234&PID_5678

[Standard.NTarm64.10.0...16299]
%DeviceName% = MyDevice_Install, USB\VID_1234&PID_5678

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
ManufacturerName = "Contoso"
DeviceName       = "Contoso USB Device"
)";

	constexpr char UsbKmdf[] = R"(;
; MyUsbDriver.inf - a KMDF driver for a USB device
;

[Version]
Signature   = "$Windows NT$"
Class       = USBDevice
ClassGuid   = {88bae032-5a81-49f0-bc3d-a4ff138216d6}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyUsbDriver.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyUsbDriver.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the device's hardware ID: its vendor and product IDs (and perhaps &MI_xx for an interface of a composite device)
[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyDevice_Install, USB\VID_1234&PID_5678

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyDevice_Install, USB\VID_1234&PID_5678

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
MyUsbDriver.sys

[MyDevice_Install.NT.Services]
AddService = MyUsbDriver, %SPSVCINST_ASSOCSERVICE%, MyDevice_Service_Install

[MyDevice_Service_Install]
DisplayName   = %ServiceName%
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL
ServiceBinary = %13%\MyUsbDriver.sys

[MyDevice_Install.NT.Wdf]
KmdfService = MyUsbDriver, MyDevice_Wdf

[MyDevice_Wdf]
KmdfLibraryVersion = 1.15

[Strings]
SPSVCINST_ASSOCSERVICE = 0x00000002
ManufacturerName       = "Contoso"
DiskName               = "MyUsbDriver Installation Disk"
DeviceDesc             = "Contoso USB Device"
ServiceName            = "MyUsbDriver Service"
)";

	constexpr char RootKmdf[] = R"(;
; MyVirtualDriver.inf - a KMDF driver for a software (root-enumerated) device, which has no hardware
;
; Create the device and install the driver with:
;   devgen /add /hardwareid "Root\MyVirtualDevice"
;   pnputil /add-driver MyVirtualDriver.inf /install
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyVirtualDriver.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyVirtualDriver.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyDevice_Install, Root\MyVirtualDevice

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyDevice_Install, Root\MyVirtualDevice

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
MyVirtualDriver.sys

[MyDevice_Install.NT.Services]
AddService = MyVirtualDriver, %SPSVCINST_ASSOCSERVICE%, MyDevice_Service_Install

[MyDevice_Service_Install]
DisplayName   = %ServiceName%
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL
ServiceBinary = %13%\MyVirtualDriver.sys

[MyDevice_Install.NT.Wdf]
KmdfService = MyVirtualDriver, MyDevice_Wdf

[MyDevice_Wdf]
KmdfLibraryVersion = 1.15

[Strings]
SPSVCINST_ASSOCSERVICE = 0x00000002
ManufacturerName       = "Contoso"
DiskName               = "MyVirtualDriver Installation Disk"
DeviceDesc             = "Contoso Virtual Device"
ServiceName            = "MyVirtualDriver Service"
)";

	constexpr char Umdf[] = R"(;
; MyUmdfDriver.inf - a UMDF 2 (user mode) driver (running from the driver store needs Windows 10 1803: 10.0.17134)
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyUmdfDriver.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyUmdfDriver.dll = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...17134,NTarm64.10.0...17134

; the device's hardware ID (Root\... for a software device)
[Standard.NTamd64.10.0...17134]
%DeviceDesc% = MyDevice_Install, Root\MyUmdfDevice

[Standard.NTarm64.10.0...17134]
%DeviceDesc% = MyDevice_Install, Root\MyUmdfDevice

[MyDevice_Install.NT]
CopyFiles = MyDevice_CopyFiles

[MyDevice_CopyFiles]
MyUmdfDriver.dll

; the reflector (WUDFRd), which passes the device's requests to the driver in its host process, from Windows' own INF
[MyDevice_Install.NT.HW]
Include = wudfrd.inf
Needs   = WUDFRD.NT.HW

[MyDevice_Install.NT.Services]
Include = wudfrd.inf
Needs   = WUDFRD.NT.Services

[MyDevice_Install.NT.Wdf]
UmdfService      = MyUmdfDriver, MyUmdfDriver_Install
UmdfServiceOrder = MyUmdfDriver

[MyUmdfDriver_Install]
UmdfLibraryVersion = 2.15.0
ServiceBinary      = %13%\MyUmdfDriver.dll

[Strings]
ManufacturerName  = "Contoso"
DiskName          = "MyUmdfDriver Installation Disk"
DeviceDesc        = "Contoso UMDF Device"
)";

	constexpr char Extension[] = R"(;
; MyExtension.inf - an extension INF: settings added to a device that another INF installs
;

[Version]
Signature   = "$Windows NT$"
Class       = Extension
ClassGuid   = {e2f84ce7-8efa-411c-aa69-97454ca4cb57}
ExtensionId = @GUID@        ; the same in every version of this extension
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyExtension.cat
PnpLockdown = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

; the hardware ID of the device to extend
[Standard.NTamd64.10.0...16299]
%DeviceDesc% = MyExtension_Install, PCI\VEN_1234&DEV_5678

[Standard.NTarm64.10.0...16299]
%DeviceDesc% = MyExtension_Install, PCI\VEN_1234&DEV_5678

[MyExtension_Install.NT]

[MyExtension_Install.NT.HW]
AddReg = MyExtension_HW_AddReg

[MyExtension_HW_AddReg]
; under the device's hardware key
HKR,, MySetting, 0x00010001, 1

[Strings]
ManufacturerName = "Contoso"
DeviceDesc       = "Contoso Device Extension"
)";

	constexpr char Minifilter[] = R"(;
; MyFilter.inf - a file system minifilter driver
;
; Install with right-click > Install, or: pnputil /add-driver MyFilter.inf /install
; Then: fltmc load MyFilter
;

[Version]
Signature   = "$Windows NT$"
Class       = ActivityMonitor   ; the class of the altitude range (see "File System Filter Driver Classes")
ClassGuid   = {b86dff51-a31e-4bac-b3cf-e8cfe75c9fc2}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyFilter.cat
PnpLockdown = 1

[DestinationDirs]
MyFilter_DriverFiles = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyFilter.sys = 1

[DefaultInstall.NTamd64]
OptionDesc = %ServiceDescription%
CopyFiles  = MyFilter_DriverFiles

[DefaultInstall.NTamd64.Services]
AddService = %ServiceName%,, MyFilter_Service

[DefaultInstall.NTarm64]
OptionDesc = %ServiceDescription%
CopyFiles  = MyFilter_DriverFiles

[DefaultInstall.NTarm64.Services]
AddService = %ServiceName%,, MyFilter_Service

[MyFilter_DriverFiles]
MyFilter.sys

[MyFilter_Service]
DisplayName    = %ServiceName%
Description    = %ServiceDescription%
ServiceBinary  = %13%\MyFilter.sys
Dependencies   = "FltMgr"
ServiceType    = 2              ; SERVICE_FILE_SYSTEM_DRIVER
StartType      = 3              ; SERVICE_DEMAND_START
ErrorControl   = 1              ; SERVICE_ERROR_NORMAL
LoadOrderGroup = "FSFilter Activity Monitor"
AddReg         = MyFilter_AddReg

[MyFilter_AddReg]
HKR, "Parameters", "SupportedFeatures", 0x00010001, 0x3
HKR, "Parameters\Instances", "DefaultInstance", 0x00000000, %DefaultInstance%
HKR, "Parameters\Instances\"%Instance1.Name%, "Altitude", 0x00000000, %Instance1.Altitude%
HKR, "Parameters\Instances\"%Instance1.Name%, "Flags", 0x00010001, %Instance1.Flags%

[Strings]
ManufacturerName   = "Contoso"
DiskName           = "MyFilter Installation Disk"
ServiceName        = "MyFilter"
ServiceDescription = "MyFilter File System Minifilter"
DefaultInstance    = "MyFilter Instance"
Instance1.Name     = "MyFilter Instance"
Instance1.Altitude = "370030"   ; in the Activity Monitor range: request an altitude of your own from Microsoft
Instance1.Flags    = 0x0        ; attach to new volumes automatically
)";

	constexpr char NdisLwf[] = R"(;
; MyLwf.inf - an NDIS lightweight filter driver
;
; Install with: netcfg -l MyLwf.inf -c s -i MS_MyLwf
;

[Version]
Signature   = "$Windows NT$"
Class       = NetService
ClassGuid   = {4d36e974-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyLwf.cat
PnpLockdown = 1

[DestinationDirs]
DefaultDestDir = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyLwf.sys = 1

[Manufacturer]
%ManufacturerName% = Standard,NTamd64.10.0...16299,NTarm64.10.0...16299

[Standard.NTamd64.10.0...16299]
%FilterDesc% = MyLwf_Install, MS_MyLwf

[Standard.NTarm64.10.0...16299]
%FilterDesc% = MyLwf_Install, MS_MyLwf

[MyLwf_Install]
AddReg           = MyLwf_Ndi
Characteristics  = 0x40000      ; NCF_LW_FILTER
NetCfgInstanceId = "@GUID@"     ; unique to this filter: the same in every version of it
CopyFiles        = MyLwf_CopyFiles

[MyLwf_CopyFiles]
MyLwf.sys

[MyLwf_Ndi]
HKR, Ndi,            Service,          , "MyLwf"
HKR, Ndi,            CoServices,       0x00010000, "MyLwf"
HKR, Ndi,            HelpText,         , %FilterHelp%
HKR, Ndi,            FilterClass,      , custom      ; where the filter goes in the stack (compression, encryption, scheduler...)
HKR, Ndi,            FilterType,       0x00010001, 2 ; 1 monitoring, 2 modifying
HKR, Ndi,            FilterRunType,    0x00010001, 2 ; 1 mandatory, 2 optional
HKR, Ndi\Interfaces, UpperRange,       , "noupper"
HKR, Ndi\Interfaces, LowerRange,       , "nolower"
HKR, Ndi\Interfaces, FilterMediaTypes, , "ethernet"

[MyLwf_Install.Services]
AddService = MyLwf,, MyLwf_Service_Install

[MyLwf_Service_Install]
DisplayName    = %ServiceName%
Description    = %FilterDesc%
ServiceType    = 1              ; SERVICE_KERNEL_DRIVER
StartType      = 1              ; SERVICE_SYSTEM_START
ErrorControl   = 1              ; SERVICE_ERROR_NORMAL
ServiceBinary  = %13%\MyLwf.sys
LoadOrderGroup = NDIS
AddReg         = MyLwf_Service_AddReg

[MyLwf_Service_AddReg]
HKR, Parameters, NdisImPlatformBindingOptions, 0x00010001, 0   ; default binding with NIC teaming (LBFO)

[MyLwf_Install.Remove.Services]
DelService = MyLwf, 0x200       ; SPSVCINST_STOPSERVICE

[Strings]
ManufacturerName = "Contoso"
DiskName         = "MyLwf Installation Disk"
FilterDesc       = "Contoso NDIS Lightweight Filter"
FilterHelp       = "Contoso NDIS Lightweight Filter"
ServiceName      = "MyLwf"
)";

	constexpr char KernelService[] = R"(;
; MyDriver.inf - a kernel driver that isn't for a device (a primitive driver: a service only)
;
; Install with right-click > Install, or: pnputil /add-driver MyDriver.inf /install
; Then: sc start MyDriver
;

[Version]
Signature   = "$Windows NT$"
Class       = System
ClassGuid   = {4d36e97d-e325-11ce-bfc1-08002be10318}
Provider    = %ManufacturerName%
DriverVer   = @DATE@,1.0.0.0
CatalogFile = MyDriver.cat
PnpLockdown = 1

[DestinationDirs]
MyDriver_DriverFiles = 13

[SourceDisksNames]
1 = %DiskName%

[SourceDisksFiles]
MyDriver.sys = 1

[DefaultInstall.NTamd64]
CopyFiles = MyDriver_DriverFiles

[DefaultInstall.NTamd64.Services]
AddService = MyDriver,, MyDriver_Service

[DefaultInstall.NTarm64]
CopyFiles = MyDriver_DriverFiles

[DefaultInstall.NTarm64.Services]
AddService = MyDriver,, MyDriver_Service

[MyDriver_DriverFiles]
MyDriver.sys

[MyDriver_Service]
DisplayName   = %ServiceName%
Description   = %ServiceDescription%
ServiceBinary = %13%\MyDriver.sys
ServiceType   = 1               ; SERVICE_KERNEL_DRIVER
StartType     = 3               ; SERVICE_DEMAND_START
ErrorControl  = 1               ; SERVICE_ERROR_NORMAL

[Strings]
ManufacturerName   = "Contoso"
DiskName           = "MyDriver Installation Disk"
ServiceName        = "MyDriver"
ServiceDescription = "Contoso Kernel Driver"
)";

	constexpr InfTemplate Templates[] = {
		{ L"&PCI Device (KMDF)", L"MyPciDriver.inf", 0, PciKmdf },
		{ L"&USB Device (WinUSB)", L"MyUsbDevice.inf", 0, UsbWinUsb },
		{ L"USB Device (&KMDF)", L"MyUsbDriver.inf", 0, UsbKmdf },
		{ L"&Software Device (KMDF, Root Enumerated)", L"MyVirtualDriver.inf", 0, RootKmdf },
		{ L"User Mode Driver (UM&DF 2)", L"MyUmdfDriver.inf", 0, Umdf },
		{ L"&Extension INF", L"MyExtension.inf", 0, Extension },
		{ L"&File System Minifilter", L"MyFilter.inf", 1, Minifilter },
		{ L"&NDIS Lightweight Filter", L"MyLwf.inf", 1, NdisLwf },
		{ L"Kernel Service (&Primitive Driver)", L"MyDriver.inf", 2, KernelService },
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

std::string InfTemplates::GuidText(GUID const& guid) {
	return std::format("{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
		guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

std::string InfTemplates::Generate(InfTemplate const& infTemplate, Values const& values) {
	std::string text = infTemplate.Text;
	ReplaceAll(text, "@DATE@", std::format("{:02}/{:02}/{:04}", values.Date.wMonth, values.Date.wDay, values.Date.wYear));
	ReplaceAll(text, "@GUID@", GuidText(values.Guid));
	ReplaceAll(text, "\n", "\r\n");
	return text;
}
