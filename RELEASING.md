# ezquake-tf release process

Release tags use `vYYYY.MM.DD` without prerelease/test suffixes. Build packages
locally whenever the required toolchain is available. The manual GitHub Actions
workflow is a fallback, not the default release path.

Every Windows and Linux package must include the complete repository `qw`
directory and a SHA-256 sidecar.

## Windows x64

```powershell
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
