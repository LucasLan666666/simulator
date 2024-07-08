#include <gmp.h>
#include <cstdio>
#include <queue>
#include <stack>
#include <tuple>
#include <utility>
#include "common.h"

class NodeElement;
bool compMergable(NodeElement* ele1, NodeElement* ele2);

enum ElementType { ELE_EMPTY, ELE_NODE, ELE_INT, ELE_SPACE};
class NodeElement {
public:
  ElementType eleType = ELE_EMPTY;
  Node* node = nullptr;
  mpz_t val;
  int hi, lo;
  std::set<std::tuple<Node*, int, int>> referNodes;
  NodeElement(ElementType type = ELE_EMPTY, Node* _node = nullptr, int _hi = -1, int _lo = -1) {
    mpz_init(val);
    eleType = type;
    node = _node;
    hi = _hi;
    lo = _lo;
    if (type == ELE_NODE) {
      referNodes.insert(std::make_tuple(_node, _hi, _lo));
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
    if (eleType != ELE_SPACE) {
      for (auto iter : ret->referNodes) {
        std::get<1>(iter) = std::get<2>(iter) + _hi;
        std::get<2>(iter) = std::get<2>(iter) + _lo;
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
    for (NodeElement* element : elements) {
      if (element->eleType == ELE_SPACE) return false;
    }
    return true;
  }
};

class Segments {
public:
  std::set<int> cuts;
  std::set<std::pair<int, int>> seg;
  int width = 0;
  bool overlap = false;
  bool isArith = false; // used in arith / updated by arith
  Segments(int _width) {
    width = _width;
    cuts.insert(_width - 1);
  }
  Segments(NodeComponent* comp) {
    construct(comp);
  }
  void construct(NodeComponent* comp) {
    int hi = comp->width - 1;
    for (NodeElement* element : comp->elements) {
      addCut(hi);
      hi -= (element->hi - element->lo + 1);
    }
    width = comp->width;
  }

  void addCut(int idx) {
    if (idx >= width - 1 || idx < 0) return;
    cuts.insert(idx);
  }
  void addRange(int hi, int lo) {
    for (auto range : seg) {
      if (hi == range.first && lo == range.second) continue;
      if (hi < range.second || lo > range.first) continue;
      overlap = true;
    }
    addCut(hi);
    addCut(lo-1);
    seg.insert(std::make_pair(hi, lo));
  }
};
#define NODE_REF first
#define NODE_UPDATE second
/* refer & update segment */
static std::map<Node*, std::pair<Segments*, Segments*>> nodeSegments;

std::map <Node*, NodeComponent*> componentMap;
std::map <ENode*, NodeComponent*> componentEMap;
static std::vector<Node*> checkNodes;
static std::map<Node*, Node*> aliasMap;

NodeComponent* spaceComp(int width) {
  NodeComponent* comp = new NodeComponent();
  comp->addElement(new NodeElement(ELE_SPACE, nullptr, width - 1, 0));
  return comp;
}

void addRefer(NodeComponent* dst, NodeComponent* comp) {
  for (size_t i = 0; i < comp->elements.size(); i ++)
    dst->elements[0]->referNodes.insert(comp->elements[i]->referNodes.begin(), comp->elements[i]->referNodes.end());
}

NodeComponent* mergeMux(NodeComponent* comp1, NodeComponent* comp2) {
  NodeComponent* ret;
  if (comp1->assignSegEq(comp2)) {
    ret = comp1->dup();
    ret->invalidateAll();
    for (size_t i = 0; i < comp1->elements.size(); i ++) {
      ret->elements[i]->referNodes.insert(comp2->elements[i]->referNodes.begin(), comp2->elements[i]->referNodes.end());
    }
  } else {
    ret = spaceComp(comp1->width);
    addRefer(ret, comp1);
    addRefer(ret, comp2);
  }
  return ret;
}

bool compMergable(NodeElement* ele1, NodeElement* ele2) {
  if (ele1->eleType != ele2->eleType) return false;
  if (ele1->eleType == ELE_NODE) return ele1->node == ele2->node && ele1->lo == ele2->hi + 1;
  else return true;
}

NodeComponent* merge2(NodeComponent* comp1, NodeComponent* comp2) {
  NodeComponent* ret = new NodeComponent();
  ret->merge(comp1);
  ret->merge(comp2);
  return ret;
}

Node* getLeafNode(bool isArray, ENode* enode);

void setUpdateArith(Node* node) {
  nodeSegments[node].NODE_UPDATE->isArith = true;
}

void setRefArith(Node* node) {
  nodeSegments[node].NODE_REF->isArith = true;
}

NodeComponent* ENode::inferComponent(Node* n, bool isArith) {
  if (nodePtr) {
    Node* node = getLeafNode(true, this);
    if (node->isArray()) return spaceComp(node->width);
    Assert(componentMap.find(node) != componentMap.end(), "%s is not visited", node->name.c_str());
    NodeComponent* ret = componentMap[node]->getbits(width-1, 0);
    if (!ret->fullValid()) {
      ret = new NodeComponent();
      ret->addElement(new NodeElement(ELE_NODE, node, width - 1, 0));
    }
    componentEMap[this] = ret;
    if (isArith) setRefArith(n);
    return ret;
  }

  NodeComponent* ret = nullptr;
  NodeComponent* child1, *child2;
  switch (opType) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_REM:
    case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
    case OP_DSHL: case OP_DSHR: case OP_AND: case OP_OR: case OP_XOR:
    case OP_ASUINT: case OP_ASSINT: case OP_CVT: case OP_NEG: case OP_NOT:
    case OP_ANDR: case OP_ORR: case OP_XORR:
      ret = spaceComp(width);
      for (ENode* childENode : child) {
        if (childENode) {
          NodeComponent* childComp = childENode->inferComponent(n, true);
          addRefer(ret, childComp);
        }
      }
      setUpdateArith(n);
    break;
    case OP_BITS:
      child1 = getChild(0)->inferComponent(n, isArith);
      ret = child1->getbits(values[0], values[1]);
      break;
    case OP_CAT: 
      child1 = getChild(0)->inferComponent(n, isArith);
      child2 = getChild(1)->inferComponent(n, isArith);
      ret = merge2(child1, child2);
      break;
    case OP_INT: {
      std::string str;
      int base;
      std::tie(base, str) = firStrBase(strVal);
      ret = new NodeComponent();
      ret->addElement(new NodeElement(str, base, width-1, 0));
      break;
    }
    case OP_HEAD:
      child1 = getChild(0)->inferComponent(n, isArith);
      ret = child1->getbits(child1->width - 1, values[0]);
      break;
    case OP_TAIL:
      child1 = getChild(0)->inferComponent(n, isArith);
      ret = child1->getbits(values[0]-1, 0);
      break;
    case OP_SHL:
      child1 = getChild(0)->inferComponent(n, isArith);
      ret = child1->dup();
      ret->addElement(new NodeElement("0", 16, values[0] - 1, 0));
      break;
    case OP_SHR:
      child1 = getChild(0)->inferComponent(n, isArith);
      ret = child1->getbits(child1->width - 1, values[0]);
      break;
    case OP_PAD:
      child1 = getChild(0)->inferComponent(n, isArith);
      ret = child1->getbits(values[0]-1, 0);
      break;
    case OP_WHEN:
      if (!getChild(1) && !getChild(2)) break;
      if (!getChild(1)) {
        child2 = getChild(2)->inferComponent(n, isArith);
        ret = child2->dup();
        ret->invalidateAll();
        break;
      } else if (!getChild(2)) {
        child1 = getChild(1)->inferComponent(n, isArith);
        ret = child1->dup();
        ret->invalidateAll();
        break;
      }
      /* otherwise continue... */
    case OP_MUX:
      getChild(0)->inferComponent(n, true);
      child1 = getChild(1)->inferComponent(n, isArith);
      child2 = getChild(2)->inferComponent(n, isArith);
      ret = mergeMux(child1, child2);
      break;
    default:
      ret = spaceComp(width);
      for (ENode* childENode : child) {
        if (childENode) {
          NodeComponent* childComp = childENode->inferComponent(n, isArith);
          addRefer(ret, childComp);
        }
      }
      break;
  }
  if (!ret) ret = spaceComp(width);
  componentEMap[this] = ret;
  return ret;
}

NodeComponent* Node::inferComponent() {
  if (assignTree.size() != 1 || isArray() || sign || width == 0) {
    NodeComponent* ret = spaceComp(width);
    for (ExpTree* tree : assignTree) {
      NodeComponent* comp = tree->getRoot()->inferComponent(this, false);
      addRefer(ret, comp);
    }
    for (ExpTree* tree : arrayVal) {
      NodeComponent* comp = tree->getRoot()->inferComponent(this, false);
      addRefer(ret, comp);
    }
    return ret;
  }
  return assignTree[0]->getRoot()->inferComponent(this, false);
}

void setComponent(Node* node, NodeComponent* comp) {
  if (comp->fullValid()) {
    for (NodeElement* element : comp->elements) {
      element->referNodes.clear();
      if (element->eleType == ELE_NODE) element->referNodes.insert(std::make_tuple(element->node, element->hi, element->lo));
    }
  }
  componentMap[node] = comp;
/* update node segment */
  if (nodeSegments.find(node) == nodeSegments.end()) nodeSegments[node] = std::make_pair(new Segments(node->width), new Segments(node->width));
/* update segment*/
  nodeSegments[node].second->construct(comp);
}

void ExpTree::replace(Node* oldNode, Node* newNode) {
  std::stack<ENode*> s;
  if (getRoot()) s.push(getRoot());
  if (getlval()) s.push(getlval());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    top->width = -1; // clear width
    if (top->getNode() && getLeafNode(true, top) == oldNode) {
      top->nodePtr = newNode;
      top->child.clear();
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
  getRoot()->inferWidth();
}

ExpTree* dupSplittedTree(ExpTree* tree, Node* regold, Node* regnew) {
  ENode* lvalue = tree->getlval()->dup();
  ENode* rvalue = tree->getRoot()->dup();
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  ret->replace(regold, regnew);
  ret->replace(regold->getDst(), regnew->getDst());
  return ret;
}

ExpTree* dupTreeWithBits(ExpTree* tree, int hi, int lo) {
  ENode* lvalue = tree->getlval()->dup();
  ENode* rvalue = tree->getRoot()->dup();
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(rvalue, nullptr, -1));
  while(!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if (top->nodePtr || top->opType == OP_INT) {
      ENode* bits = new ENode(OP_BITS);
      bits->addChild(top);
      bits->addVal(hi);
      bits->addVal(lo);
      bits->width = hi - lo + 1;
      if (parent) parent->child[idx] = bits;
      else rvalue = bits;
    } else {
      if (top->opType == OP_WHEN || top->opType == OP_MUX) {
        if (top->getChild(1)) s.push(std::make_tuple(top->getChild(1), top, 1));
        if (top->getChild(2)) s.push(std::make_tuple(top->getChild(1), top, 2));
      } else if (top->opType == OP_RESET) {
        if (top->getChild(1)) s.push(std::make_tuple(top->getChild(1), top, 1));
      }
    }
  }
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  return ret;
}

void graph::splitNodes() {
  int num = 0;
  /* initialize nodeSegments */
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) nodeSegments[node] = std::make_pair(new Segments(node->width), new Segments(node->width));
  }
/* update nodeComponent & nodeSegments */
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      NodeComponent* comp = node->inferComponent();
      setComponent(node, comp);
      // printf("after infer %s\n", node->name.c_str());
      // node->display();
      // for (size_t i = 0; i < componentMap[node]->elements.size(); i ++) {
      //   NodeElement* element = componentMap[node]->elements[i];
      //   printf("=>  %s [%d, %d] (totalWidth = %d)\n", element->eleType == ELE_NODE ? element->node->name.c_str() : (element->eleType == ELE_INT ? (std::string("0x") + mpz_get_str(nullptr, 16, element->val)).c_str() : "EMPTY"),
      //        element->hi, element->lo, componentMap[node]->width);
      //   for (auto iter : element->referNodes) {
      //     printf("    -> %s [%d %d]\n", std::get<0>(iter)->name.c_str(), std::get<1>(iter), std::get<2>(iter));
      //   }
      // }
      Assert(node->width == componentMap[node]->countWidth(), "%s width not match %d != %d", node->name.c_str(), node->width, comp->width);
    }
  }
  std::set<Node*> validNodes;
  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    for (int j = sortedSuper[i]->member.size() - 1; j >= 0; j --) {
      Node* node = sortedSuper[i]->member[j];
      NodeComponent* comp = componentMap[node];
      bool isValid = false;
      if (node->type != NODE_OTHERS) isValid = true;
      else if (comp->elements.size() == 1 && ((comp->elements[0]->eleType == ELE_NODE && comp->elements[0]->node == node) || comp->elements[0]->eleType == ELE_SPACE)) {
        isValid = true;
      } else if (comp->elements.size() == 1 && comp->elements[0]->eleType == ELE_INT) {

      } else {
        isValid = validNodes.find(node) != validNodes.end();
      }
      if (isValid) {
        validNodes.insert(node);
        for (NodeElement* element : comp->elements) {
          if (element->eleType == ELE_SPACE) {
            for (auto iter : element->referNodes) {
              Node* referNode = std::get<0>(iter);
              int hi = std::get<1>(iter), lo = std::get<2>(iter);
              if (node == referNode) continue;
              validNodes.insert(referNode);
              nodeSegments[referNode].first->addRange(hi, lo);
            }
          } else if (element->eleType == ELE_NODE) {
            Node* referNode = element->node;
            if (node == referNode) continue;
            validNodes.insert(referNode);
            nodeSegments[referNode].first->addRange(element->hi, element->lo);
          }
        }
      }
    }
  }

