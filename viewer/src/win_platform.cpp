#include "win_platform.h"

#include <GL/gl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <cstdio>
#include <cstring>

namespace f3v {

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::string exe_dir_utf8() {
    wchar_t b[32768] = {0};
    GetModuleFileNameW(nullptr, b, 32767);
    std::wstring s = b;
    const std::size_t p = s.find_last_of(L"\\/");
    return narrow(p == std::wstring::npos ? std::wstring(L".") : s.substr(0, p));
}

std::vector<std::string> command_line_args() {
    std::vector<std::string> out;
    int n = 0;
    LPWSTR* a = CommandLineToArgvW(GetCommandLineW(), &n);
    if (!a) return out;
    for (int i = 1; i < n; ++i) out.push_back(narrow(a[i]));
    LocalFree(a);
    return out;
}

std::string env_utf8(const char* name) {
    wchar_t b[32768] = {0};
    return GetEnvironmentVariableW(widen(name).c_str(), b, 32767) > 0 ? narrow(b) : std::string();
}

std::string pick_folder(HWND owner, const std::wstring& title, const std::string& start) {
    std::string result;
    const HRESULT hr_init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, reinterpret_cast<void**>(&dlg)))) {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dlg->SetTitle(title.c_str());
        dlg->SetOkButtonLabel(L"Usar esta pasta");
        if (!start.empty()) {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(widen(start).c_str(), nullptr, IID_IShellItem, reinterpret_cast<void**>(&folder)))) { dlg->SetFolder(folder); folder->Release(); }
        }
        if (SUCCEEDED(dlg->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) { result = narrow(path); CoTaskMemFree(path); }
                item->Release();
            }
        }
        dlg->Release();
    } else {
        // Windows muito antigo: seletor classico
        BROWSEINFOW bi;
        std::memset(&bi, 0, sizeof bi);
        bi.hwndOwner = owner;
        bi.lpszTitle = title.c_str();
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            wchar_t b[MAX_PATH] = {0};
            if (SHGetPathFromIDListW(pidl, b)) result = narrow(b);
            CoTaskMemFree(pidl);
        }
    }
    if (SUCCEEDED(hr_init)) CoUninitialize();
    return result;
}

bool KeyValueFile::load(const std::string& path) {
    values.clear();
    FILE* f = _wfopen(widen(path).c_str(), L"rb");
    if (!f) return false;
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) text.erase(0, 3);
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!k.empty() && k.back() == ' ') k.pop_back();
        while (!v.empty() && v.front() == ' ') v.erase(v.begin());
        values[k] = v;
    }
    return true;
}

bool KeyValueFile::save(const std::string& path, const std::string& header) const {
    FILE* f = _wfopen(widen(path).c_str(), L"wb");
    if (!f) return false;
    std::fputs(header.c_str(), f);
    for (const auto& kv : values) std::fprintf(f, "%s=%s\r\n", kv.first.c_str(), kv.second.c_str());
    std::fclose(f);
    return true;
}

float KeyValueFile::getf(const std::string& k, float def) const {
    const auto it = values.find(k);
    if (it == values.end() || it->second.empty()) return def;
    return static_cast<float>(std::atof(it->second.c_str()));
}

// ------------------------------------------------------------------------------------------------ OpenGL "moderno" no contexto 1.x
namespace {
typedef BOOL(WINAPI* PFN_ChoosePF)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef BOOL(WINAPI* PFN_SwapInterval)(int);
LRESULT CALLBACK dummy_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
}  // namespace

int choose_msaa_pixel_format(HINSTANCE inst, HDC target_dc, int want_samples, int& format_out) {
    format_out = 0;
    WNDCLASSA wc;
    std::memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = dummy_proc;
    wc.hInstance = inst;
    wc.lpszClassName = "F3VBDummyGL";
    RegisterClassA(&wc);
    HWND w = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, nullptr, nullptr, inst, nullptr);
    if (!w) return 0;
    HDC dc = GetDC(w);
    PIXELFORMATDESCRIPTOR pfd;
    std::memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
    int got = 0;
    const int pf = ChoosePixelFormat(dc, &pfd);
    if (pf && SetPixelFormat(dc, pf, &pfd)) {
        HGLRC rc = wglCreateContext(dc);
        if (rc && wglMakeCurrent(dc, rc)) {
            PFN_ChoosePF choose = reinterpret_cast<PFN_ChoosePF>(reinterpret_cast<void*>(wglGetProcAddress("wglChoosePixelFormatARB")));
            if (choose) {
                for (int s = want_samples; s >= 2 && !got; s /= 2) {
                    const int attrs[] = {
                        0x2001 /*WGL_DRAW_TO_WINDOW_ARB*/, 1, 0x2010 /*WGL_SUPPORT_OPENGL_ARB*/, 1, 0x2011 /*WGL_DOUBLE_BUFFER_ARB*/, 1,
                        0x2013 /*WGL_PIXEL_TYPE_ARB*/, 0x202B /*WGL_TYPE_RGBA_ARB*/, 0x2014 /*WGL_COLOR_BITS_ARB*/, 32, 0x2022 /*WGL_DEPTH_BITS_ARB*/, 24,
                        0x2003 /*WGL_ACCELERATION_ARB*/, 0x2027 /*WGL_FULL_ACCELERATION_ARB*/,
                        0x2041 /*WGL_SAMPLE_BUFFERS_ARB*/, 1, 0x2042 /*WGL_SAMPLES_ARB*/, s, 0};
                    int fmt = 0;
                    UINT n = 0;
                    if (choose(target_dc, attrs, nullptr, 1, &fmt, &n) && n > 0 && fmt > 0) { format_out = fmt; got = s; }
                }
            }
            wglMakeCurrent(nullptr, nullptr);
        }
        if (rc) wglDeleteContext(rc);
    }
    ReleaseDC(w, dc);
    DestroyWindow(w);
    return got;
}

void set_vsync(bool on) {
    PFN_SwapInterval f = reinterpret_cast<PFN_SwapInterval>(reinterpret_cast<void*>(wglGetProcAddress("wglSwapIntervalEXT")));
    if (f) f(on ? 1 : 0);
}

bool gl_has_extension(const char* name) {
    const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!ext || !name) return false;
    const std::size_t n = std::strlen(name);
    for (const char* p = ext; (p = std::strstr(p, name)) != nullptr; p += n)
        if ((p == ext || p[-1] == ' ') && (p[n] == ' ' || p[n] == 0)) return true;
    return false;
}

float max_anisotropy() {
    if (!gl_has_extension("GL_EXT_texture_filter_anisotropic") && !gl_has_extension("GL_ARB_texture_filter_anisotropic")) return 1.f;
    GLfloat v = 1.f;
    glGetFloatv(0x84FF /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */, &v);
    return v < 1.f ? 1.f : v;
}

}  // namespace f3v
