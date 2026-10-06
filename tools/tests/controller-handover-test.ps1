$ErrorActionPreference = 'Stop'
Push-Location (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
try {
    $includes = @(
        'tools/tests/stubs',
        'shadps4-arm64-main/src',
        'shadps4-arm64-main/externals/sdl3/include',
        'shadps4-arm64-main/externals/json/include',
        'build/win-x64/_deps/fmt-src/include',
        'shadps4-arm64-main/externals/dear_imgui',
        'shadps4-arm64-main/externals/vulkan-headers/include',
        'shadps4-arm64-main/externals/toml11/include',
        'shadps4-arm64-main/externals/magic_enum/include'
    ) | ForEach-Object { "/I$_" }
    $libraries = @(
        'build/win-x64/externals/sdl3/SDL3-static.lib',
        'build/win-x64/_deps/fmt-build/fmt.lib',
        'kernel32.lib', 'user32.lib', 'gdi32.lib', 'winmm.lib', 'imm32.lib',
        'oleaut32.lib', 'ole32.lib', 'version.lib', 'advapi32.lib', 'uuid.lib',
        'setupapi.lib', 'shell32.lib', 'hid.lib', 'dinput8.lib'
    )
    & clang-cl /std:c++latest /EHsc /MD /O2 /Gy /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX @includes `
        tools/tests/controller_handover_test.cpp /Fobuild/controller_handover_test.obj `
        /Febuild/controller_handover_test.exe /link /OPT:REF @libraries
    if ($LASTEXITCODE -ne 0) { throw 'Controller handover test compilation failed' }
    foreach ($mode in @('', '--flat', '--script')) {
        & ./build/controller_handover_test.exe $mode
        if ($LASTEXITCODE -ne 0) { throw "Controller handover test failed: $mode" }
    }
} finally {
    Pop-Location
}