/* creating nodes */
  std::map<Node*, std::vector<Node*>> splittedNodes;
  std::map<Node*, std::set<Node*>> splittedNodesSet;
  for (Node* reg : regsrc) {
    if (reg->status != VALID_NODE || reg->isArray() || reg->assignTree.size() > 1 || reg->sign || reg->width == 0) continue;
    if (nodeSegments[reg].first->cuts.size() > 1 || nodeSegments[reg->getDst()].second->cuts.size() > 1) {
      // printf("node %s(w = %d): overlap %d isArith %d %d\n", reg->name.c_str(), reg->width, nodeSegments[reg].first->overlap, nodeSegments[reg].first->isArith, nodeSegments[reg->getDst()].second->isArith);
      // for (int cut : nodeSegments[reg].first->cuts) printf("%d ", cut);
      // printf("\n-------\n");
      // for (int cut : nodeSegments[reg->getDst()].second->cuts) printf("%d ", cut);
      // printf("\n-------\n");
    }
    if (nodeSegments[reg].first->cuts.size() <= 1|| nodeSegments[reg].first->overlap || nodeSegments[reg].first->cuts != nodeSegments[reg->getDst()].second->cuts) continue;
    Segments* seg = nodeSegments[reg].first;
    printf("splitNode %s\n", reg->name.c_str());
    splittedNodes[reg] = std::vector<Node*>();
    splittedNodes[reg].resize(reg->width);
    int lo = 0;
    for (int hi : seg->cuts) {
      Node* newSrcNode = reg->dup(reg->type, reg->name + format("$%d_%d", hi, lo));
      newSrcNode->width = hi - lo + 1;
      newSrcNode->super = new SuperNode(newSrcNode);
      Node* newDstNode = reg->dup(reg->getDst()->type, reg->getDst()->name + format("$%d_%d", hi, lo));
      newDstNode->width = hi - lo + 1;
      newDstNode->super = new SuperNode(newDstNode);

      newSrcNode->bindReg(newDstNode);

      splittedNodes[reg][lo] = splittedNodes[reg][hi] = newSrcNode;
      splittedNodesSet[reg].insert(newSrcNode);
      componentMap[newDstNode] = componentMap[reg->getDst()]->getbits(hi, lo);
      if (supersrc.find(reg->super) != supersrc.end()) supersrc.insert(newSrcNode->super);
      if (reg->updateTree) newSrcNode->updateTree = dupSplittedTree(reg->updateTree, reg, newSrcNode);
      if (reg->resetTree) newSrcNode->resetTree = dupSplittedTree(reg->resetTree, reg, newSrcNode);
      for (ExpTree* tree: reg->assignTree) {
        ExpTree* newTree = dupTreeWithBits(tree, hi, lo);
        newTree->setlval(new ENode(newSrcNode));
        newSrcNode->assignTree.push_back(newTree);
      }
      for (ExpTree* tree: reg->getDst()->assignTree) {
        ExpTree* newTree = dupTreeWithBits(tree, hi, lo);
        newTree->setlval(new ENode(newDstNode));
        newDstNode->assignTree.push_back(newTree);
      }
      lo = hi + 1;
      regsrc.push_back(newSrcNode);
    }
    if (supersrc.find(reg->super) != supersrc.end()) supersrc.erase(reg->super);
    componentMap.erase(reg->getDst());
    reg->status = DEAD_NODE;
    reg->getDst()->status = DEAD_NODE;
  }
