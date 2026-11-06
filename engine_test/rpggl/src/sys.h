// =====================================================================
//  sys.h - threads portateis (Win32 puro no Windows; std:: nos demais)
//
//  Por que embrulhar?  MinGW com "win32 threads" nao tem std::thread;
//  usando CreateThread/CRITICAL_SECTION/CONDITION_VARIABLE direto
//  funciona em MSVC (/MT) e em MinGW sem depender de libwinpthread.
//
//  ThreadPool: duas filas.  ALTA = tarefas curtas do quadro (IA dos NPCs
//  em paralelo); BAIXA = trabalhos de fundo (gerar chunks, gravar saves).
// =====================================================================
#pragma once
#include <atomic>
#include <functional>
#include <deque>
#include <vector>
#include <memory>
#ifdef _WIN32
  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <mutex>
  #include <condition_variable>
  #include <thread>
  #include <chrono>
#endif

#ifdef _WIN32
struct Mutex {
    CRITICAL_SECTION cs;
    Mutex() { InitializeCriticalSection(&cs); }
    ~Mutex() { DeleteCriticalSection(&cs); }
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
    Mutex(const Mutex&) = delete; Mutex& operator=(const Mutex&) = delete;
};
struct CondVar {
    CONDITION_VARIABLE cv;
    CondVar() { InitializeConditionVariable(&cv); }
    void wait(Mutex& m) { SleepConditionVariableCS(&cv, &m.cs, INFINITE); }
    void waitMs(Mutex& m, int ms) { SleepConditionVariableCS(&cv, &m.cs, (DWORD)ms); }
    void notifyOne() { WakeConditionVariable(&cv); }
    void notifyAll() { WakeAllConditionVariable(&cv); }
};
struct Thread {
    HANDLE h; std::function<void()>* fn;
    Thread() : h(0), fn(0) {}
    static DWORD WINAPI Tramp(LPVOID p) { std::function<void()>* f = (std::function<void()>*)p; (*f)(); return 0; }
    void start(const std::function<void()>& f) { fn = new std::function<void()>(f); h = CreateThread(NULL, 0, Tramp, fn, 0, NULL); }
    void join() { if (h) { WaitForSingleObject(h, INFINITE); CloseHandle(h); h = 0; } delete fn; fn = 0; }
};
static inline void SleepMs(int ms) { Sleep((DWORD)ms); }
#else
struct Mutex {
    std::mutex m;
    void lock() { m.lock(); }
    void unlock() { m.unlock(); }
};
struct CondVar {
    std::condition_variable_any cv;
    void wait(Mutex& m) { cv.wait(m); }
    void waitMs(Mutex& m, int ms) { cv.wait_for(m, std::chrono::milliseconds(ms)); }
    void notifyOne() { cv.notify_one(); }
    void notifyAll() { cv.notify_all(); }
};
struct Thread {
    std::thread t;
    void start(const std::function<void()>& f) { t = std::thread(f); }
    void join() { if (t.joinable()) t.join(); }
};
static inline void SleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
#endif

struct Lock {
    Mutex& m; explicit Lock(Mutex& mm) : m(mm) { m.lock(); } ~Lock() { m.unlock(); }
    Lock(const Lock&) = delete; Lock& operator=(const Lock&) = delete;
};

class ThreadPool {
public:
    ThreadPool() : stop_(false), started_(false) {}
    ~ThreadPool() { Stop(); }

    void Start(int n) {
        if (started_) return;
        stop_ = false; started_ = true;
        if (n < 1) n = 1;
        workers_.resize((size_t)n);
        for (int i = 0; i < n; i++) workers_[(size_t)i].start([this]() { Loop(); });
    }
    void Stop() {
        if (!started_) return;
        { Lock l(m_); stop_ = true; cv_.notifyAll(); }
        for (size_t i = 0; i < workers_.size(); i++) workers_[i].join();
        workers_.clear(); started_ = false;
    }
    int Workers() const { return (int)workers_.size(); }

    // fundo (baixa prioridade): geracao de chunks, gravacao de saves
    void Submit(const std::function<void()>& f) { Lock l(m_); low_.push_back(f); cv_.notifyOne(); }
    // curta (alta prioridade)
    void SubmitHigh(const std::function<void()>& f) { Lock l(m_); high_.push_back(f); cv_.notifyOne(); }

    // executa body(inicio, fim) em blocos e ESPERA terminar (o thread chamador tambem trabalha)
    void ParallelFor(int count, int grain, const std::function<void(int, int)>& body) {
        if (count <= 0) return;
        if (grain < 1) grain = 1;
        if (!started_ || workers_.empty() || count <= grain) { body(0, count); return; }
        struct Sync { std::atomic<int> remaining; Mutex m; CondVar cv; Sync() : remaining(0) {} };
        std::shared_ptr<Sync> sy(new Sync());
        int tasks = (count + grain - 1)/grain;
        sy->remaining = tasks - 1;                    // a ultima tarefa roda aqui
        for (int t = 0; t < tasks - 1; t++) {
            int a = t*grain, b = Mini(count, a + grain);
            const std::function<void(int, int)>* bp = &body;
            SubmitHigh([sy, a, b, bp]() {
                (*bp)(a, b);
                if (--sy->remaining == 0) { Lock l(sy->m); sy->cv.notifyAll(); }
            });
        }
        body((tasks - 1)*grain, count);
        Lock l(sy->m);
        while (sy->remaining.load() > 0) sy->cv.waitMs(sy->m, 2);
    }
    int PendingLow() { Lock l(m_); return (int)low_.size(); }

private:
    void Loop() {
        for (;;) {
            std::function<void()> job;
            {
                Lock l(m_);
                while (!stop_ && high_.empty() && low_.empty()) cv_.wait(m_);
                if (stop_ && high_.empty() && low_.empty()) return;
                if (!high_.empty()) { job = high_.front(); high_.pop_front(); }
                else { job = low_.front(); low_.pop_front(); }
            }
            job();
        }
    }
    Mutex m_; CondVar cv_;
    std::deque<std::function<void()> > high_, low_;
    std::vector<Thread> workers_;
    bool stop_, started_;
};
