# Memory Map

## Linker Regions

| Region | Address | Size | Evidence | Notes |
| --- | ---: | ---: | --- | --- |
| Flash/IROM | `0x08000000` | `0x00020000` | `bms24v_platform/MDK-ARM/bms24v_platform/bms24v_platform.sct:5-6` | 128 KiB region |
| RAM/IRAM | `0x20000000` | `0x00024000` | `bms24v_platform/MDK-ARM/bms24v_platform/bms24v_platform.sct:12` | 144 KiB region |
| Stack | N/A | `0x400` | `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:31-34` | Startup assembly stack reserve |
| Heap | N/A | `0x200` | `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:42-46` | Startup assembly heap reserve |

CubeMX metadata also records heap `0x200` and stack `0x400`.

Evidence: `bms24v_platform/bms24v_platform.ioc:194,206`.  
Confidence: High.

## Vector Table And Startup

The vector table starts with initial stack pointer and `Reset_Handler`. `Reset_Handler` imports `SystemInit` and `__main`, so normal C runtime initialization occurs before `main()`.

Evidence: `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:55-60,116-119`.  
Confidence: High.

## Static Data Hotspots

| Data | Owner | Evidence | Notes |
| --- | --- | --- | --- |
| OLED GRAM | `Int_OLED.c` | `Int/Int_OLED.c:4` | Global display buffer `OLED_GRAM[144][8]` |
| BQ transfer buffers | Stack locals in `Int_BQ76952` | `Int/Int_BQ76952.c:144-230,419-482,518-614` | Bounded by `INT_BQ76952_TRANSFER_MAX_LEN` |
| EEPROM verify buffer | Stack local | `Int/Int_EEPROM.c:166-207` | `verify[INT_EEPROM_PAGE_SIZE_BYTES]` |
| OCV lookup table | `Com_BQ76952.c` | `Com/Com_BQ76952.c:10-16` | `static const` table |
| File-scope states | `App_*`/`Int_*` modules | `App/App_SC8815.c:29`; `Int/Int_BQ76952.c:49-50`; `Int/Int_SC8815.c:47` | No heap allocation needed |

## Allocation Rule

No heap allocation pattern was identified in the scanned active modules. Keep embedded code static or stack-bounded unless a new requirement justifies heap use and documents failure behavior.

Evidence: active module scans in `evidence_index.md`; startup heap exists but no allocation evidence in scanned code.  
Confidence: Medium because this is based on scanned paths, not binary symbol analysis.

## Section Placement

No custom section placement beyond the default scatter file regions was identified.

Evidence: `bms24v_platform/MDK-ARM/bms24v_platform/bms24v_platform.sct:5-12`.  
Confidence: Medium. Human confirmation: Review linker map if custom attributes are added later.

## Unknowns

- Stack high-water usage.
- Map file size summary.
- Bootloader/OTA split.
- Flash wear policy.
- EEPROM application schema.
