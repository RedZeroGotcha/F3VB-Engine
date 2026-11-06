#include "log.h"

#include <chrono>
#include <cstdarg>
#include <vector>

namespace f3v {

namespace {
std::tm local_tm(std::time_t t) {
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}
bool exists(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}
}  // namespace

std::string Log::stamp_name(std::time_t t) {
    const std::tm tm = local_tm(t);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d-%02d-%02d_%02d-%02d-%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year % 100, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::string Log::file_name(std::time_t t, const std::string& prefix) { return prefix + "_" + stamp_name(t) + ".txt"; }

bool Log::open(const std::string& dir, const std::string& prefix) {
    close();
    const std::string base = file_name(std::time(nullptr), prefix);
    const std::string folder = dir.empty() ? std::string() : dir + ((dir.back() == '/' || dir.back() == '\\') ? "" : "/");
    std::string p = folder + base;
    for (int n = 2; exists(p) && n < 1000; ++n) p = folder + base.substr(0, base.size() - 4) + "_" + std::to_string(n) + ".txt";
    f_ = std::fopen(p.c_str(), "wb");
    if (!f_) return false;
    path_ = p;
    return true;
}

void Log::close() {
    if (f_) std::fclose(f_);
    f_ = nullptr;
}

void Log::line(const std::string& s) {
    if (!f_) return;
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    const long ms = static_cast<long>(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
    const std::tm tm = local_tm(t);
    std::fprintf(f_, "[%02d:%02d:%02d.%03ld] %s\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ms, s.c_str());
    std::fflush(f_);
}

void Log::linef(const char* fmt, ...) {
    if (!f_) return;
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    const int n = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    std::vector<char> buf(static_cast<std::size_t>(n > 0 ? n : 0) + 1);
    if (n > 0) std::vsnprintf(buf.data(), buf.size(), fmt, ap2);
    va_end(ap2);
    line(std::string(buf.data()));
}

void Log::block(const std::string& text) {
    if (!f_) return;
    std::string out;
    out.reserve(text.size() + text.size() / 40);
    for (std::size_t i = 0; i < text.size(); ++i) {          // CRLF para abrir bem no Bloco de Notas antigo tambem
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) out += "\r\n";
        else if (text[i] != '\r') out += text[i];
        else out += '\r';
    }
    if (out.size() < 2 || out.compare(out.size() - 2, 2, "\r\n") != 0) out += "\r\n";
    std::fwrite(out.data(), 1, out.size(), f_);
    std::fflush(f_);
}

}  // namespace f3v
