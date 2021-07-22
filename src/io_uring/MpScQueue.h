/*
MIT License

Copyright (c) 2020 Dolev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <iostream>
#include <thread>

using std::cout;
using std::endl;

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define NODE_SIZE 1620

static inline void * my_align_malloc(size_t align, size_t size) {
  void * ptr;
  int ret = posix_memalign(&ptr, align, size);
  if (ret != 0)
    abort();

  return ptr;
}

static inline void  my_align_free(void * ptr) {
  free(ptr);
}

// enum  is int we want something smaller as char
enum State { empty, set, handled};
template <class T>
class MpScQueue final {
 private:
  class Node {
   public:
    T data;

    // if the node and its data are valid.  0- empty  , 1- set , 2 - handled
    std::atomic<char> is_set;
    Node() : data(), is_set(0) {
    }

    explicit Node(T d) : data(d), is_set(0) {
    }

    Node(const Node &n) {
      data = n.data;
      is_set = n.is_set;
    }
  };

  class bufferList final {
   public:
    Node currbuffer[NODE_SIZE] alignas(CACHE_LINE_SIZE);
    std::atomic<bufferList*> next alignas(CACHE_LINE_SIZE);
    bufferList* prev  alignas(CACHE_LINE_SIZE);

    // we have one thread that takes out elelemts and it is the only one that moves the head
    unsigned int head;

    // start with 1
    unsigned int positionInQueue;

    bufferList() : next(NULL), prev(NULL), head(0), positionInQueue(0) {
    }

    explicit bufferList(unsigned int bufferSizez) : next(NULL), prev(NULL), head(0), positionInQueue(1) {
      //memset(currbuffer, 0, NODE_SIZE *sizeof(Node));
    }

    explicit bufferList(unsigned int bufferSizez , unsigned int positionInQueue , bufferList* prev) :
      next(NULL), prev(prev), head(0), positionInQueue(positionInQueue) {
      //memset(currbuffer, 0, NODE_SIZE * sizeof(Node));
    }
  };

  unsigned int bufferSize;

  // the beginning of the last array which we insert elements into (for the producers)
  std::atomic<bufferList*> tailOfQueue alignas(CACHE_LINE_SIZE);

  // we need a global tail so the queue will be wait free - if not the queue is lock free
  std::atomic<uint_fast64_t> gTail alignas(CACHE_LINE_SIZE);

  // the first array that contains data for the thread that takes elements from the queue (for the single consumer)
  bufferList* headOfQueue;

 public:
  MpScQueue() : bufferSize(NODE_SIZE), tailOfQueue(NULL), gTail(0) {
    void* buffer = my_align_malloc(PAGE_SIZE, sizeof(bufferList));
    headOfQueue = new(buffer)bufferList(bufferSize);
    tailOfQueue = headOfQueue;
  }

  ~MpScQueue() {
    while (headOfQueue->next.load(std::memory_order_acquire) != NULL) {
      bufferList* next = headOfQueue->next.load(std::memory_order_acquire);
      my_align_free(headOfQueue);
      headOfQueue = next;
    }

    my_align_free(headOfQueue);
  }

  // return false if the queue is empty
  // to take out an element - just one thread call this func
  bool dequeue(T& data) {
    while (true) {
      bufferList* tempTail = tailOfQueue.load(std::memory_order_seq_cst);
      unsigned int prevSize = bufferSize*(tempTail->positionInQueue - 1);

      if ((headOfQueue == tailOfQueue.load(std::memory_order_acquire)) &&
        (headOfQueue->head == (gTail.load(std::memory_order_acquire) - prevSize))) {
        // the queue is empty
        return false;
      } else if (headOfQueue->head < bufferSize) {
        // there is elements in the current array
        Node* n = &(headOfQueue->currbuffer[headOfQueue->head]);
        if (n->is_set.load(std::memory_order_acquire) == 2) {
          headOfQueue->head++;
          continue;
        }

        bufferList* tempHeadOfQueue = headOfQueue;
        unsigned int tempHead = tempHeadOfQueue->head;
        bool flag_moveToNewBuffer = false, flag_buffer_all_handeld = true;

        // is not set yet - try to take out set elements that are next in line
        while (n->is_set.load(std::memory_order_acquire) == 0) {
          if (tempHead  < bufferSize) {
            // there is elements in the current array.
            Node* tn = &(tempHeadOfQueue->currbuffer[tempHead++]);

            // the data is valid and the element in the head is still in insert process
            if (tn->is_set.load(std::memory_order_acquire) == 1 && n->is_set.load(std::memory_order_acquire) == 0) {
              // Scan start
              bufferList* scanHeadOfQueue = headOfQueue;
              for (unsigned int scanHead = scanHeadOfQueue->head;
                scanHeadOfQueue != tempHeadOfQueue ||
                  (scanHead < tempHead - 1 && n->is_set.load(std::memory_order_acquire) == 0); scanHead++) {
                // we reach the end of the buffer -move to the next
                if (scanHead >= bufferSize) {
                  scanHeadOfQueue = scanHeadOfQueue->next.load(std::memory_order_acquire);
                  scanHead = scanHeadOfQueue->head;
                  continue;
                }

                Node* scanN = &(scanHeadOfQueue->currbuffer[scanHead]);

                // there is another element that is set - the scan start again until him
                if (scanN->is_set.load(std::memory_order_acquire) == 1) {
                  // this is the new item to be evicted
                  tempHead = scanHead;
                  tempHeadOfQueue = scanHeadOfQueue;
                  tn = scanN;
                  scanHeadOfQueue = headOfQueue;
                  scanHead = scanHeadOfQueue->head;
                }
              }

              if (n->is_set.load(std::memory_order_acquire) == 1) {
                break;
              }  // Scan end

              data = std::move(tn->data);
              tn->is_set.store(2, std::memory_order_release);  // set taken

              // if we moved to a new buffer ,we need to move forward the head so in the end we can delete the buffer
              if (flag_moveToNewBuffer && (tempHead-1) == tempHeadOfQueue->head) {
                tempHeadOfQueue->head++;
              }

              return true;
            }  // the data is valid and the element in the head is still in insert process

            if (tn->is_set.load(std::memory_order_acquire) == 0) {
              flag_buffer_all_handeld = false;
            }
          }

          // we reach the end of the buffer -move to the next
          if (tempHead >= bufferSize) {
            // we want to "fold" the queue: if there is a thread that got stuck,
            // we want to keep only that buffer and delete the rest( upfront from it)
            if (flag_buffer_all_handeld && flag_moveToNewBuffer) {
               // there is no place to move
              if (tempHeadOfQueue == tailOfQueue.load(std::memory_order_acquire))
                return false;

              bufferList* next = tempHeadOfQueue->next.load(std::memory_order_acquire);
              bufferList* prev = tempHeadOfQueue->prev;

              // if we do not have where to move
              if (next == NULL)
                return false;

              next->prev = prev;
              prev->next.store(next, std::memory_order_release);
              my_align_free(tempHeadOfQueue);

              tempHeadOfQueue = next;
              tempHead = tempHeadOfQueue->head;
              flag_buffer_all_handeld = true;
              flag_moveToNewBuffer = true;
            } else {
              bufferList* next = tempHeadOfQueue->next.load(std::memory_order_acquire);

              // if we do not have where to move
              if (next == NULL)
                return false;

              tempHeadOfQueue = next;
              tempHead = tempHeadOfQueue->head;
              flag_moveToNewBuffer = true;
              flag_buffer_all_handeld = true;
            }
          }
        }  //try to take out set elements that are next in line

        // n->is_set.load() State::empty
        if (n->is_set.load(std::memory_order_acquire) == 1) {
          headOfQueue->head++;
          data = std::move(n->data);
          return true;
        }
      }

      // move to the next array and delete the prev array
      if (headOfQueue->head >= bufferSize) {
        if (headOfQueue == tailOfQueue.load(std::memory_order_acquire))
          return false;  // there is no place to move

        bufferList* next = headOfQueue->next.load(std::memory_order_acquire);
        if (next == NULL)
          return false;  // if we do not have where to move

        my_align_free(headOfQueue);
        headOfQueue = next;
      }
    }
  }

  void enqueue(T&& data) {
    bufferList* tempTail;
    unsigned int location = gTail.fetch_add(1, std::memory_order_seq_cst);
    bool go_back = false;
    while (true) {
      // points to the last buffer in queue
      tempTail = tailOfQueue.load(std::memory_order_acquire);

      // the amount of items in the queue without the current buffer
      unsigned int prevSize = bufferSize*(tempTail->positionInQueue-1);

      // the location is back in the queue - we need to go back in the queue to the right buffer
      while (location <prevSize) {
        go_back = true;
        tempTail = tempTail->prev;
        prevSize -= bufferSize;
      }

      // the amount of items in the queue
      unsigned int globalSize = bufferSize + prevSize;

      // we are in the right buffer
      if (prevSize <= location && location < globalSize) {
        int index = location - prevSize;
        Node* n = &(tempTail->currbuffer[index]);
        n->data = std::forward<T>(data);
        n->is_set.store(1, std::memory_order_relaxed);  // need this to signal the thread that the data is ready

        // allocating a new buffer and adding it to the queue
        if (index == 1 && !go_back) {
          void* buffer = my_align_malloc(PAGE_SIZE, sizeof(bufferList));
          bufferList* newArr = new(buffer)bufferList(bufferSize, tempTail->positionInQueue + 1, tempTail);
          bufferList* Nullptr = NULL;

          if (!(tempTail->next).compare_exchange_strong(Nullptr, newArr))
            my_align_free(newArr);
        }

        return;
      }

      // the location we got is in the next buffer
      if (location >= globalSize) {
        bufferList* next = (tempTail->next).load(std::memory_order_acquire);

        // we do not have a next buffer - so we can try to allocate a new one
        if (next == NULL) {
          void* buffer = my_align_malloc(PAGE_SIZE, sizeof(bufferList));
          bufferList* newArr = new(buffer)bufferList(bufferSize, tempTail->positionInQueue + 1, tempTail);
          bufferList* Nullptr = NULL;

          // if (CAS(tailOfQueue->next, NULL, newArr)) 1 next-> new array
          if ((tempTail->next).compare_exchange_strong(Nullptr, newArr))
            tailOfQueue.store(newArr, std::memory_order_release);
          else
            my_align_free(newArr);
        } else {
          // if it is not null move to the next buffer
          tailOfQueue.compare_exchange_strong(tempTail, next);
        }
      }
    }
  }

	void enqueue(T const& data) {
    bufferList* tempTail;
    unsigned int location = gTail.fetch_add(1, std::memory_order_seq_cst);
    bool go_back = false;
    while (true) {
      // points to the last buffer in queue
      tempTail = tailOfQueue.load(std::memory_order_acquire);

      // the amount of items in the queue without the current buffer
      unsigned int prevSize = bufferSize*(tempTail->positionInQueue-1);

      // the location is back in the queue - we need to go back in the queue to the right buffer
      while (location <prevSize) {
        go_back = true;
        tempTail = tempTail->prev;
        prevSize -= bufferSize;
      }

      // the amount of items in the queue
      unsigned int globalSize = bufferSize + prevSize;

      // we are in the right buffer
      if (prevSize <= location && location < globalSize) {
        int index = location - prevSize;
        Node* n = &(tempTail->currbuffer[index]);
        n->data = data;
        n->is_set.store(1, std::memory_order_relaxed);  // need this to signal the thread that the data is ready

        // allocating a new buffer and adding it to the queue
        if (index == 1 && !go_back) {
          void* buffer = my_align_malloc(PAGE_SIZE, sizeof(bufferList));
          bufferList* newArr = new(buffer)bufferList(bufferSize, tempTail->positionInQueue + 1, tempTail);
          bufferList* Nullptr = NULL;

          if (!(tempTail->next).compare_exchange_strong(Nullptr, newArr))
            my_align_free(newArr);
        }

        return;
      }

      // the location we got is in the next buffer
      if (location >= globalSize) {
        bufferList* next = (tempTail->next).load(std::memory_order_acquire);

        // we do not have a next buffer - so we can try to allocate a new one
        if (next == NULL) {
          void* buffer = my_align_malloc(PAGE_SIZE, sizeof(bufferList));
          bufferList* newArr = new(buffer)bufferList(bufferSize, tempTail->positionInQueue + 1, tempTail);
          bufferList* Nullptr = NULL;

          // if (CAS(tailOfQueue->next, NULL, newArr)) 1 next-> new array
          if ((tempTail->next).compare_exchange_strong(Nullptr, newArr))
            tailOfQueue.store(newArr, std::memory_order_release);
          else
            my_align_free(newArr);
        } else {
          // if it is not null move to the next buffer
          tailOfQueue.compare_exchange_strong(tempTail, next);
        }
      }
    }
  }
};
