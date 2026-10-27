/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ #pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#include <stddef.h>

#if defined(GU_PROFILE_GA107_M6500QC)
/* ASUS Vivobook M6500QC (Ryzen 5000H, Radeon iGPU 1002:1638 drives the panel),
 * RTX 3050 Laptop 4GB (GA107 10de:25a2, subsystem 1043:11d3, rev a1, VBIOS
 * 94.07.5b.00.97) at 01:00.0 behind root port 00:01.1. GPU and display values
 * come from the read-only preflight captures RUN00/RUN01 (2026-10-27). */
#define GU_PROFILE_LABEL "RTX3050M 4GB GA107 / ASUS M6500QC"
#define GU_BOARD_ID 0x25a210deU
#define GU_BOARD_SUBSYSTEM 0x11d31043U
#define GU_AMD_ID 0x16381002U
#define GU_AMD_SUBSYSTEM 0x11d31043U
#define GU_AMD_GOP_BUS 4U
/* Internal panel: PciRoot(0)/Pci(8,1)/Pci(0,0)/AcpiAdr(0x80010400). */
#define GU_AMD_GOP_ADR 0x80010400U
#define GU_BAR0_BASE 0xfb000000ULL
/* 3D controller class (Optimus), MSI-X header at 0xc8. */
#define GU_CLASS_REVISION 0x030200a1U
#define GU_CONFIG_C8 0x00050011U
#define GU_CHIP_ID 0xb77000a1U
#define GU_DEVICE_MEMORY_SIZE 0x1000U
#define GU_PROM_SHA256 "89d4d78bcae54c007d1410bcd0747a5f0c161940b352543fcd931a0dad96a675"
/* No UEFI driver binds the dGPU, so the firmware leaves its command register
   at 0. The app enables memory decode itself and restores it on exit. */
#define GU_TARGET_DECODE_OFF 1
#else
#define GU_PROFILE_LABEL "RTX3090 24GB / ASUS B650M"
#define GU_BOARD_ID 0x220410deU
#define GU_BOARD_SUBSYSTEM 0x145410deU
#define GU_AMD_ID 0x164e1002U
#define GU_AMD_SUBSYSTEM 0x88771043U
#define GU_AMD_GOP_BUS 12U
#define GU_AMD_GOP_ADR 0x80010300U
#define GU_BAR0_BASE 0xf5000000ULL
#define GU_DEVICE_MEMORY_SIZE 0x6000U
#define GU_CHIP_ID 0xb72000a1U
#define GU_CLASS_REVISION 0x030000a1U
#define GU_CONFIG_C8 0x00050011U
#define GU_PROM_SHA256 "8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4"
#define GU_TARGET_DECODE_OFF 0
#endif
#define GU_BAR0_END (GU_BAR0_BASE + 0xffffffULL)
/* Each card family has its own explicit write switch; the shared code keys on
   GU_EXPERIMENTAL_UNLOCK only. */
#if defined(GU_EXPERIMENTAL_3090) && defined(GU_EXPERIMENTAL_3050)
#error Select exactly one experimental card family
#endif
#if defined(GU_EXPERIMENTAL_3090) && defined(GU_PROFILE_GA107_M6500QC)
#error GU_EXPERIMENTAL_3090 cannot target the GA107 profile
#endif
#if defined(GU_EXPERIMENTAL_3050) && !defined(GU_PROFILE_GA107_M6500QC)
#error GU_EXPERIMENTAL_3050 requires GU_PROFILE_GA107_M6500QC
#endif
#if defined(GU_EXPERIMENTAL_3090) || defined(GU_EXPERIMENTAL_3050)
#define GU_EXPERIMENTAL_UNLOCK 1
#endif
/* The default artifact is read-only. Legacy host containment tests are never
   a production enable switch; experimental firmware requires its own define. */
