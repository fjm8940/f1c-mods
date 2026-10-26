# Red Flag source-port manifest

Port date: 2026-08-17

| Development path | Preserved origin | Purpose |
|---|---|---|
| `src\gr44.c` | `f1c_ida_decompiled\mods\red_flag\gr44.c` | Current editable source; `339BA4F5EDE2FB1D7B9A25ACF14A44845E09FB71A98241A1F4D9A902DCC65B72` |
| `generated\redflag.c` | `f1c_ida_decompiled\mods\red_flag\redflag.c` | Generated release source; `931D2BC5CCFB5F6CD14A75C36E066DD43919E39E239300383F8A903A164ECEF8` |
| `tools\make_release.py` | `f1c_ida_decompiled\mods\red_flag\make_release.py` | Historical generation provenance; `95403BD81BAB297402E0A07D60368F95E80153495E1820D418600812F5018350` |
| `docs\LEGACY_README.md` | `f1c_ida_decompiled\mods\red_flag\README.md` | Implementation/test history; `4493BE2D8118A88E505FD36745B78AD4A936EF6CC8253378FE435480DBFB33F3` |
| `docs\REVIEW_NT2004_COMPARISON.md` | `f1c_ida_decompiled\mods\red_flag\REVIEW_NT2004_COMPARISON.md` | Compatibility research; `8004ED080FE0A77104648E4430A91230B512E6167FBFF3C5D034BA73B08CB6AF` |

The post-copy hashes in this manifest are independently verified against their
origins before this port is accepted. No binaries, injector, legacy revisions,
or runtime scripts are copied by this source-only port.

## Current verified development state

The table above records immutable port provenance, not current content hashes.
Current behavior and lifecycle authority are in `CURRENT_STATUS.md`.

Previous scoped native-icon v25 exact state (preserved):

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `BFE7EFD9199FF42994F6C3D3B44B06DF38FC5525F1B6A17511DD01F500219862` |
| `generated\redflag.c` | `3AAFC690E3E9368243810A21FA824CC9ECFD3FA897852ADDD74E5D2EB1AB7736` |
| `tools\field_reviewer.py` | `4FAE3441E14DEAC92318E3EAAF947DD0347B2E94E1BC37A59C1D428A9ADCE24E` |
| `tools\field_gui.py` | `3DC5BF0FDECE22D5F0419BAD5E35FE285534AA37CACA3410FE1B0063ED227C9B` |
| `trials\2026-08-19\redflag-weather-main-pit-autotrial-v23.dll` | `3D33781BA21662D63FBE4161EB901754D864AAD9133EEEB2E77D6897FE0D0BD3` |
| `trials\2026-08-19\redflag-weather-incident-amplifier-v24.dll` | `EE3EF3695807AB6863E367C96F961C0CB4FEA94B01B6F0889E3B0B7E706E3027` |
| `trials\2026-08-19\redflag-scoped-native-icon-v25.dll` | `0E99002053CF1002D67250D99AC1030B69B8E61396D8B61A5C40F1B66FB290CC` |

Exact build/load and live dry-weather evidence is recorded in
`trials\2026-08-19\SCOPED_NATIVE_ICON_V25_RECORD.md`.

Static-review fixes v26 exact state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `040AEB42E0ED2860901A470F2967BCBB217736EBE58A8AE9E083822249B0EEE3` |
| `generated\redflag.c` | `C7BA14D69FD27494F54C968D8D18D2E38B506CF187EA211470298DC99D1601DF` |
| `tools\make_release.py` | `2BE7983ED6AC07590ACD59FB0F701ED61E6B5908E2FDA9B7B08FFFCC0111596F` |
| `tools\field_reviewer.py` | `7F4CBF7BCB2CB84EB0BAF908551857F84EAEA9138DB309824EC999F8FBE1F696` |
| `tools\field_gui.py` | `F4AF5F4F31CE618D6D7548992C5536BB45DC477DDD2C02EF080A57EF0742CD79` |
| `tests\test_static_contracts.py` | `85D36AD8171CE2FAB1C3CF4178600F55715FD37D51B75A284B5C262A3542A52B` |
| `trials\2026-08-19\redflag-static-review-fixes-v26.dll` | `DE8AA42BE54E415206BD6288BD3FD58B0D8F3B0C463D969A203AC1F45206B678` |

