// game/LinkList.hpp -- OWNER: WP3. Port of LinkList.cs (intrusive doubly linked list with two sentinels; Sound's SE round-robin uses it).
#pragma once

class LinkList {
public:
  class Node {
  public:
    Node* m_befor = nullptr;
    Node* m_after = nullptr;
    virtual ~Node() = default;
    void RemoveList() {
      if (m_befor != nullptr) {
        (m_befor->m_after = m_after)->m_befor = m_befor;
        m_befor = nullptr;
        m_after = nullptr;
      }
    }
  };
  Node m_front, m_tail;
  LinkList() { (m_front.m_after = &m_tail)->m_befor = &m_front; }
  Node* GetTop() { return m_front.m_after; }
  Node* GetTail() { return m_tail.m_befor; }
  void SetTop(Node* node) {
    node->RemoveList();
    node->m_befor = &m_front;
    node->m_after = m_front.m_after;
    m_front.m_after->m_befor = node;
    m_front.m_after = node;
  }
  void SetTail(Node* node) {
    node->RemoveList();
    node->m_befor = m_tail.m_befor;
    node->m_after = &m_tail;
    m_tail.m_befor->m_after = node;
    m_tail.m_befor = node;
  }
};
