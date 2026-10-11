# Verification

## Automated

`model_tests` checks UTF-8 names and paths, signed multi-monitor positions, transparent/rounded style settings, empty layouts, duplicate shortcuts, bounds, duplicate IDs, every possible truncated input prefix and one-bit corruption at every byte of a representative layout. CI builds with MSVC, runs these tests, checks that the application remains running, and packages an x64 ZIP.

## Manual Windows 11 checklist

- [ ] Drag an `.lnk`, `.url`, executable, folder and document from Explorer; verify the originals are not moved.
- [ ] Open shortcuts by double-click and keyboard; inspect missing-file feedback.
- [ ] Move, resize, collapse, duplicate and delete groups, including a locked group.
- [ ] Verify transparency affects the background, not text/icons; test both 0% and 100%.
- [ ] Test corner radius 0/24/64 and icon size 24/40/72, with/without labels.
- [ ] Test all three materials, tint, a custom PNG/JPEG and removing the background.
- [ ] Link a local folder; create/rename/delete a child and verify refresh without restart.
- [ ] Filter shortcuts; remove a filtered result and check the correct original remains intact.
- [ ] Export/import; corrupt a copy and check rejection; restart and compare all settings.
- [ ] Test startup toggle, single instance, tray recovery after Explorer restart and hotkey conflicts.
- [ ] Test Win+D, full-screen apps, Alt+Tab and virtual desktop switching.
- [ ] Test 100/125/150/200% DPI, negative monitor coordinates, unplug/replug and sleep/resume.
- [ ] Check idle CPU, rapid slider updates and large/slow network folder behavior.

Source code and cross-compilation alone do not establish these Windows shell/UI behaviors. Record runtime evidence here as testing is performed.

## Initial verification evidence

- GCC native model tests: passed, including corruption and truncation checks.
- MinGW-w64 x64 Release build: passed without compiler warnings; imports only Windows system DLLs.
- Microsoft compiler + model tests + process launch + ZIP packaging: passed in GitHub Actions (Windows Server 2022 runner).
- Wine 9 with a 1600×1000 test display: settings and desktop panels render; labels can be hidden; radius 64 and Transparent mode apply live; searching `note` finds only the Notepad entry. Added a font fallback after observing missing Segoe UI in this environment.
- These checks do **not** replace the Windows 11 shell checklist above; native desktop integration, multiple monitors and real wallpaper composition require verification on the target OS.