Build and verification evidence is recorded in
`trials\2026-08-19\STATIC_REVIEW_FIXES_V26_RECORD.md`. The v26 DLL has not
been live-loaded or human-validated.

LightLoader-bound v27 RC5 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `8CB44E55CADF8624E2228C438A6227827AFDD741B2077BDEF4A2804F1230CD72` |
| `generated\redflag.c` | `A7825F93DBCABB48328AF02BEF0364C4CFDE629F234D195510CBB0C146058A91` |
| `trials\2026-08-20\redflag-lightloader-bound-v27-rc5.dll` | `BDADA7A89E93B12B72A00079F9CA0C593FCFF78B18951E622900CA1D9FC1F4A6` |
| `development\lightloader\v1.1.0-protected-plugins-rc.1\build\win32-protected-rc4\dinput8.dll` | `0391640D0CFDF7154357FA3F2C27BF8BFE8AAA9AB3773F5BCDBB27568834F321` |

Build and negative-path evidence is recorded in
`trials\2026-08-20\LIGHTLOADER_BOUND_V27_RC5_RECORD.md`. RC1 through RC4 and
their generated before-states are preserved as failed/intermediate evidence;
none is approved for distribution.

Configurable-activation v28 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `A0044FC35E9B7FA25DC680BA7F89430061AC330908058DFD8411A9BEB46D1EE7` |
| `generated\redflag.c` | `72674B6F5CC9AF78FC51004A60BAD453C7F6A7ECAF32394139AD68FA8B566C74` |
| `redflag.ini` | `F7E8E6BBCE64A90601567EFE2B44B350BC35DCD3CC08F39CB66DBAB682B46F95` |
| `docs\CONFIGURATION.md` | `218D42AEAD315C13DBE75645386816D59DACC637CB91C1E9AC6416CE4482BE20` |
| `tools\field_gui.py` | `A425884077A50E704F5C2D51D53B796A5C10D43833A594DAB3A64607F448C0DA` |
| `tests\test_static_contracts.py` | `8279E560E7D42FD62EF1FC014323841DCAAE1FF34876FAEB23571E6F1A783C3E` |
| `f1c_ida_decompiled\tools\red_flag_trial.py` | `C12D4FDB9EF3217B8D0983C4D332B5822FD205D679B7947F06E27BEFE4074052` |
| `trials\2026-08-24\redflag-configurable-v28-rc3-trial.dll` | `DF0ECA0FE94D996334F1EFB8E2081A1DE39691CC03254EA3D503D655A08581A6` |

Build and static verification evidence is recorded in
`trials\2026-08-24\CONFIGURABLE_ACTIVATION_V28_RECORD.md`. v28 remains a
development trial pending exact-hash human validation. The initial v28 and RC2
DLLs remain preserved in the same trial directory as intermediate evidence.
RC2 adds the reviewed post-sequence cooldown re-arm; RC3 fixes the preferred PE
image base so same-name builds are byte-reproducible across output directories.

Active severe-obstruction v39 RC2 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `AB2813FBE8167728E02FF67E2812CA994D4E429EECE9FD5CACFAB66F5D51549D` |
| `generated\redflag.c` | `BB7FE84132A6B4864331911E6A90837B6DBF7C73F7A9AB2410157C24B420EAE7` |
| `tests\test_static_contracts.py` | `AAB6720322A0B80DC6BE344D9F0AD7735B7FAF77716AED516EDCFEF394B9917B` |
| `trials\2026-08-24\v39-rc2-build-a\redflag.dll` | `5BE91B3460CA02A3F068ADEECA3FB1F36DDA574A84AF97CEA7CA7DBB12B531FD` |

The same-basename independent rebuild is byte-identical. Build and validation
evidence is recorded in
`trials\2026-08-24\ACTIVE_SEVERE_OBSTRUCTION_V39_RC2_RECORD.md`. v39 is not
for distribution and has not been staged while the game is running.

