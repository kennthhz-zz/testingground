#include <concepts>
#include <coroutine>
#include <exception>
#include <iostream>
#include <thread>
#include <chrono>  
#include <typeinfo>

void run(std::coroutine_handle<> h)
{
  std::cout<<std::this_thread::get_id()<<" "<<"in Run\n";
  std::this_thread::sleep_for (std::chrono::seconds(5));
  h.resume();
}

struct return_type {

  struct promise_type {

    promise_type() {
      std::cout<<"promise_type() for promise:"<<(void*)this<<"\n";
    }

    ~promise_type() {
      std::cout<<"~promise_type() for promise:"<<(void*)this<<"\n";
    }

    auto get_return_object() {
      std::cout<<"get_return_object for promise:"<<(void*)this<<"\n";
      return return_type(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    auto initial_suspend() {
      return std::suspend_never{};
    }

    auto final_suspend() noexcept {
      if (prev_ != nullptr) {
        auto hh = std::coroutine_handle<promise_type>::from_promise(*prev_);
        hh.resume();
      }

      return std::suspend_never{};
    }
        
    void unhandled_exception() {
      std::exit(1);
    }
     
    void return_void() {}

    promise_type* prev_ = nullptr;
  };

  return_type(bool async) : async_(async) {}

  return_type(std::coroutine_handle<promise_type> h) : handle_{h} {
  }

  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<promise_type> h) {
    std::cout<<"await_suspend for promise:"<<(void*)(&h.promise())<<"\n";
    if (async_) {
      std::thread t([&promise = h.promise()](){
        std::this_thread::sleep_for (std::chrono::seconds(1));
        std::cout<<std::this_thread::get_id()<<" "<<"async operation completed\n";
        auto h = std::coroutine_handle<promise_type>::from_promise(promise);
        h.resume();
      });
        
      t.detach();
    } else {
      std::cout<<"current promise:"<<(void*)(&handle_.promise())<<" h promise:"<<(void*)&h.promise()<<"\n";
      handle_.promise().prev_ = &h.promise();
    }
  }

  void await_resume() const noexcept { 
  }

  std::coroutine_handle<promise_type> handle_;
  bool async_ = false;
};

return_type Foo2()
{ 
  std::cout<<std::this_thread::get_id()<<" "<<"enter Foo2\n";
  return_type r(true);
  co_await r;
  std::cout<<std::this_thread::get_id()<<" "<<"resume in Foo2\n";
  co_return;
}

return_type Foo1()
{
  std::cout<<std::this_thread::get_id()<<" "<<"enter Foo1\n";
  co_await Foo2();
  std::cout<<std::this_thread::get_id()<<" resume in Foo1\n";
  co_return;
}

return_type Foo0()
{
  std::cout<<std::this_thread::get_id()<<" "<<"enter Foo0\n";
  co_await Foo1();
  std::cout<<std::this_thread::get_id()<<" resume in Foo0\n";
  co_return;
}

int main() {
  auto r = Foo0();
  std::cout<<std::this_thread::get_id()<<" main thread wait for 10 seconds\n";;
  std::this_thread::sleep_for (std::chrono::seconds(10));
}