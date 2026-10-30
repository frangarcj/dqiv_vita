// miniunity/coroutine.hpp -- IEnumerator replacement on C++20 coroutines.   OWNER: WP1 (engine core)
//   yield return 0                  -> co_yield 0                    (resume next frame, after all Update())
//   yield return new WaitForSeconds -> co_yield WaitForSeconds{s}    (Time.deltaTime based)
//   yield return null / break       -> co_yield 0 / co_return
#pragma once
#include <coroutine>
#include <utility>

namespace miniunity {

struct WaitForSeconds { float s; };

struct Coroutine {
  struct promise_type {
    float waitLeft = 0;
    Coroutine get_return_object() { return Coroutine{std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    std::suspend_always yield_value(int) { waitLeft = 0; return {}; }
    std::suspend_always yield_value(WaitForSeconds w) { waitLeft = w.s; return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
  std::coroutine_handle<promise_type> h{};
  Coroutine() = default;
  explicit Coroutine(std::coroutine_handle<promise_type> hh) : h(hh) {}
  Coroutine(Coroutine&& o) noexcept : h(std::exchange(o.h, {})) {}
  Coroutine& operator=(Coroutine&& o) noexcept { if (this != &o) { if (h) h.destroy(); h = std::exchange(o.h, {}); } return *this; }
  Coroutine(const Coroutine&) = delete; Coroutine& operator=(const Coroutine&) = delete;
  ~Coroutine() { if (h) h.destroy(); }
  bool Done() const { return !h || h.done(); }
  bool MoveNext() { if (Done()) return false; h.resume(); return !h.done(); }   // C# IEnumerator.MoveNext(): false == finished
  float WaitLeft() const { return h ? h.promise().waitLeft : 0.f; }
  void SetWaitLeft(float f) { if (h) h.promise().waitLeft = f; }
};

}  // namespace miniunity