Racing-line race-control v40 RC3 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `9915F93520E9D1C0E53E9CA744B5AA40DBFF13F11B21E4CC35BBE022A4C2D112` |
| `generated\redflag.c` | `4C43FFFA23A63058A50392B7B1527F3339C82AE61CE4E7D8F54EF85C3C9BEB7D` |
| `tests\test_static_contracts.py` | `867D5A1D0F31A7020AD8CE2358CEB1ADFAEBD26A1C9A0639B21A6A9EBB8C8E20` |
| `redflag.ini` | `99D73B97602926745A60CF0E611BA5639B14C0B56F500921B53E49CAA218893F` |
| `trials\2026-08-24\v40-racing-line-only\F1C_RedFlag_v40_rc3.dll` | `C2A2542F7042D8E97C13B73BAB3ED84192BB2CD1E7E583EEAC6A48968CD7757F` |

Build and classifier evidence is recorded in
`trials\2026-08-24\RACING_LINE_RACE_CONTROL_V40_RC3_RECORD.md`. v40 is not
for distribution and has not been staged while the game is running.

Passive-placeholder/loader/replay v41 RC2 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `5023C77FDC24736107EC8BC899F69469DCE9FABA594120F6DBE8810DC0629725` |
| `generated\redflag.c` | `D3F02BD23D98745879BF4A7710B427C800AAB2907FA43FD722E01F677F201E9A` |
| `tests\test_static_contracts.py` | `EC10DDCBC4B10F84F029EDB3592DDD3DB416DDC749F0D7E9D9C60CB038F82CB2` |
| `redflag.ini` | `06BC0F8C1086E3E7754DA9735305427A23DE25CDEB68983C770CE651D6A12AC0` |
| `trials\2026-08-24\v41-passive-placeholder-loader-replay\F1C_RedFlag_v41_rc2.dll` | `09E9BEE9B3EC54572BDC5DDBD0F7D6CE21F0F9BABF9EBA4D2AD8405D6D79101B` |

Build, loader and replay evidence is recorded in
`trials\2026-08-24\PASSIVE_PLACEHOLDER_LOADER_REPLAY_V41_RC2_RECORD.md`. v41
It is exact-hash staged only in the closed V10 test
profile.

On-track-pileup/NUMPAD2-DNF v42 RC1 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `7A002C6808955D090BF39F94896406ED23C508A5DB8D6A85984CB6B37E2C96A4` |
| `generated\redflag.c` | `BF4FF0FCDCFA7860F225753AEC4DBA0008243E075A606670CA3DA1BF268079D6` |
| `tests\test_static_contracts.py` | `17B9D8DF6B569132F6B72BF45327650479A27DA85148EC726DB7493C55D24C6D` |
| `redflag.ini` | `7FD84A9EF36277FDB06384FCDCE048573CA0EAA1958874A48AD92434A24BE364` |
| `trials\2026-08-24\v42-ontrack-pileup-numpad2-dnf\F1C_RedFlag_v42_rc1.dll` | `467439C236B7403AB042B9FE9526F15B12334C0E4FCCCC4C6B83C0022CE6BC1B` |

Build, live false-negative, NUMPAD2 revival, loader and replay evidence is
recorded in
`trials\2026-08-24\ONTRACK_PILEUP_NUMPAD2_DNF_V42_RC1_RECORD.md`. v42 is not
for distribution. It is exact-hash staged only in the closed V10 test profile.

Terminal-line-authority v43 RC1 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `7993E291BFB3E188151858D91AC36DD0836143CAC1EF438D77B2E11C990CCAD0` |
| `generated\redflag.c` | `6C6B373D24AC420E97A6CB59B9FB9897D8B95A7E5214E1DB458F1FB8A116D1A5` |
| `tests\test_static_contracts.py` | `D38D20B2C9252C8CBC6725DAD1CD7F6773029F6FFD5884595B001848C3B8D9BA` |
| `redflag.ini` | `EC1E3E0E5BBEC6833E8AC483DAFFEB51829D3BF0F53F6AF246ABDA2B0B8A9604` |
| `trials\2026-08-24\v43-terminal-line-authority\F1C_RedFlag_v43_rc1.dll` | `F9A877EFA379D3F5A5F20D0BA9EC98509AC9AE57EAAB6907CB9E62D7F3D42EAA` |

