# ezquake-tf release process

Release tags use `vYYYY.MM.DD` without prerelease/test suffixes. Build packages
locally whenever the required toolchain is available. The manual GitHub Actions
workflow is a fallback, not the default release path.

Every Windows and Linux package must include the complete repository `qw`
directory and a SHA-256 sidecar.

## Release metadata

Before every release executable build:

1. Update `EZQUAKE_TF_RELEASE_VERSION` in `src/version.h`.
2. Commit all changes that belong to the release.
3. Run the CMake configure preset again so the build receives the exact date
   and time of the current `HEAD` commit.
4. Build the executable and check that `/version` starts with these three
   lines:

```text
ezquake-tf <release tag>
Last commit: <HEAD commit date and time>
Discord: http://dc.qwtf.net
```

The second line is generated from `git show -s --format=%cI HEAD`; it must not
be replaced by the executable compilation time.

## Windows x64

```powershell
cmake --preset msbuild-x64
cmake --build --preset msbuild-x64-release
$package = "ezquake-tf-windows-x64"
New-Item -ItemType Directory -Path $package | Out-Null
Copy-Item "build-msbuild-x64/Release/ezquake.exe" $package
Copy-Item "qw" $package -Recurse
Compress-Archive -Path "$package/*" -DestinationPath "$package.zip" -CompressionLevel Optimal
$hash = (Get-FileHash "$package.zip" -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $package.zip" | Set-Content "$package.zip.sha256" -NoNewline
```

## Linux x86_64

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./misc/appimage/appimage-manual_creation.sh
package=ezquake-tf-linux-x86_64
mkdir "$package"
cp ezQuake-x86_64.AppImage "$package/"
cp -a qw "$package/"
tar -czf "$package.tar.gz" "$package"
sha256sum "$package.tar.gz" > "$package.tar.gz.sha256"
```

Before publication, verify the tag commit, both downloaded assets, their
sidecars, executable formats, and that each archive contains exactly the local
`qw` file set.