/* updating componentMap */
  for (auto iter : componentMap) {
    Node* node = iter.first;
    NodeComponent* comp = iter.second;
    bool missingReferred = false;
    if (comp->elements.size() == 1 && comp->elements[0]->eleType == ELE_NODE && node == comp->elements[0]->node) missingReferred = true;
    else missingReferred = !comp->fullValid();
    if (missingReferred) continue;
    for (size_t i = 0; i < comp->elements.size(); i ++) {
      NodeElement* element = comp->elements[i];
      if (element->eleType != ELE_NODE) continue;
      if (splittedNodes.find(element->node) != splittedNodes.end()) {
        std::vector<NodeElement*> replaceElements;
        int width = element->hi - element->lo + 1;
        int start = element->hi;
        while (width != 0) {
          Assert(splittedNodes[element->node][start], "empty entry");
          Node* referNode = splittedNodes[element->node][start];
          replaceElements.push_back(new NodeElement(ELE_NODE, referNode, referNode->width - 1, 0));
          start -= referNode->width;
          width -= referNode->width;
        }
        if (replaceElements.size() == 1) {
          comp->elements[i] = replaceElements[0];
        } else {
          comp->elements.erase(comp->elements.begin() + i);
          comp->elements.insert(comp->elements.begin() + i, replaceElements.begin(), replaceElements.end());
        }
        i += replaceElements.size() - 1;
      }
    }
  }

  for (auto iter : componentMap) {
    Node* node = iter.first;
    NodeComponent* comp = iter.second;
    bool missingReferred = false;
    if (comp->elements.size() == 1 && comp->elements[0]->eleType == ELE_NODE && node == comp->elements[0]->node) missingReferred = true;
    else missingReferred = !comp->fullValid();
    if (missingReferred) continue;
    // if (comp->elements.size() != 1) continue;
    ENode* newRoot = nullptr;
    int hiBit = node->width - 1;
    for (size_t i = 0; i < comp->elements.size(); i ++) {
      NodeElement* element = comp->elements[i];
      ENode* enode;
      if (element->eleType == ELE_NODE) {
        enode = new ENode(element->node);
        enode->width = element->node->width;
        int validHi = enode->width - 1;
        int validLo = 0;
        if (hiBit > element->hi) {
          validHi += hiBit - element->hi;
          validLo += hiBit - element->hi;
          ENode* lshift = new ENode(OP_SHL);
          lshift->addVal(hiBit - element->hi);
          lshift->addChild(enode);
          lshift->width = validHi + 1;
          enode = lshift;
        } else if (hiBit < element->hi) {
          validHi -= element->hi - hiBit;
          validLo = 0;
          ENode* rshift = new ENode(OP_SHR);
          rshift->addVal(element->hi - hiBit);
          rshift->addChild(enode);
          rshift->width = validHi + 1;
          enode = rshift;
        }

        if ((validHi - validLo) > (element->hi - element->lo) || validHi > hiBit) {
          ENode* bits = new ENode(OP_BITS_NOSHIFT);
          bits->addChild(enode);
          bits->addVal(hiBit);
          bits->addVal(hiBit - (element->hi - element->lo));
          bits->width = hiBit + 1;
          enode = bits;
        }
      } else {
        mpz_t realVal;
        mpz_init(realVal);
        if (hiBit > element->hi) mpz_mul_2exp(realVal, element->val, hiBit - element->hi);
        else if (hiBit < element->hi) mpz_tdiv_q_2exp(realVal, element->val, element->hi - hiBit);
        else mpz_set(realVal, element->val);
        
        enode = new ENode(OP_INT);
        enode->strVal = mpz_get_str(nullptr, 10, realVal);
        enode->width = hiBit + 1;

      }
      if (newRoot) {
        ENode* cat = new ENode(OP_OR);
        cat->addChild(newRoot);
        cat->addChild(enode);
        cat->width = MAX(newRoot->width, enode->width);
        newRoot = cat;
      } else newRoot = enode;
      hiBit -= comp->elements[i]->hi - comp->elements[i]->lo + 1;

    }
    node->assignTree.clear();
    node->assignTree.push_back(new ExpTree(newRoot, new ENode(node)));
    num ++;
  }