Build and live Bernoldi evidence is recorded in
`trials\2026-08-24\TERMINAL_LINE_AUTHORITY_V43_RC1_RECORD.md`. v43 is not for
distribution. It is exact-hash staged only in the closed V10 test profile.

Replay-segmentation v44 RC4 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `F8905499C780B5CA11B41150CE7A1DEBB515FEF7D49034B796706FFF8547A0DF` |
| `generated\redflag.c` | `4866F71FD482B38B1A7F304648277F522586C4D8975FC195760A779EB41594FB` |
| `tests\test_static_contracts.py` | `B99BF55F824E7B0752BFC6B3DE43DA6D3CD940A24445952566FEA267B153FFED` |
| `redflag.ini` | `4F67FEB6FA98001285ADF67AF918BD859A7EF66CC383975577C8B5EB8AE137AB` |
| `trials\2026-08-24\v44-replay-segmentation\F1C_RedFlag_v44_rc4.dll` | `1D9A9A1B485D09B33754D9AEAC40685BC5C7B489C8E8ED98FB68A6B6CDBCD388` |

Build, hook-ABI, configuration and staging evidence is recorded in
`trials\2026-08-24\REPLAY_SEGMENTATION_V44_RC4_RECORD.md`. v44 is not for
distribution pending live multi-part creation and playback validation.

Native-replay-finalize v45 RC2 exact development state:

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `14D42BFFD0F145B7FBD7507E78CCBF43394FA538CA66B066E8CE227E93B6DE6F` |
| `generated\redflag.c` | `4B8E762DE4E856B15B5C93C7EB8DEFBE2E226F0659359D51D0D583B182F9B63F` |
| `tests\test_static_contracts.py` | `72F3A3E05956BD35565FAB3A2B836D49330CCB49CFF6B8427211CD523784F9AB` |
| `redflag.ini` | `4F67FEB6FA98001285ADF67AF918BD859A7EF66CC383975577C8B5EB8AE137AB` |
| `trials\2026-08-24\v45-native-replay-finalize\F1C_RedFlag_v45_rc2.dll` | `8C683824AE728AE336C9B7476504DC9F267509B39DB489E2D8414D0908454C82` |

The v44 live failure and v45 native-writer replacement are recorded in
`trials\2026-08-24\NATIVE_REPLAY_FINALIZE_V45_RC2_RECORD.md`. v45 is staged
only for V10 human replay creation and playback validation.

## BR-20260828-A0B0C6 guarded-Y trial — 2026-08-31

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `D5CB6B58A04EC8F088DC82B9776154EDE69497BC05A8C9C5DE8BC90659BDA646` |
| `generated\redflag.c` | `2EA6025415233968BF41E1264D81FE012BEACD8E4143CD07373C09C6E6D9A3BC` |
| `tests\test_static_contracts.py` | `3EB0A46DAB849FF2BA29B0058AF2053651C902BD7CF56274DD879C6A58ACF87D` |
| `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\redflag-y-restart-fix-v2-NOT-FOR-DISTRIBUTION.dll` | `CA19EDAF1DBCA49FB97E8DB72C158FB6744CB467F19FE3635746C3DEAC7F34D8` |
| `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\f1c_game_update_broker-abi13-NOT-FOR-DISTRIBUTION.dll` | `306FDFE8BD73B1D20A72F2C34ED1F8C0E156FB8A215058D570EB96964971A267` |

The corresponding implementation, validation, staging and restoration record
is `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\IMPLEMENTATION_AND_TEST_RECORD.md`.
This state is a development trial and is not approved for distribution.

### Configurable Y / manual-acceptance v4

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `3A09D18459C7CDB5BE935E5ADAE5C8642EEC03C8B907A3FE81B0BAFDFBBF968D` |
| `generated\redflag.c` | `D3E16290B1EA502CE6C15F237B4E810D5639F7592331AC6F22FE6F030A7B42FD` |
| `tests\test_static_contracts.py` | `9B392C7056BDE81C8E964ECA3511ED53E756B1D8E8BC10F3B3DA8C19D3BCDDBC` |
| `redflag.ini` | `2FC3DB3C22B09C108133F99F8D9CCEC05D2276B6B6CE31BA9A1E629FA8A5C344` |
| `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\redflag-configurable-y-manual-t-v4-NOT-FOR-DISTRIBUTION.dll` | `F8A968E3609D43E89480E880D03F6E442BE3579645EA576EE3356A2FD8560C89` |

