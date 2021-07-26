#include <stdio.h>
#include <sstream>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "liburing.h"
#include <iostream>
#include <chrono> 
#include <time.h>
#include <thread>
#include <concepts>
#include <coroutine>
#include "executor.h"

static const uint64_t RingSize = 3000;
static const uint64_t BuffSize = 4096;
static const uint64_t Pages = 1000;
static const uint64_t Files = 400;

struct file_write_info;

struct async_result {

  struct promise_type {

    auto get_return_object() {
      return async_result(std::coroutine_handle<promise_type>::from_promise(*this));
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

  async_result(bool async, file_write_info* file_info) : async_(async), file_info(file_info){}

  async_result(std::coroutine_handle<promise_type> h) : handle_{h} {}

  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<promise_type> h);

  void await_resume() const noexcept {}

  std::coroutine_handle<promise_type> handle_;
  bool async_ = false;
  file_write_info* file_info;
};

struct file_write_info { 
  file_write_info(int fd, int vcore) : size_written(0), fd(fd), vcore(vcore) {
  }

  uint64_t size_written;
  uint64_t size;
  async_result::promise_type* promise;
  int fd;
  int vcore;
};

struct file_write_page {
    int id;
    off_t offset;
    file_write_info* file_info;
    struct iovec *iov;
};





