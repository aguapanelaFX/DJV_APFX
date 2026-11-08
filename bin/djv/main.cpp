// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/App/App.h>

#include <tlRender/UI/Init.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Error.h>
#include <ftk/Core/Path.h>

#include <vector>

#include <iostream>

#if defined(_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#endif // _WINDOWS

#if defined(_WIN32)
namespace
{
    // APFX portable mode.
    //
    // * ffmpeg.exe / ffprobe.exe shipped next to djv.exe (or in an "ffmpeg"
    //   folder beside it) are found first, by putting those folders at the
    //   front of PATH. DJV falls back to the external command on its own for
    //   anything the packaged FFmpeg cannot decode (H.264, HEVC, ProRes...).
    // * When a "portable.txt" file sits next to djv.exe or one folder up,
    //   settings and logs go to a "settings" folder beside it instead of
    //   %APPDATA%, unless -settingsFile / -logFile are given.
    std::filesystem::path exeDir()
    {
        std::wstring buf(32768, L'\0');
        const DWORD n = GetModuleFileNameW(NULL, buf.data(), static_cast<DWORD>(buf.size()));
        buf.resize(n);
        return std::filesystem::path(buf).parent_path();
    }

    void apfxPortable(std::vector<std::string>& args)
    {
        std::error_code ec;
        const std::filesystem::path bin = exeDir();
        if (bin.empty())
            return;

        // Bundled FFmpeg first on PATH.
        std::wstring path;
        if (const wchar_t* env = _wgetenv(L"PATH"))
            path = env;
        path = bin.wstring() + L";" + (bin / L"ffmpeg").wstring() + L";" +
            (bin / L"ffmpeg" / L"bin").wstring() + L";" + path;
        _wputenv_s(L"PATH", path.c_str());

        // Portable settings.
        std::filesystem::path root;
        if (std::filesystem::exists(bin / L"portable.txt", ec))
            root = bin;
        else if (std::filesystem::exists(bin.parent_path() / L"portable.txt", ec))
            root = bin.parent_path();
        if (root.empty())
            return;
        bool hasSettings = false;
        bool hasLog = false;
        for (const auto& a : args)
        {
            if (a == "-settingsFile") hasSettings = true;
            if (a == "-logFile") hasLog = true;
        }
        const std::filesystem::path dir = root / L"settings";
        std::filesystem::create_directories(dir, ec);
        std::vector<std::string> extra;
        if (!hasSettings)
        {
            extra.push_back("-settingsFile");
            extra.push_back(ftk::fromFileSystem(dir / L"djv.json"));
        }
        if (!hasLog)
        {
            extra.push_back("-logFile");
            extra.push_back(ftk::fromFileSystem(dir / L"djv.log"));
        }
        args.insert(args.begin() + (args.empty() ? 0 : 1), extra.begin(), extra.end());
    }
}
#endif // _WIN32

FTK_MAIN()
{
    try
    {
        auto context = ftk::Context::create();
        tl::ui::init(context);
        auto args = ftk::convert(argc, argv);
#if defined(_WIN32)
        apfxPortable(args);
#endif // _WIN32
        auto app = djv::app::App::create(context, args);
        if (app->hasCmdLineHelp())
            return 0;
        app->run();
    }
    catch(const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

#if defined(_WINDOWS)
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    return wmain(__argc, __wargv);
}
#endif // _WINDOWS