Evidence: `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\CONFIGURABLE_Y_AND_MANUAL_ACCEPT_RECORD.md`.

### Clean HUD / detector-guard v5

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `B4D7D08C6ED3BBE762F7C953DAC85CEB319E7953CB2541281A18D344652C3181` |
| `generated\redflag.c` | `646D580CC02FB4589AFB27080BF9CBECC1A1C2E199926C5D008637D3D4FDFCBC` |
| `tests\test_static_contracts.py` | `8E6AA5345BD839A5B44CBD8B4C7FB52E07E524093062707C58A67E1725847C66` |
| `redflag.ini` | `2FC3DB3C22B09C108133F99F8D9CCEC05D2276B6B6CE31BA9A1E629FA8A5C344` |
| `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\redflag-clean-hud-detector-guards-v5-NOT-FOR-DISTRIBUTION.dll` | `1FE37957D436926C6C4103B625B4E2B1AA97CD993E85B04DE4357E3642E27A83` |

Evidence: `trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\HUD_PREFIX_AND_DETECTOR_AUDIT_RECORD.md`.

### Coordinated feedback remediation / loose-parts cleanup v9

| Path | SHA-256 |
|---|---|
| `src\gr44.c` | `425ABAF72A244616ED592A2B1186DFC80F941B9952FF5B3ED87ED5C9370BA79E` |
| `generated\redflag.c` | `CA5DC302CF9C8DD2AA7D69400845871D4135D9AE5AAAE650DC5565BCAD1CFB22` |
| `tests\test_static_contracts.py` | `DFEFCA40C0853A1DF87B0F023B3DE4F6D8CF7CCA05D6FEDA646D65B47591DFD5` |
| `redflag.ini` | `2FC3DB3C22B09C108133F99F8D9CCEC05D2276B6B6CE31BA9A1E629FA8A5C344` |
| `trials\2026-08-31\remaining-feedback-remediation\redflag-loose-parts-cleanup-v9-NOT-FOR-DISTRIBUTION.dll` | `3054526E8D0CE862851A125D3ED777584F2A919FC0A08ECFE1C063B50E0CC42F` |

Evidence: `trials\2026-08-31\remaining-feedback-remediation\COORDINATED_FEEDBACK_REMEDIATION_RECORD.md`.

### Instant Replay roster-count diagnostic v10 / matched control v11

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic default off) | `47048BDD95A8405EF8E444456516C7196CBCCCCAB9B2818685354AFB2796E3B0` |
| `generated\redflag.c` (canonical diagnostic default off) | `6DBFE7FD71464273B079EC6A396932C017642229811308A2DE450F0DB40F986B` |
| `tests\test_static_contracts.py` | `98E10D7D6D784ABF98EB7843019C36A64F7A915132571A15892473D5BFE2506C` |
| `trials\2026-08-31\instant-replay-live-crash\gr44.replay-count-stable-diagnostic.c` | `97159F713D0CE46667A433DA344209AA7B435019D38FA60818D718BB4C6A048C` |
| `trials\2026-08-31\instant-replay-live-crash\redflag.replay-count-stable-diagnostic.generated.c` | `457582CAEDE5BA10DF5E834BD438C2B4EABA9B1CA6375468F4E9C6AAE3DC1DA4` |
| `trials\2026-08-31\instant-replay-live-crash\redflag-replay-count-stable-diagnostic-v10-NOT-FOR-DISTRIBUTION.dll` | `48981D6BD077448CB9EB9C983A37073505F623BA357ACB68BA30A38E26D9315E` |
| `trials\2026-08-31\instant-replay-live-crash\redflag-replay-roster-control-v11-NOT-FOR-DISTRIBUTION.dll` | `BE06A64892349BC5D5A4BF67C6F937AF611CDF4A79E5CA0E44CB83E3E76C44EB` |

Evidence: `trials\2026-08-31\instant-replay-live-crash\INCIDENT_RECORD.md`.
The v10 diagnostic intentionally leaves final results incomplete and must not
be distributed. The canonical source and v11 control keep the switch off.

