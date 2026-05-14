# Changelog

All notable firmware changes are documented in this file.

## Versioning Policy
- Version format is `major.minor.patch3` using `x.x.xxx`.
- All version changes require explicit user approval before updating any version number.
- For bug fixes, cleanup, UI tweaks, small behavior changes, and routine maintenance, increment only the last three digits.
- Example: `1.5.002` -> `1.5.003` -> `1.5.004`.
- Increase the middle digit and reset the last three digits to `000` for larger feature releases, grouped functional additions, or meaningful user-facing capability changes.
- Increase the first digit only for rare major platform, architecture, or compatibility shifts that justify a major release boundary.
- When in doubt, use the smallest possible version bump and default to incrementing the last three digits.
- Do not change `sw_version`, changelog version headers, or release labels unless the user has approved that specific version change.

## [1.5.004] - 2026-05-14
- Hardened OTA and manual reboot reconnect logic in web UI to prevent blank page and false failure after update or reboot.
- Fixed DS18B20 sensor regression by making OneWire GPIO41 patch persistent in build process.
- Improved process/tooling: enforced version/changelog sync, explicit approval for version changes, and robust release workflow.
- No functional changes to user-facing features beyond bugfixes and reliability improvements.

## [1.5.003] - 2026-05-14
- Web UI status refresh stabilization:
  - Replaced reload-based status updates with in-place `/status` polling updates.
  - Added no-cache headers for `/` and `/status` responses.
  - Removed conflicting duplicate tab script logic.
- OTA UX and reliability updates:
  - Fixed stale OTA filename/file-input behavior requiring fresh `.bin` selection each run.
  - Added clearer file selection state and disabled upload button until valid selection.
  - Redirect to `/?tab=status` after OTA completion and confirmed `/version` response.
  - Removed legacy standalone OTA page generator in favor of embedded OTA path only.
- Route cleanup:
  - Redirected `/settings` route to embedded settings tab at `/?tab=settings`.
- Tooling:
  - Updated VS Code build tasks to use `./build.sh all` and `./build.sh clean all`.

## [1.5.002] - 2026-05-13
- Fixed display behavior in OFF mode.
- Added non-blocking delay when switching modes.

## [1.5.001] - 2026-05-13
- Added US/EU thermostat region mode.
- Added EU humidity logic and EU relay selection behavior.
- Added display fixes and documentation updates.

## [1.5.000] - 2026-05-05
- Release 1.5.000.

## [1.4.016] - 2026-04-30
- Added SHT45 sensor support.
- Updated display behavior.
- Added backup heat code.

## [1.4.015] - 2026-04-15
- Release 1.4.015.

## [1.4.014] - 2026-03-08
- Display/layout cleanup.
- Hydronic lockout fan stability improvements.

## [1.4.013] - 2026-03-08
- Added DS18B20 sensors to Home Assistant MQTT integration.

## [1.4.012] - 2026-03-08
- Fixed DS18B20 sensors on GPIO41 (ESP32-S3).

## [1.4.009] - 2026-02-22
- Added touch calibration system.
- Enhanced fan control.
- Added dual hydronic sensor support.

## [1.4.003] - 2026-02-22
- Fixed brightness persistence in NVM (follow-up fix).
- Enabled display sleep.

## [1.4.002] - 2026-02-22
- Initial fix attempt for brightness not saving to NVM.

## [1.4.001] - 2026-01-24
- Added bidirectional MQTT schedule synchronization.

## [1.4.0] - 2026-01-04
- Major build system improvements.
- Firmware organization improvements.

## [1.3.9] - 2026-01-01
- Added comprehensive debug logging to web console.
- Added fan cycle debugging.

## [1.3.8] - 2025-12-22
- Release 1.3.8.

## [1.3.7] - 2025-12-19
- Fixed fan relay oscillation bug when manual ON mode conflicted with HVAC fan control.
- Updated schematic and PCB for missing trace.

## [1.3.6] - 2025-12 (documented module update)
- Weather module updates documented in `WEATHER_UPDATES_v1.3.5.md`:
  - Fixed negative high/low weather temperature display logic.
  - Added weather force-update capability.
  - Added weather HTTP timeout handling.
  - Updated weather default update interval.
  - Added Wi-Fi IP logging improvements.

## [1.3.5] - 2025-12-06
- Initial commit.

## Notes
- Historical versions above are derived from version-bump commits in `src/Main-Thermostat.cpp`.
- Some intermediate versions may not be present if they were not tagged by a version-string change commit.
