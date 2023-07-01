/**
 * @file topoSort.cpp
 * @brief 为图结构进行拓扑排序 分解电路图的状态转移到子任务完成
 */

#include "graph.h"
#include "common.h"
#include <set>

int dfsSortNodes(std::vector<Node*>& s, graph* g, int idx) {
  while (!s.empty()) {
    Node* top = s.back();
    top->set_id(idx++);
    g->sorted.push_back(top);
    s.pop_back();

    for (Node* next : top->next) {
      Assert(next->inEdge > 0, "Invalid inEdge %d for node(%s, %d)\n", next->inEdge, next->name.c_str(), next->id);
      next->inEdge--;
      if (!next->inEdge && (next->type != NODE_REG_DST && next->type != NODE_ACTIVE)) s.push_back(next);
    }
  }
  return idx;
}

void computeNextRegs(std::vector<std::vector<Node*>>& nextRegs, graph* g) {
  std::vector<std::set<Node*>>prevRegs(g->sorted.size());
  for (Node* n : g->sources) {
    for(Node* prevNode : n->regNext->prev) prevRegs[prevNode->id].insert(n);
  }
  for (int i = g->sorted.size() - 1; i >= 0; i --) {
    if(g->sorted[i]->type == NODE_REG_SRC) continue;
    for(Node* prevNode : g->sorted[i]->prev) {
      Assert(prevNode->id <= i, "invalid edge from %d to %d\n", prevNode->id, i);
      prevRegs[prevNode->id].insert(prevRegs[i].begin(), prevRegs[i].end());
    }
  }

  for (size_t i = 0; i < g->sources.size(); i ++)
    nextRegs[i].insert(nextRegs[i].end(), prevRegs[g->sources[i]->id].begin(), prevRegs[g->sources[i]->id].end());
}

void sortRegisters(std::vector<std::vector<Node*>>& nextRegs, graph* g, int idx) { // nextRegs: nodes with type NODE_REG_SRC
  Assert(nextRegs.size() == g->sources.size(), "nextReg not match to sources");
  std::vector<Node*> order;
  std::vector<Node*> s;
  for(size_t i = 0; i < nextRegs.size(); i++) {
    g->sources[i]->visited = NOT_VISIT;
    g->sources[i]->scratch = i;
    s.push_back(g->sources[i]);
  }
  while(!s.empty()) {
    Node* top = s.back();
    Assert(top->type == NODE_REG_SRC, "Invalid node type %d for %s\n", top->type, top->name.c_str());
    if(top->visited == EXPANDED) {
      top->visited = VISITED;
      s.pop_back();
      order.push_back(top);
    } else if (top->visited == NOT_VISIT) {
      top->visited = EXPANDED;
      for (Node* n: nextRegs[top->scratch]) {
        if(n == top) continue;
        if(n->visited == EXPANDED) { // top is in loop, mark
          top->regSplit = true;
          top->visited = VISITED;
          break;
        }
      }
      if(top->regSplit){
        s.pop_back();
        continue;
      }
      for (Node* n: nextRegs[top->scratch]) {
        if(n->visited == NOT_VISIT) s.push_back(n);
      }
    } else {
      s.pop_back();
    }
  }

  for (Node* n : g->sources) {
    if(n->regSplit) {
      n->regNext->set_id(idx ++);
      g->sorted.push_back(n->regNext);
    } else {
      n->regNext->name = n->name;
    }
  }

  for (Node* n : order) {
    n->regNext->set_id(idx ++);
    g->sorted.push_back(n->regNext);
  }
  std::cout << "spilt " << g->sources.size() - order.size() << " (" << g->sources.size() << ") registers\n";
}

void topoSort(graph* g) {
  int idx = 0;

  std::vector<std::vector<Node*>> nextRegs(g->sources.size());
  std::vector<Node*> s(g->sources);

  s.insert(s.end(), g->input.begin(), g->input.end());
  s.insert(s.end(), g->constant.begin(), g->constant.end());
  s.insert(s.end(), g->memRdata1.begin(), g->memRdata1.end());

  idx = dfsSortNodes(s,  g, idx);

  computeNextRegs(nextRegs, g);
  sortRegisters(nextRegs, g, idx);
}