### Player-DQ garage diagnostic v18 / control v19

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical replay diagnostic default off) | `164B8D607725CC432060E66DF5DAED25E37A7D57F4AEB1FEDA6371115715913F` |
| `generated\redflag.c` (canonical replay diagnostic default off) | `72884C34844CABAAEFE30E0AB3668B3DCB951D96EB35CD142046BEF950CD61CD` |
| `tests\test_static_contracts.py` | `41575C171ECEFB494EA312724D2C4C280E86B7C2DC5B986D4F9B16B91E16243F` |
| `trials\2026-08-31\player-dq-garage\redflag-player-dq-garage-proof-v18-NOT-FOR-DISTRIBUTION.dll` | `D033CC1B73BDDDB2B4CFE77C12C7C62D97FD1EAEB5C1AEE4A317C97E4B32EE4C` |
| `trials\2026-08-31\player-dq-garage\redflag-player-dq-garage-control-v19-NOT-FOR-DISTRIBUTION.dll` | `6169CB1A38DC59F0C716107CEB7388F9F085743C04F061AC726450008338E329` |

Evidence and live protocol:
`trials\2026-08-31\player-dq-garage\PLAYER_DQ_GARAGE_DIAGNOSTIC_RECORD.md`.
v18 keeps final results incomplete to preserve the stable survivor-count replay
experiment. The canonical source and v19 control keep that diagnostic switch
off. v18 is exact-hash staged only in the closed V10 clean-test profile;
neither artifact is approved for distribution.

### Native terminal-player placement diagnostic v34 / control v35

The staged v18 live attempt failed closed before arming restart because F1C did
not expose its required `PitState>=3` garage transform. v34 removes that
disproven gate and observes native F1C placement without installing the player
SessionTick hook or writing player state/pose. It retains the survivor-only
roster for replay isolation. It also requires ordered survivor-pointer epoch
continuity before GRID/green lap writes and latches post-constructor failures.
v35 is the matched canonical default-off control. v24, v27 and v30 are
preserved but rejected because they left pointer/failure/concurrency paths
incompletely guarded.

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `F069B53A59F72FC2FD0254369C1C1F5F20498EE9060467A532C67F838ADF191E` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `E17276AA7C2ACDFD69F600DBE51DD1B5AC2222CCE8671A0E78E45A25CDA64C06` |
| `tests\test_static_contracts.py` | `0975F94860A7C2C6C0232F5AB72725A08A5A1014F9BD02C70DD570333F5F02B9` |
| `trials\2026-08-31\player-dq-garage\redflag-player-terminal-native-placement-v34-NOT-FOR-DISTRIBUTION.dll` | `E444FAC38E5D8CD330E0BD03C054041C8649FC019956D919EEF9829039903369` |
| `trials\2026-08-31\player-dq-garage\redflag-player-terminal-native-placement-control-v35-NOT-FOR-DISTRIBUTION.dll` | `045AA90E12885A51F4DED3AE627C570D76112A687865D7F4801F1A7C68AC868C` |

Evidence and the live protocol are in
`trials\2026-08-31\player-dq-garage\PLAYER_DQ_GARAGE_DIAGNOSTIC_RECORD.md`.
Static contracts pass 41/41. v34 is exact-hash staged only in the closed V10
clean-test profile; the replaced v18 DLL is preserved under
`installed-before-v34`. Neither artifact is approved for distribution.

v34 was subsequently live-rejected: the native constructor revived the counted
terminal player into P22 and the retained reduced roster produced `17/17`.
Preserved evidence:
`trials\2026-08-31\player-dq-garage\RedFlag.v34.player-grid-p22-and-17of17.log`,
SHA-256
`8FF54D53792CDEE8C0D71B2C4D54BEABCDF247B3F7FC36002A8F9B8E05FB036A`.
The hashes above remain preservation identifiers only; v34 is rejected and
must not be staged again or distributed.

