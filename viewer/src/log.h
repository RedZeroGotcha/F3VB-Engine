// Log em arquivo de texto com carimbo de hora: f3viewer_log_MM-DD-YY_hh-mm-ss.txt (criado ao abrir o visualizador).
// Portatil (so C++ padrao), gravacao com flush a cada linha: se o programa fechar de repente, o log ate ali esta salvo.
#pragma once
#include <cstdio>
#include <ctime>
#include <string>

namespace f3v {

class Log {
public:
    Log() = default;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log() { close(); }

    // Cria <dir>/f3viewer_log_MM-DD-YY_hh-mm-ss.txt (dir vazio = pasta atual; a pasta ja deve existir).
    // Se o nome ja existir (duas execucoes no mesmo segundo), acrescenta _2, _3...
    bool open(const std::string& dir, const std::string& prefix = "f3viewer_log");   // prefixo: "f3mapviewer_log" no visualizador de mapas
    void close();
    bool is_open() const { return f_ != nullptr; }
    const std::string& path() const { return path_; }

    void line(const std::string& s);              // "[hh:mm:ss.mmm] texto"
    void linef(const char* fmt, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;
    void block(const std::string& text);          // texto cru (tabelas de analise), sem carimbo por linha

    static std::string stamp_name(std::time_t t); // "MM-DD-YY_hh-mm-ss"
    static std::string file_name(std::time_t t, const std::string& prefix = "f3viewer_log");  // "<prefixo>_MM-DD-YY_hh-mm-ss.txt"

private:
    std::FILE* f_ = nullptr;
    std::string path_;
};

}  // namespace f3v