#if defined(GU_HOST_WRITE_TEST) && __STDC_HOSTED__ != 1
#error GU_HOST_WRITE_TEST is forbidden in firmware builds
#endif
#if defined(GU_STOCK_PROFILE_SS0) || defined(GU_STOCK_PROFILE_NAME)
#error Stock profile value and name must be derived from the explicit selector
#endif
#if (defined(GU_STOCK_PROFILE_ORIGINAL_03100062) + defined(GU_STOCK_PROFILE_OBSERVED_03000062) + defined(GU_STOCK_PROFILE_GA107_52600052) + defined(GU_ENTRY_LATCHED_SS1_PRESERVE) + defined(GU_ENTRY_LATCHED_SS1_FULL)) > 1
#error Select exactly one stock profile
#endif
#if (defined(GU_STOCK_PROFILE_ORIGINAL_03100062) || defined(GU_STOCK_PROFILE_OBSERVED_03000062)) && defined(GU_EXPERIMENTAL_3050)
#error GA102 stock selectors cannot be used for GA107
#endif
#if defined(GU_STOCK_PROFILE_GA107_52600052) && defined(GU_EXPERIMENTAL_3090)
#error The GA107 stock selector cannot be used for GA102
#endif
#if defined(GU_EXPERIMENTAL_UNLOCK) && defined(GU_STOCK_PROFILE_ORIGINAL_03100062)
#define GU_STOCK_PROFILE_SS0 0x03100062U
#define GU_STOCK_PROFILE_NAME "original03100062"
#elif defined(GU_EXPERIMENTAL_UNLOCK) && defined(GU_STOCK_PROFILE_OBSERVED_03000062)
#define GU_STOCK_PROFILE_SS0 0x03000062U
#define GU_STOCK_PROFILE_NAME "observed03000062"
#elif defined(GU_EXPERIMENTAL_UNLOCK) && defined(GU_STOCK_PROFILE_GA107_52600052)
/* RTX 3050M UEFI cold state (preflight RUN01): SS0 0x52600052, SS1 0x6. The Linux
   dump showed 0x52600152, read after a driver had touched the card. */
#define GU_STOCK_PROFILE_SS0 0x52600052U
#define GU_STOCK_PROFILE_NAME "ga107Stock52600052"
#elif defined(GU_EXPERIMENTAL_UNLOCK) && defined(GU_ENTRY_LATCHED_SS1_PRESERVE)
#define GU_STOCK_PROFILE_NAME "entryLatchedSs1Preserve"
#elif defined(GU_EXPERIMENTAL_UNLOCK) && defined(GU_ENTRY_LATCHED_SS1_FULL)
#define GU_STOCK_PROFILE_NAME "entryLatchedSs1Full"
#elif defined(GU_EXPERIMENTAL_UNLOCK)
#error Experimental firmware requires exactly one explicit stock profile
#elif defined(GU_STOCK_PROFILE_ORIGINAL_03100062) || defined(GU_STOCK_PROFILE_OBSERVED_03000062) || defined(GU_STOCK_PROFILE_GA107_52600052) || defined(GU_ENTRY_LATCHED_SS1_PRESERVE) || defined(GU_ENTRY_LATCHED_SS1_FULL)
#error Stock profile selection requires experimental firmware
#endif
#if defined(GU_EXPERIMENTAL_UNLOCK) || (defined(GU_HOST_WRITE_TEST) && __STDC_HOSTED__ == 1)
#define GU_PREFLIGHT_ONLY 0
#else
#define GU_PREFLIGHT_ONLY 1
#endif
/* GA107 preflight doubles as the capture run for the probe-derived constants:
   identity mismatches (config, display path, PROM) are logged and the remaining
   read-only stages still run. It cannot arm; port.c blocks every write. */