### Player grid-gate and HUD denominator v36 / control v37

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `6C58C75F08F73473EF60801CCC4EC1C47BA2B466C1BDA7B95317FFA875FFFF79` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `947BF25E59A6BD5F647F8F3AB9D597F37D738CF585DB811D152EC94B734D5C9E` |
| `tests\test_static_contracts.py` | `D12327AAD20BC7D3441B97AAF68BEA858CB8F7B936C958AE951793F2388C29CE` |
| `trials\2026-08-31\player-dq-garage\v36-grid-gate-hud-denominator\redflag-player-grid-gate-hud-denominator-v36-NOT-FOR-DISTRIBUTION.dll` | `0E0168661607AC05DBAF7E327F2D97BDCFB92FDD4D6C2582FB6FD10F914581BF` |
| `trials\2026-08-31\player-dq-garage\v36-grid-gate-hud-denominator\redflag-player-grid-gate-hud-denominator-control-v37-NOT-FOR-DISTRIBUTION.dll` | `3439BD8A7823D0B38BB84BEF76914E1A8DB0338330834D0B6BDE2544CD54007E` |

Evidence and test scope:
`trials\2026-08-31\player-dq-garage\v36-grid-gate-hud-denominator\V36_LIVE_TEST_RECORD.md`.
v36 is exact-hash staged only for a disposable live test and is not approved
for distribution. The replaced v34 DLL is preserved under
`installed-before-v36`.

### Alesi damaged blocked-line v56 / matched control v57

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `7CD740777E7D39E2318F048A82529E3740348CE3940D1A1E283ECFFC98B801A5` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `BD6605153683B3DDB9921B51197B444277771928F613F1E8AE4FABF2F6EE4310` |
| `tests\test_static_contracts.py` | `96200D69AD7FE9C1DE828E0A506D12033701534DA4AFAE7D7B29C3DA97EC2EE5` |
| `trials\2026-09-01\alesi-blocked-line-v56\redflag-alesi-blocked-line-v56-NOT-FOR-DISTRIBUTION.dll` | `E543C6EE811D12E160077CAF8015392E1509BB967B6995F529ABCB8303CABCA7` |
| `trials\2026-09-01\alesi-blocked-line-v56\redflag-alesi-blocked-line-v57-control-NOT-FOR-DISTRIBUTION.dll` | `495DAB4B0C553934DFF3BECAC879CB95756F8E7F06772CDD595978B87D3B38CF` |

Evidence and live protocol:
`trials\2026-09-01\alesi-blocked-line-v56\V56_ALESI_BLOCKED_LINE_RECORD.md`.
v56 retains the denominator/grid/native-garage/garage-lock diagnostics while
v57 is the matched canonical diagnostic-default-off control. Neither artifact
is approved for distribution; v56 was not installed while the game was open.

### Stable debris-pool scan v58 / matched control v59

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `C8A97E4A0CE2AA3AD5839FECCA4BF112BEA6E279C463EC2093F8A6891B2372A2` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `B9744474F2181537F26D31B8E27F07162FF533A4706323283D073AB58161EC81` |
| `tests\test_static_contracts.py` | `16F3C97945AD2A1B50AE61618E0109BEDD4511CDD1789A29345F8A1DD46AE549` |
| `trials\2026-09-01\debris-pool-scan-v58\redflag-debris-pool-scan-v58-NOT-FOR-DISTRIBUTION.dll` | `C5405538383AF10D8405F3D4779EF92AE751650386BE2362290DEB8DADD0A58C` |
| `trials\2026-09-01\debris-pool-scan-v58\redflag-debris-pool-scan-v59-control-NOT-FOR-DISTRIBUTION.dll` | `816BF9EDC6F94B963C0E1B4C1B84769FF21B7D8E8E935815342E7076212E97E5` |

Evidence and live protocol:
`trials\2026-09-01\debris-pool-scan-v58\V58_DEBRIS_POOL_SCAN_RECORD.md`.
v58 retains the v56 diagnostic profile and is not approved for distribution.
It was not installed while the game was running.

