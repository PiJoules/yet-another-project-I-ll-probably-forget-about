#ifndef KERNEL_INCLUDE_KERNEL_LINKEDLIST_H_
#define KERNEL_INCLUDE_KERNEL_LINKEDLIST_H_

namespace kern {

template <typename T>
class Node {
 public:
  Node(const T &val, Node *next) : val_(val), next_(next) {}
  Node(T val) : Node(val, /*next=*/nullptr) {}

  T &get() { return val_; }
  const T &get() const { return val_; }
  Node *next() { return next_; }
  const Node *next() const { return next_; }
  void setNext(Node *next) { next_ = next; }

  // Return the last node in this list.
  Node *back() {
    if (!next_) return this;
    return next_->back();
  }

  // Circular shift the nodes on this list. That is, move the front of the list
  // to the end, then return the new front of the list.
  Node *Cycle() {
    if (!next_) return this;
    Node *back_ = back();
    Node *new_front = next_;
    next_ = nullptr;
    back_->setNext(this);
    return new_front;
  }

  // Create a new node at the end of this list with a new value. Return this
  // new node.
  Node *Append(const T &val) {
    if (next_) {
      return next_->Append(val);
    } else {
      next_ = new Node(val);
      return next_;
    }
  }

 private:
  T val_;
  Node *next_;
};

template <typename T>
using LinkedList = Node<T>;

}  // namespace kern

#endif  // KERNEL_INCLUDE_KERNEL_LINKEDLIST_H_