#if GU_PREFLIGHT_ONLY && defined(GU_PROFILE_GA107_M6500QC)
#define GU_PREFLIGHT_CAPTURE 1
#else
#define GU_PREFLIGHT_CAPTURE 0
#endif
#if defined(GU_ENTRY_LATCHED_SS1_PRESERVE) || defined(GU_ENTRY_LATCHED_SS1_FULL)
#define GU_ENTRY_LATCHED 1
#endif
#define GU_SELECTOR_PHASE_ENTRY 0U
#define GU_SELECTOR_PHASE_SS0_WRITTEN 1U
#define GU_SELECTOR_PHASE_SS1_WRITE 2U
#define GU_SELECTOR_PHASE_TARGET 3U
typedef struct GU_DMA GU_DMA;
struct GU_DMA {
  GU_DMA *next;
  VOID *cpu, *mapping;
  UINT64 dma;
  UINTN bytes, pages;
  BOOLEAN mapped;
};
typedef struct {
  EFI_BOOT_SERVICES *bs;
  EFI_PCI_IO_PROTOCOL *pci;
  EFI_HANDLE image, handle;
  UINT64 bar_base, bar_size, ticks;
  UINT16 pm_cap, pcie_cap, msi_cap, msix_cap;
  UINT32 saved_config[64];
  UINT16 saved_devctl, saved_devctl2;
  UINT8 pcie_version;
  BOOLEAN fault, retained, started, quiesced, armed, owned, display_ok, saved;
  BOOLEAN profile_verified, prom_verified;
  BOOLEAN master_attributes_pending;
  UINT64 master_original_attributes;
  UINT16 master_original_command;
  EFI_PCI_IO_PROTOCOL *master_pci;
  /* Containment may run at TPL_NOTIFY. Capture diagnostics without file IO;
     preserve first errors even if a later containment retry succeeds. */
  EFI_STATUS master_attribute_status, master_hardware_status;
  UINT64 master_last_attributes;
  UINT16 master_last_command;
  UINT32 flrs, mmio_writes, dma_count;
  GU_DMA *allocations;
  BOOLEAN (*display_check)(VOID);
} GU_PLATFORM;
extern GU_PLATFORM Gu;

VOID GuInit(EFI_BOOT_SERVICES *Bs, EFI_PCI_IO_PROTOCOL *Pci);
#ifdef GU_ENTRY_LATCHED
/* Only ColdCheck may latch, after all exact-card/cold/PROM checks. These
   values are private to port.c; the vendor core can only read/compare them. */
BOOLEAN GuLatchEntrySelectors(UINT32 Ss0, UINT32 Ss1);
BOOLEAN GuGetEntrySelectors(UINT32 *Ss0, UINT32 *Ss1);
BOOLEAN GuEntrySelectorsMatch(UINT32 Ss0, UINT32 Ss1);
#endif
#ifdef GU_ENTRY_LATCHED_SS1_FULL
/* The full policy owns the only permitted SS1 write phase. */
BOOLEAN GuSelectorPhase(UINT32 Phase);
#endif
BOOLEAN GuTargetConfig(const UINT32 *Config, UINTN Segment, UINTN Bus, UINTN Device, UINTN Function);
BOOLEAN GuSafeToReturn(VOID);
EFI_STATUS GuDmaAllocate(UINTN Bytes, VOID **Cpu, UINT64 *Dma);
EFI_STATUS GuDmaFree(VOID *Cpu, UINT64 Dma);
UINT32 GuRead32(UINT32 Offset);
VOID GuWrite32(UINT32 Offset, UINT32 Value);
EFI_STATUS GuSavePci(VOID);
EFI_STATUS GuRestorePci(VOID);
EFI_STATUS GuFlr(VOID);
EFI_STATUS GuClearMaster(VOID);
EFI_STATUS GuEnableMaster(VOID);
EFI_STATUS GuConfigRead(EFI_PCI_IO_PROTOCOL_WIDTH Width, UINT32 Offset, VOID *Value);
EFI_STATUS GuConfigWrite(EFI_PCI_IO_PROTOCOL_WIDTH Width, UINT32 Offset, VOID *Value);
VOID GuStarted(VOID);
VOID GuQuiesced(VOID);
VOID GuLog(const char *Format, ...);
VOID GuLogText(const char *Text);
VOID GuCopyFromIo(VOID *Destination, const VOID *Source, size_t Bytes);
VOID *GuAlloc(size_t Bytes);
VOID GuFree(VOID *Memory);
VOID GuStall(UINTN Microseconds);