### Fail-soft loose-part cleanup v60 / matched control v61

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `CD19766322A6AEE8F771E72E39F739D952C9A6E18FF5D1157AE3C1CCB8552A7D` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `791BF1F440E11A86AFD2EBF5D68B9DED54431019AF9444F8496DE18FCCB9806F` |
| `tests\test_static_contracts.py` | `7DE62ECEDC3E689AE642E42B161433F9C812D11B5CF19CB9AD1635A1960D6FA7` |
| `trials\2026-09-01\debris-failsoft-v60\redflag-debris-failsoft-v60-NOT-FOR-DISTRIBUTION.dll` | `0CBE9665948050F232B4490241E00B660B222509507C12DF7C6C488D17BF7560` |
| `trials\2026-09-01\debris-failsoft-v60\redflag-debris-failsoft-v61-control-NOT-FOR-DISTRIBUTION.dll` | `3EC252D2333D3550A747921D845429A9762E34B9A5F9373D45DCC1FF009C3450` |

Evidence and live protocol:
`trials\2026-09-01\debris-failsoft-v60\V60_DEBRIS_FAILSOFT_RECORD.md`.
v60 retains the v58 diagnostic profile. It
was not installed while the game was running.

### DynMan embedded-object correction v62 / matched control v63

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `B0135B22BD663D3342DF04ECD753B35E67B2F46EF3D761859247555B6935533D` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `D2C8CD29ECFD49B73F194074F00AA37CAB6C6955BD37BD51BC11D92B0B8D91E6` |
| `tests\test_static_contracts.py` | `918905D7EAFFD0824E97224948C576A2BFD44B384BDE1B30EDEFA4639C969A0B` |
| `trials\2026-09-01\dynman-object-v62\redflag-dynman-object-v62-NOT-FOR-DISTRIBUTION.dll` | `2F93508C0BA638E5DD99DF87EA0141345E9DC3DE79D2AB64B96FF6C0BBC3CC32` |
| `trials\2026-09-01\dynman-object-v62\redflag-dynman-object-v63-control-NOT-FOR-DISTRIBUTION.dll` | `ADB9BAC6B5DA7344FC1AD470CEB01C8807A32D288B5705173105337923FB6977` |

Evidence and live protocol:
`trials\2026-09-01\dynman-object-v62\V62_DYNMAN_OBJECT_RECORD.md`.
v62 retains the v60 diagnostic profile. It
was not installed while the game was running.

### One-shot debris cleanup v64

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `181FD35A3F76817D6D9AF86FB232189938A6625FA0DF026B4072B157F5DFB22F` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `0F82718D614A289A1A16DFB3DF4D9AC37CA22F49DD32F79EB613DDE3B09A63AF` |
| `tests\test_static_contracts.py` | `1E7C71A843AE32C330C795FC1E51E13F6BF90408CCD7E993A048C75B495D0883` |
| `trials\2026-09-01\one-shot-debris-v64\redflag.v64.diagnostic.dll` | `E5BD3D5E3045BC78753BD52BA78050E9AAB187633FC3CD33A687210AFEC15B6F` |

Evidence and live protocol:
`trials\2026-09-01\one-shot-debris-v64\V64_ONE_SHOT_DEBRIS_RECORD.md`.
v64 prevents a second broker scan after the successful loose-part expiry clamp.
It is exact-hash installed only in the V10 clean-test profile and remains not
for distribution. The displaced v62 DLL, installed INI and final v62 log are
preserved in the v64 trial directory.

Live v64 is rejected: the debris callback quarantined the shared control broker
client, every green restoration post returned stale-token result 4, and the
temporary 11-survivor roster remained as `11/11`.

### Isolated debris-cleanup broker client v66

| Path | SHA-256 |
|---|---|
| `src\gr44.c` (canonical diagnostic defaults off) | `9CBCE3851C267949480F59055858C62C55789EF8FE8AF6A0367A579A3E51BD1E` |
| `generated\redflag.c` (canonical diagnostic defaults off) | `1DEAF6AD240CB5724FD281D808EB84554DCA3F6925E8BE14A1F7686DE46E083A` |
| `trials\2026-09-01\cleanup-client-isolation-v66\redflag.v66.diagnostic.dll` | `9326D8E09B742F05F4BA4C87A21AC73E7E37A5D11771000F39A34DB7B3EC509E` |

Evidence and live protocol:
`trials\2026-09-01\cleanup-client-isolation-v66\V66_CLEANUP_CLIENT_ISOLATION_RECORD.md`.
v66 gives DynMan cleanup its own quarantinable broker client and keeps the
classification/Y control token independent. It is exact-hash installed only in
the V10 clean-test profile.
