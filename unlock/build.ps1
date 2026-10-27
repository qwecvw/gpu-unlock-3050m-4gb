# Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md.
param(
 [string]$Clang='clang',
 [string]$LldLink='lld-link',
 [string]$Python='python',
 [ValidateSet('rtx3090-b650m','ga107-m6500qc')][string]$Profile='rtx3090-b650m',
 [switch]$ExperimentalUnlock,
 [ValidateSet('original03100062','observed03000062','ga107Stock52600052','entryLatchedSs1Preserve','entryLatchedSs1Full')][string]$StockProfile
)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
 if(!(Test-Path -LiteralPath $PSScriptRoot)){throw 'Missing output parent'}
 if($ExperimentalUnlock -and !$PSBoundParameters.ContainsKey('StockProfile')){throw '-StockProfile is required with -ExperimentalUnlock'}
 if(!$ExperimentalUnlock -and $PSBoundParameters.ContainsKey('StockProfile')){throw '-StockProfile is only valid with -ExperimentalUnlock'}
 $ga107=$Profile -eq 'ga107-m6500qc'
 if($ga107 -and $StockProfile -in @('original03100062','observed03000062')){throw "$StockProfile is a GA102 selector"}
 if(!$ga107 -and $StockProfile -eq 'ga107Stock52600052'){throw 'ga107Stock52600052 needs -Profile ga107-m6500qc'}
 $card=if($ga107){'RTX3050M'}else{'RTX3090'}
 $out=if($ExperimentalUnlock){"build/$Profile/experimental/$StockProfile"}else{"build/$Profile"}
 if($Profile -eq 'rtx3090-b650m'){$out=if($ExperimentalUnlock){"build/experimental/$StockProfile"}else{'build'}}
 $name=if($ExperimentalUnlock){"$card-EXPERIMENTAL-$StockProfile"}else{"$card-PREFLIGHT"}
 $targetPci=if($ga107){'0x25a210deU'}else{'0x220410deU'}
 $defines=@()
 if($ga107){$defines+='-DGU_PROFILE_GA107_M6500QC'}
 if($ExperimentalUnlock){$defines+=$(if($ga107){'-DGU_EXPERIMENTAL_3050'}else{'-DGU_EXPERIMENTAL_3090'});$defines+=$(switch($StockProfile){'original03100062'{'-DGU_STOCK_PROFILE_ORIGINAL_03100062'} 'observed03000062'{'-DGU_STOCK_PROFILE_OBSERVED_03000062'} 'ga107Stock52600052'{'-DGU_STOCK_PROFILE_GA107_52600052'} 'entryLatchedSs1Preserve'{'-DGU_ENTRY_LATCHED_SS1_PRESERVE'} 'entryLatchedSs1Full'{'-DGU_ENTRY_LATCHED_SS1_FULL'}})}
 New-Item -ItemType Directory -Path $out -Force | Out-Null
 $sources=@('app','app_io','ownership','format','runtime','sha256','checks','port','compat',
  'vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')
 $objects=@()
 foreach($source in $sources) {
  $object="$out/"+($source -replace '/','_')+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -D__KERNEL__ -DGU_UEFI_PORT "-DGU_TARGET_PCI_ID=$targetPci" @defines -I compat -I ../deps/edk2/MdePkg/Include -I ../deps/edk2/MdePkg/Include/X64 -c "$source.c" -o $object
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"};$objects+=$object
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/$name.map" "/out:$out/$name.EFI" @objects
 if($LASTEXITCODE -ne 0){throw 'EFI link failed'}
 & $Python verify_pe.py "$out/$name.EFI" "$out/$name.map" --output "$out/pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'PE verification failed'}
 [ordered]@{BuiltUtc=[DateTime]::UtcNow.ToString('o');Profile=$Profile;
  TargetPci=$(if($ga107){'25a210de'}else{'220410de'});Subsystem=$(if($ga107){'11d31043'}else{'145410de'});
  Mode=$(if($ExperimentalUnlock){'experimental-exact-card-manual-unlock'}elseif($ga107){'diagnostic-preflight-capture'}else{'diagnostic-preflight-only'});
  CanArm=[bool]$ExperimentalUnlock;Defines=$defines;ManualConfirmation='U once; Esc or 30-second timeout cancels';
  StockProfile=$(if($ExperimentalUnlock){$StockProfile}else{$null});ColdChipExpected=$(if($ga107){'0xb77000a1'}else{'0xb72000a1'});ColdSS0Expected=$(if($StockProfile -like 'entryLatchedSs1*'){'first fully validated cold entry; immutable'}elseif($ExperimentalUnlock){switch($StockProfile){'original03100062'{'0x03100062'} 'observed03000062'{'0x03000062'} 'ga107Stock52600052'{'0x52600052'}}}else{'unknown'});ColdSS1Expected=$(if($StockProfile -like 'entryLatchedSs1*'){'first fully validated cold entry <= 0xf; immutable'}else{'0x6'});
  SS0Target='0x88888888';SS1Target=$(if($StockProfile -eq 'entryLatchedSs1Preserve'){'preserve entry; no SS1 writes'}elseif($StockProfile -eq 'entryLatchedSs1Full'){'0x8; one guarded write'}elseif($ExperimentalUnlock){'0x8'}else{'no writes'});
  OriginalBaselineCapture=$(if($ga107){'Preflight RUN01 UEFI cold capture (SS0 0x52600052, SS1 0x6)'}else{'G:/RTX3090-PREFLIGHT/RUN00/REPORT.TXT'});
  StockProfileSource=$(if($StockProfile -like 'entryLatchedSs1*'){'Explicit immutable validated entry latch; no fixed stock selector assumption'}elseif($StockProfile -eq 'observed03000062'){'Explicit observed03000062 experiment selection; original baseline capture does not evidence this SS0'}elseif($ExperimentalUnlock){'OriginalBaselineCapture'}else{'preflight has no selected stock profile'});
  PromSha256=$(if($ga107){'89d4d78bcae54c007d1410bcd0747a5f0c161940b352543fcd931a0dad96a675'}else{'8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4'});
  DeviceMemorySize=$(if($ga107){'0x1000'}else{'0x6000'});HardwareTested=$false;
  EfiSha256=(Get-FileHash -LiteralPath "$out/$name.EFI" -Algorithm SHA256).Hash;
  Headers=@(Get-ChildItem -LiteralPath . -Recurse -File -Filter '*.h' | Where-Object {$_.FullName -notlike '*\build\*'} | Sort-Object FullName | ForEach-Object {[ordered]@{Path=$_.FullName.Substring($PSScriptRoot.Length+1);Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}});
  Sources=@($sources | ForEach-Object {[ordered]@{Path="$_.c";Sha256=(Get-FileHash -LiteralPath "$_.c" -Algorithm SHA256).Hash}})
  } | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath "$out/build-manifest.json"
 "Built $name.EFI; Profile=$Profile; ExperimentalUnlock=$([bool]$ExperimentalUnlock); hardware untested."
} finally {Pop-Location}
