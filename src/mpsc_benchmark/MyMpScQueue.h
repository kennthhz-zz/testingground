#include <atomic>
#include <iostream>
#include <mutex>
#include <unordered_set>
#include <thread>
#include <chrono>

const int MY_CACHE_LINE_SIZE = 64;
const int ItemListCapacity = 10000000;
std::mutex g_mutex;
using namespace std::chrono_literals;

template <typename T>
class MyMpScQueue final{
 public:
  MyMpScQueue() : req_tail_index_(0), head_index_(0) {
    tailOfQueue_ = new ItemList(0);
    headOfQueue_ = tailOfQueue_;
  }

  void enqueue(const T& data) {
    auto req_tail_index = std::atomic_fetch_add_explicit(&req_tail_index_, 1, std::memory_order_relaxed);
    
    while (true) {
      auto tailOfQueue = tailOfQueue_.load(std::memory_order_acquire);

      if (req_tail_index >= (tailOfQueue->index_ + 1) * ItemListCapacity) {
        // request index is to the right of current (outside of) tail list.
        // the request need to wait the current list filled up first.
        continue;
      }
      else { 
        // proceed to set data
        auto item_to_set = &(tailOfQueue->items_[req_tail_index % ItemListCapacity]);
        item_to_set->data_ = data;
        std::atomic_store_explicit(&(item_to_set->is_set_), 1, std::memory_order_relaxed);
        auto count = std::atomic_fetch_add_explicit(&tailOfQueue->enqueued_count_, 1, std::memory_order_relaxed);

        // Expand
        if (count == ItemListCapacity - 1) {
          auto new_tailOfQueue = new ItemList(tailOfQueue->index_ + 1);
          std::atomic_store_explicit(&tailOfQueue->next_, new_tailOfQueue, std::memory_order_relaxed);
          std::atomic_store_explicit(&tailOfQueue_, new_tailOfQueue, std::memory_order_release);
        }

        return;
      }
    }
  }
  
  void verify() {
    auto ptr = headOfQueue_;
    unsigned int count = 0;
    int index = 0;
    std::unordered_set<T> set;
    while (ptr != nullptr) {

      if (ptr->items_[index % ItemListCapacity].is_set_ == 1) {
        count++;
        set.insert(ptr->items_[index % ItemListCapacity].data_);
      }
        
      index++;
      if (index % ItemListCapacity == 0) {
        ptr = ptr->next_;
      }
    }

    std::cout<<"result:"<< (count == set.size())<<" total:"<<count<<" unique_value_count:"<<set.size()<<"\n";
  }

 private:
  class Item final {
   public:
    T data_;
    std::atomic<int> is_set_;
  };

  class ItemList final {
   public:
    ItemList(uint_fast64_t index) : next_ { nullptr }, index_ { index }, enqueued_count_ {0} {
      memset(items_, 0, ItemListCapacity * sizeof(Item));
    }

    Item items_[ItemListCapacity];
    std::atomic<ItemList*> next_;
    std::atomic<uint_fast64_t> index_;
    std::atomic<int> enqueued_count_;
  };
 
 private:
  std::atomic<ItemList*> tailOfQueue_;
  ItemList* headOfQueue_;
  std::atomic<uint_fast64_t> req_tail_index_;
  std::atomic<uint_fast64_t> head_index_;
};