/* update superNodes, replace reg by splitted regs */
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    for (Node* member : sortedSuper[i]->member) {
      if (member->type == NODE_REG_SRC || member->type == NODE_REG_DST) {
        Assert(sortedSuper[i]->member.size() == 1, "invalid merged SuperNode");
      }
      if (member->type == NODE_REG_SRC && splittedNodesSet.find(member) != splittedNodesSet.end()) {
        std::set<SuperNode*> replaceSuper;
        for (Node* node : splittedNodesSet[member]) {
          replaceSuper.insert(node->super);
        }
        sortedSuper.erase(sortedSuper.begin() + i);
        sortedSuper.insert(sortedSuper.begin() + i, replaceSuper.begin(), replaceSuper.end());
        i += replaceSuper.size() - 1;
      }
      if (member->type == NODE_REG_DST && splittedNodesSet.find(member->getSrc()) != splittedNodesSet.end()) {
        std::set<SuperNode*> replaceSuper;
        for (Node* node : splittedNodesSet[member->getSrc()]) {
          replaceSuper.insert(node->getDst()->super);
        }
        sortedSuper.erase(sortedSuper.begin() + i);
        sortedSuper.insert(sortedSuper.begin() + i, replaceSuper.begin(), replaceSuper.end());
        i += replaceSuper.size() - 1;
      }
    }
  }
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );
  removeNodesNoConnect(DEAD_NODE);
/* update connection */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }

  reconnectAll();

  printf("[splitNode] update %d nodes (total %ld)\n", num, countNodes());
  printf("[splitNode] split %ld registers (total %ld)\n", splittedNodes.size(), regsrc.size());
}