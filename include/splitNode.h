#include <cstdio>
#include "common.h"
class NodeElement;
bool compMergable(NodeElement* ele1, NodeElement* ele2);

enum ElementType { ELE_EMPTY, ELE_NODE, ELE_INT, ELE_SPACE};
enum OPLevel {OPL_BITS, OPL_LOGI, OPL_ARITH};
#define referNode(iter) (std::get<0>(iter))
#define referHi(iter) (std::get<1>(iter))
#define referLo(iter) (std::get<2>(iter))
#define referLevel(iter) (std::get<3>(iter))

class NodeElement {
public:
  ElementType eleType = ELE_EMPTY;
  Node* node = nullptr;
  mpz_t val;
  int hi, lo;
  std::set<std::tuple<Node*, int, int, OPLevel>> referNodes;
  OPLevel referType;
  NodeElement(ElementType type = ELE_EMPTY, Node* _node = nullptr, int _hi = -1, int _lo = -1) {
    mpz_init(val);
    eleType = type;
    node = _node;
    hi = _hi;
    lo = _lo;
    if (type == ELE_NODE) {
      referNodes.insert(std::make_tuple(_node, _hi, _lo, OPL_BITS));
    }
  }
  NodeElement(std::string str, int base = 16, int _hi = -1, int _lo = -1) {
    mpz_init(val);
    mpz_set_str(val, str.c_str(), base);
    hi = _hi;
    lo = _lo;
    eleType = ELE_INT;
    updateWidth();
  }
  void updateWidth() {
    if (lo != 0) mpz_tdiv_q_2exp(val, val, lo);
    mpz_t mask;
    mpz_init(mask);
    mpz_set_ui(mask, 1);
    mpz_mul_2exp(mask, mask, hi - lo + 1);
    mpz_sub_ui(mask, mask, 1);
    mpz_and(val, val, mask);
    hi = hi - lo;
    lo = 0;
  }
  NodeElement* dup() {
    NodeElement* ret = new NodeElement();
    mpz_set(ret->val, val);
    ret->eleType = eleType;
    ret->node = node;
    ret->hi = hi;
    ret->lo = lo;
    ret->referNodes.insert(referNodes.begin(), referNodes.end());
    return ret;
  }
  /* select [_hi, lo] */
  NodeElement* getBits(int _hi, int _lo) {
    Assert(_hi <= hi - lo + 1, "invalid range [%d, %d]", _hi, lo);
    NodeElement* ret = dup();
    if (eleType == ELE_NODE) {
      ret->hi = _hi + ret->lo;
      ret->lo = _lo + ret->lo;
    } else {
      ret->hi = _hi;
      ret->lo = _lo;
      ret->updateWidth();
    }
    for (auto iter : ret->referNodes) {
      if (referLevel(iter) != OPL_ARITH) {
        referHi(iter) = std::get<2>(iter) + _hi;
        referLo(iter) = std::get<2>(iter) + _lo;
      }
    }
    return ret;
  }
  void merge(NodeElement* ele) {
    Assert(compMergable(this, ele), "not mergable");
    if (eleType == ELE_NODE) {
      lo = ele->lo;
    } else {
      mpz_mul_2exp(val, val, ele->hi - lo + 1);
      mpz_add(val, val, ele->val);
      hi += ele->hi + 1;
      updateWidth();
    }
    referNodes.insert(ele->referNodes.begin(), ele->referNodes.end());
  }
};

