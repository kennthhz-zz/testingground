#include <iostream>
#include <memory>

using namespace std;

template <typename T>
class Node
{
  public:
    Node(
      shared_ptr<T> value,
      shared_ptr<Node<T>> next = nullptr,
      shared_ptr<Node<T>> previous = nullptr,
      shared_ptr<Node<T>> up = nullptr,
      shared_ptr<Node<T>> down = nullptr)
    {
      value_ = value;
      next_ = next;
      previous_ = previous;
      up_ = up;
      down_ = down;
    }

    T value_;
    shared_ptr<Node<T>> next_;
    shared_ptr<Node<T>> previous_;
    shared_ptr<Node<T>> up_;
    shared_ptr<Node<T>> down_;
};

template <typename T>
class SkipList
{
  public:

    SkipList(const T& min)
    {
      min_ = min;
      levels_ = 0;
    }

    shared_ptr<Node<T>> Search(const T& valueToSearch)
    {
      shared_ptr<Node<T>> current = top_left_;
      while (current != nullptr)
      {
        if (valueToSearch == current.value_)
        {
          return current;
        }
        else if (valueToSearch > current.value && (current.next_ == nullptr || valueToSearch < current.next_.value))
        {
          if (current.down_ != null)
          {
            current = current.down_;
            continue;
          }
          else
          {
            return current;
          }
        }
        else
          current = current.next_;
      }

      return nullptr;
    }

    void Insert(const T& valueToInsert)
    {
      if (bottom_left_ == nullptr)
      {
        // assert top_left_ should be null
        bottom_left_ = make_shared<Node<T>>(min_);
        auto newNode = make_shared<Node<T>>(valueToInsert, null, bottom_left_);
        top_left_ = bottom_left_;
        levels_ = 1;
        return;
      }

      // find the top position
      auto ref_node = Search(valueToInsert);

      // insert into bottom row
      shared_ptr<Node<T>> newNode = nullptr;
      if  (node == null)
      {
        newNode = make_shared<Node<T>>(valueToInsert, bottom_left_);
      }
      else
      {
        while (ref_node.down_ != nullptr)
        {
          ref_node = ref_node.down_;
        }

        while (ref_node != null)
        {
          if (valueToInsert >= ref_node.value_)
          {
            if (ref_node.next_ != nullptr)
            {
              if (valueToInsert < ref_node.next.value_)
              {
                // insert
                newNode = make_shared<Node<T>>(valueToInsert, ref_node.next_, ref_node);
                ref_node.next_ = newNode;
                newNode.next_.previous_ = newNode;
                break;
              }
              else
              {
                current = ref_node.next_;
              }
            }
            else
            {
              // append
              newNode = make_shared<Node<T>>(valueToInsert, ref_node.next_, ref_node);
              ref_node.next_ = newNode;
            }
          }
        }
      }

      // propagate to upper level
      int current_level = 1;
      while (FlipHead())
      {
        current_level++;
        if (current_level > levels_)
        {
          // Add new level
          ref_node = newNode;
          while (ref_node.previous_ != nullptr)
          {
            ref_node = ref_node.previous_;
          }

          auto minNode = make_shared<Node<T>>(min_, nullptr, nullptr, nullptr, ref_node);
          ref_node.up_ = minNode;
          levels_++;
        }

        // get to current level position by go back and up
        ref_node = newNode;
        while (ref_node.previous_ != nullptr)
        {
          ref_node = ref_node.previous_;
          if (ref_node.up_ != nullptr)
          {
            ref_node = ref_node.up_;
            break;
          }
        }

        auto newNewNode = make_shared<Node<T>>(valueToInsert, ref_node.next_, ref_node, nullptr, newNode);
        ref_node.next_ = newNewNode;
        newNewNode.next_.previous_ = newNewNode;
        newNode.up_ = newNewNode;
        newNode = newNewNode;
      }

  private:
    bool FlipHead()
    {
      return true;
    }

    T min_;
    shared_ptr<Node<T>> top_left_;
    shared_ptr<Node<T>> bottom_left_;
    int levels_;
};

int main()
{
  SkipList<int> list(-1);
}