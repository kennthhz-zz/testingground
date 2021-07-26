#include <iostream>
#include <memory>
#include <vector>
#include <set>

constexpr int size = 10;
class MyObject;

template <typename T>
class ObjectPool;

template <typename T>
struct Deleter {
  Deleter(ObjectPool<T>* pool) {
    pool_ = pool;
  }

  void operator()(T* r) {
    pool_->Return(r);
  }

  ObjectPool<T>* pool_;
};

template <typename T>
class ObjectPool {
 public:
    ObjectPool() : deleter_(this) {
    for (int i = 0; i < size-1; i++) {
      objects_[i].set_next(&objects_[i+1]);
    }

    freelist_ = &objects_[0];
    freelist_size_ = size;
  }

  std::unique_ptr<T, Deleter<T>> Get() {
    auto current = freelist_;
    if (current == nullptr) {
      throw "out of memory";
    }

    freelist_ = dynamic_cast<T*>(freelist_->next());
    current->set_next(nullptr);
    std::unique_ptr<T, Deleter<T>> pointer(current, deleter_);
    freelist_size_--;
    return pointer;
  }

  void Return(T* obj) {
    obj->set_next(freelist_);
    freelist_ = obj;
    freelist_size_++;
  }

  void PrintFreelist() {
    if (freelist_ == nullptr) {
      std::cout<<"Free list is empty\n";
      return;
    }

    auto current = freelist_;
    while (true) {
      if (current == nullptr) {
        break;
      }

      std::cout<<current - &objects_[0]<<" ";
      current = dynamic_cast<T*>(current->next());
    }

    std::cout<<"\n";
  }

 private:
  T objects_[size];
  Deleter<T> deleter_;
  T* freelist_;
  int freelist_size_;
};

class PooledObject {
 public:
  explicit PooledObject() : next_(nullptr) {}

  virtual ~PooledObject() = default;

  void set_next(PooledObject* next) {
    next_ = next;
  }

  PooledObject* next() {
    return next_;
  }
  
 private:
  PooledObject* next_;
};

class MyObject final : public PooledObject {
 public:
  MyObject(){}
  
 private:
  std::string data_;
};

void test(ObjectPool<MyObject>& pool) {
  pool.PrintFreelist();
  auto p = pool.Get();
  pool.PrintFreelist();
  p = pool.Get();
  pool.PrintFreelist();
}

int main() {
  ObjectPool<MyObject> pool;
  test(pool);
  pool.PrintFreelist();
}