class NodeComponent{
public:
  std::vector<NodeElement*> elements;
  int width;
  NodeComponent() {
    width = 0;
  }
  void addElement(NodeElement* element) {
    elements.push_back(element);
    width += element->hi - element->lo + 1;
  }
  void merge(NodeComponent* node) {
    if (elements.size() != 0 && node->elements.size() != 0 && compMergable(elements.back(), node->elements[0])) {
      elements.back()->merge(node->elements[0]);
      elements.insert(elements.end(), node->elements.begin() + 1, node->elements.end());
    } else {
      elements.insert(elements.end(), node->elements.begin(), node->elements.end());
    }
    width += node->countWidth();
    countWidth();
  }
  NodeComponent* dup() {
    NodeComponent* ret = new NodeComponent();
    for (size_t i = 0; i < elements.size(); i ++) {
      ret->addElement(elements[i]->dup());
    }
    return ret;
  }
  NodeComponent* getbits(int hi, int lo) {
    int w = width;
    bool start = false;
    NodeComponent* comp = new NodeComponent();
    if (hi >= width) {
      comp->addElement(new NodeElement("0", 16, hi - MAX(lo, width), 0));
      hi = MAX(lo, width - 1);
    }
    if (hi < width) {
      for (size_t i = 0; i < elements.size(); i ++) {
        int memberWidth = elements[i]->hi - elements[i]->lo + 1;
        if ((w > hi && (w - memberWidth) <= hi)) {
          start = true;
        }
        if (start) {
          int selectHigh = MIN(hi, w-1) - (w - memberWidth);
          int selectLo = MAX(lo, w - memberWidth) - (w - memberWidth);
          comp->addElement(elements[i]->getBits(selectHigh, selectLo));
          if ((w - memberWidth) <= lo) break;
        }
        w = w - memberWidth;
      }
    }
    countWidth();
    return comp;
  }
  int countWidth() {
    int w = 0;
    for (NodeElement* comp : elements) w += comp->hi - comp->lo + 1;
    if (w != width) {
      for (size_t i = 0; i < elements.size(); i ++) {
        printf("  %s [%d, %d] (totalWidth = %d)\n", elements[i]->eleType == ELE_NODE ? elements[i]->node->name.c_str() : (elements[i]->eleType == ELE_INT ? (std::string("0x") + mpz_get_str(nullptr, 16, elements[i]->val)).c_str() : "EMPTY"),
             elements[i]->hi, elements[i]->lo, width);
      }
    }
    Assert(w == width, "width not match %d != %d\n", w, width);
    return width;
  }
  bool assignSegEq(NodeComponent* comp) {
    if (comp->width != width || comp->elements.size() != elements.size()) return false;
    for (size_t i = 0; i < elements.size(); i ++) {
      if ((elements[i]->hi - elements[i]->lo) != (comp->elements[i]->hi - comp->elements[i]->lo)) return false;
    }
    return true;
  }
  bool assignAllEq(NodeComponent* comp) {
    if (comp->width != width || comp->elements.size() != elements.size()) return false;
    for (size_t i = 0; i < elements.size(); i ++) {
      if ((elements[i]->hi - elements[i]->lo) != (comp->elements[i]->hi - comp->elements[i]->lo)) return false;
      if (elements[i]->eleType != comp->elements[i]->eleType) return false;
      if (elements[i]->eleType == ELE_NODE && elements[i]->node != comp->elements[i]->node) return false;
    }
    return true;
  }
  void invalidateAsWhole() {
    elements.clear();
    elements.push_back(new NodeElement(ELE_SPACE, nullptr, width - 1, 0));
  }
  void invalidateAll() {
    for (NodeElement* element : elements) {
      element->eleType = ELE_SPACE;
      element->hi -= element->lo;
      element->lo = 0;
      element->node = nullptr;
    }
  }
  bool fullValid() {
    if (elements.size() == 0) return false;
    for (NodeElement* element : elements) {
      if (element->eleType == ELE_SPACE) return false;
    }
    return true;
  }
  void setReferArith() {
    for (NodeElement* element : elements) {
      for (auto iter : element->referNodes) {
        referLevel(iter) = OPL_ARITH;
      }
    }
  }
  void setReferLogi() {
    for (NodeElement* element : elements) {
      for (auto iter : element->referNodes) {
        if (referLevel(iter) < OPL_LOGI) referLevel(iter) = OPL_LOGI;
      }
    }
  }
  void display() {
    printf("comp width %d %p\n", width, this);
    for (size_t i = 0; i < elements.size(); i ++) {
      NodeElement* element = elements[i];
      printf("=>  %s [%d, %d] (totalWidth = %d)\n", element->eleType == ELE_NODE ? element->node->name.c_str() : (element->eleType == ELE_INT ? (std::string("0x") + mpz_get_str(nullptr, 16, element->val)).c_str() : "EMPTY"),
            element->hi, element->lo, width);
      for (auto iter : element->referNodes) {
        printf("    -> %s [%d %d]\n", std::get<0>(iter)->name.c_str(), std::get<1>(iter), std::get<2>(iter));
      }
    }
  }
};

class Segments {
public:
  std::set<int> cuts;
  std::set<std::pair<int, int>> arithSeg;
  std::set<std::pair<int, int>> logiSeg;
  std::set<std::pair<int, int>> bitsSeg;
  std::set<int> invalidCut;
  int width = 0;
  bool overlap = false;
  Segments(int _width) {
    width = _width;
    cuts.insert(_width - 1);
  }
  Segments(NodeComponent* comp) {
    construct(comp);
  }
  void construct(NodeComponent* comp) {
    width = comp->width;
    int hi = comp->width - 1;
    for (NodeElement* element : comp->elements) {
      addCut(hi);
      hi -= (element->hi - element->lo + 1);
    }
  }

  void addCut(int idx) {
    if (idx >= width || idx < 0) return;
    cuts.insert(idx);
  }
  void addRange(int hi, int lo, OPLevel level) {
    for (auto range : arithSeg) {
      if (hi == range.first && lo == range.second) continue;
      if (hi < range.second || lo > range.first) continue;
      overlap = true;
    }
    for (auto range : logiSeg) {
      if (hi == range.first && lo == range.second) continue;
      if (hi < range.second || lo > range.first) continue;
      overlap = true;
    }
    for (auto range : bitsSeg) {
      if (hi == range.first && lo == range.second) continue;
      if (hi < range.second || lo > range.first) continue;
      overlap = true;
    }

    if(level == OPL_ARITH) arithSeg.insert(std::make_pair(hi, lo));
    else if(level == OPL_LOGI) logiSeg.insert(std::make_pair(hi, lo));
    else if(level == OPL_BITS) bitsSeg.insert(std::make_pair(hi, lo));
    else Panic();
  }
  void updateCut() {
    for (auto iter : arithSeg) {
      for (int i = iter.second; i < iter.first; i ++) invalidCut.insert(i);
      addCut(iter.first);
      addCut(iter.second - 1);
    }
    for (auto iter : logiSeg) {
      addCut(iter.first);
      addCut(iter.second - 1);
    }
    for (auto iter : bitsSeg) {
      addCut(iter.first);
      addCut(iter.second - 1);
    }
    std::set<int> oldCuts(cuts);
    cuts.clear();
    std::set_difference(oldCuts.begin(),oldCuts.end(),invalidCut.begin(),invalidCut.end(),inserter(cuts , cuts.begin()));
  }
};