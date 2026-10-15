# Releasing

Notes for the maintainer. Users and contributors don't need this.

1. Bump the version in `src\version.h`. It goes into both the code and the exe's VERSIONINFO, which iliz_updater compares with the release tag.
2. Commit and push.
3. Run `release.bat`. It builds both exes and writes `dist\iliz_resmon-<version>.zip` (both exes, `iliz_updater.exe`, `iliz_updater.json`, `LICENSE`) and `dist\SHA256SUMS`. The updater exe is taken from `..\iliz_updater\bin`; set `ILIZ_UPDATER_EXE` to use another one.
4. Publish a GitHub release tagged `v<version>` with both files:

   ```
   gh release create v0.18.0 dist\iliz_resmon-0.18.0.zip dist\SHA256SUMS --title v0.18.0 --notes "..."
   ```

5. Check that the updater sees it: `bin\iliz_updater.exe --check --json` should report the new version.

`iliz_resmon.ini` is never packed into the zip and is listed in `preserve` in `iliz_updater.json`, so updates keep user settings. Forks must change `repo` in `iliz_updater.json` to their own repository.