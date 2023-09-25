/**
 * @file generator.cpp
 * @brief generator
 */

#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";
#define OLDNAME(node, width) (width <= 64 ? (node->name + "$oldVal") : "oldValMpz")

#define UI(x) (std::string("mpz_get_ui(") + x + ")")
#define nameUI(node) (node->width > 64 ? UI(node->name) : node->name)
#define nodeEqualsZero(node) (node->width > 64 ? "mpz_sgn(" + node->name + ")" : node->name)

#define STEP_START(file, g, node)                                    \
  do {                                                               \
    nodeNum++;                                                       \
    file << "void S" << g->name << "::step" << node->id << "() {\n"; \
    MUX_COUNT(file << "allActiveTimes[" << node->id << "] ++;\n"); \
    MUX_COUNT(file << "activeNum ++;\n"); \
    MUX_COUNT(file << "bool isActive = false;\n"); \
  } while (0)

void setOldVal(std::ofstream& file, Node* node) {
  if (node->type != NODE_REG_SRC && node->status == VALID_NOT_USE) {
    return;
  }
  std::string oldName = node->name + "$oldVal";
  file << (node->width > 64 ? "mpz_set(oldValMpz, " + node->name + ")" :
            widthUType(node->width) + " " + oldName + " = " + node->name) << ";\n";
}

void activateNode(std::ofstream& file, Node* node, std::set<Node*>& nextNodes, std::set<int>& s, int activeType, Node* superNode) {
  for (Node * next : nextNodes) {
    if (next->type == NODE_ARRAY_MEMBER) next = next->parent;
    if (next->status == VALID_NODE && next->type != NODE_ACTIVE) { // TODO: next->type == SUPER_NODE
      Node* activeNode = next->master;
      if (activeNode->setOrder.size() == 0) {
        // std::cout << node->name << " active invalid " << next->name << std::endl;
        continue;
      }
      int activateID = activeNode->id;

      // std::cout << node->name << " " << node->type << " activate " << activeNode->name << " " << activateID << " " << nextNodes.size()<< " origin " << next->name << " "<< next->id << std::endl;
      if (activeType == 0) {  // normal nodes
        Assert(activateID > superNode->id, "%s %d -> %d (<== %s)\n", activeNode->name.c_str(), superNode->id, activateID, activeNode->name.c_str());
        if (node->type == NODE_REG_DST) {
          if (activateID > superNode->id)
          std::cout << "invalid " << node->name << " " << node->type << " " << superNode->id << " -> " << activateID << std::endl;
        } else if (activateID <= superNode->id) {
          std::cout << "invalid " << node->name << " " << superNode->id << " -> " << activateID << std::endl;
        }

      } else if (activeType == 1) {  // regs in stepId()
        if (activateID > superNode->id) continue;
      } else {
        if (activateID <= superNode->id) continue;
      }
      if(s.find(activateID) == s.end()) {
        s.insert(activateID);
      }
    }
  }
}

void activateReg(std::ofstream& file, Node* node, std::vector<Node*>& nextNodes, std::set<int>& s) {

}

void activateRegLeft(std::ofstream& file, Node* node, std::vector<Node*>& nextNodes, std::set<int>& s) {

}

void activate(std::ofstream& file, Node* node, std::set<Node*>& nextNodes, int activeType, Node* superNode) {
  if (nextNodes.size() == 0 && node->dimension.size() == 0) return;
  std::set<int> s;
  std::string oldName = node->type == NODE_REG_DST && node->regNext->regSplit ? node->regNext->name :
                        (node->width > 64 ? "oldValMpz" : node->name + "$oldVal");
  activateNode(file, node, nextNodes, s, activeType, superNode);
  if (node->dimension.size() != 0) {
    for (Node* member : node->member) {
      activateNode(file, member, member->next, s, activeType, superNode);
    }
  }
  if (node->dimension.size() == 0 && s.size() != 0) {
    file << "if("
          << (node->width > 64 ? "mpz_cmp(" + oldName + ", " + node->name + ") != 0" : oldName + " != " + node->name)
          << "){\n";
  }
  if (activeType != 2) {
    MUX_COUNT(file << "isActive = true;\n");
  }
  for (int idx : s) file << "activeFlags[" << idx << "] = true;\n";
  if (node->dimension.size() == 0 && s.size() != 0) file << "}\n";
}

void activate_uncond(std::ofstream& file, Node* node, std::vector<Node*>& nextNodes) {
  std::set<int> s;
  for (Node * next : nextNodes) {
    if (next->status == VALID_NODE && next->type != NODE_ACTIVE) {
      int activateID = next->id == next->clusId ? next->id : next->clusNodes[0]->id;
      if(s.find(activateID) == s.end()) {
        file << "activeFlags[" << activateID << "] = true;\n";
        s.insert(activateID);
      }
    }
  }
}

void mpzInitArray(std::ofstream& file, Node* node) {
  for (size_t depth = 0; depth < node->dimension.size(); depth ++) {
    std::string indent(depth, '\t');
    file << indent;
    file << "for(int i" << depth << " = 0; i" << depth << " < " << node->dimension[depth] << "; i" << depth << " ++) {\n";
  }
  std::string indent(node->dimension.size(), '\t');
  file << indent << "mpz_init2(" << node->name;
  for (size_t depth = 0; depth < node->dimension.size(); depth ++) {
    file << "[i" << depth << "]";
  }
  file << ", " << node->width << ");\n";
  for (size_t depth = 0; depth < node->dimension.size(); depth ++) {
    file << "}\n";
  }
}

inline void dispDimension(std::ofstream& file, Node* node) {
  for (int idx : node->dimension) file << "[" << idx << "]";
}

#define MEM_WRITE(file, w, mem, idx, val)                                     \
  do {                                                                        \
    if (w == 128) {                                                           \
      file << "mpz_div_2exp(t0, " << val->name << ", 64);\n";                 \
      file << mem << "[" << nameUI(idx) << "].hi = " << UI("t0") << ";\n";    \
      file << mem << "[" << nameUI(idx) << "].lo = " << nameUI(val) << ";\n"; \
    } else {                                                                  \
      file << mem << "[" << nameUI(idx) << "] = " << nameUI(val) << ";\n";    \
    }                                                                         \
  } while (0)

#define MEM_READ(file, w, mem, addr, dst)                                                                          \
  do {                                                                                                             \
    if (w == 128) {                                                                                                \
      file << "mpz_set_ui(" << dst->name << ", " << mem << "[" << nameUI(addr) << "].hi);\n";                      \
      file << "mpz_mul_2exp(" << dst->name << ", " << dst->name << ", " << 64 << ");\n";                           \
      file << "mpz_add_ui(" << dst->name << ", " << dst->name << ", " << mem << "[" << nameUI(addr) << "].lo);\n"; \
    } else {                                                                                                       \
      if (dst->width > 64)                                                                                         \
        file << "mpz_set_ui(" << dst->name << ", " << mem << "[" << nameUI(addr) << "]);\n";                       \
      else                                                                                                         \
        file << dst->name << " = " << mem << "[" << nameUI(addr) << "];\n";                                        \
    }                                                                                                              \
  } while (0)

#define DISP_CLUS_INSTS(file, node)                                                                \
  do {                                                                                             \
    for (int clusIdx = node->clusNodes.size() - 1; clusIdx >= 0; clusIdx--) {                      \
      Node* clusMember = node->clusNodes[clusIdx];                                                 \
      for (size_t i = 0; i < clusMember->insts.size(); i++) file << clusMember->insts[i] << "\n"; \
    }                                                                                              \
  } while (0)

#define DISP_INSTS(file, node)                                                       \
  do {                                                                               \
    for (size_t i = 0; i < node->insts.size(); i++) file << node->insts[i] << "\n"; \
  } while (0)

#if 0
#define EMU_LOG(file, id, oldName, node)                                                                          \
  do {                                                                                                            \
    file << "if(cycles >= 0) {\n";                                                                        \
    file << "std::cout << std::hex << \"" << id << ": " << node->name << "(" << node->width << ", " << node->sign \
         << "): \" ";                                                                                             \
    if (node->width > 64) {                                                                                       \
      if (node->dimension.size() == 0) \
      file << ";mpz_out_str(stdout, 16," << oldName << "); std::cout << \" -> \"; mpz_out_str(stdout, 16, "       \
           << node->name << ");";                                                                                 \
      else { \
        file << ";\n"; \
        std::string idxStr, bracket; \
        for (size_t i = 0; i < node->dimension.size(); i ++) { \
          file << "for(int i" << i << " = 0; i" << i << " < " << node->dimension[i] << "; i" << i << " ++) {\n"; \
          idxStr += "[i" + std::to_string(i) + "]"; \
          bracket += "}\n"; \
        } \
        file << "mpz_out_str(stdout, 16, " << node->name << idxStr << "); std::cout << \" \";\n" << bracket;                    \
      }  \
    } else {                                                                                                      \
      if (node->dimension.size() == 0)file << " << +" << oldName << " << \" -> \" << +" << node->name << ";";     \
      else{ \
        file << ";\n"; \
        std::string idxStr, bracket; \
        for (size_t i = 0; i < node->dimension.size(); i ++) { \
          file << "for(int i" << i << " = 0; i" << i << " < " << node->dimension[i] << "; i" << i << " ++) {\n"; \
          idxStr += "[i" + std::to_string(i) + "]"; \
          bracket += "}\n"; \
        } \
        file << "std::cout <<std::hex<< +" << node->name << idxStr << " << \" \";\n" << bracket; \
      }  \
    }                                                                                                             \
    file << "std::cout << std::endl;\n}\n";                                                                       \
  } while (0)
#define WRITE_LOG(file, name, idx, val, width)            \
  do {                                                    \
    file << "if(cycles >= 0) {\n";                \
    file << "std::cout << \"" << name << "[\";";          \
    file << "std::cout << +" << idx << ";";               \
    file << "std::cout << \"] = \"; ";                    \
    if (width > 64) {                                     \
      file << "mpz_out_str(stdout, 16, " << val << ");";  \
    } else {                                              \
      file << "std::cout << std::hex << +" << val << ";"; \
    }                                                     \
    file << "std::cout << std::endl;\n}\n";               \
  } while (0)
#define EMU_LOG2(file, id, node)                                                                                    \
  do {                                                                                                              \
    file << "if(cycles >= 0) {\n";                                                                          \
    file << "std::cout << \"" << id << ": " << node->name << "(" << node->width << ", " << node->sign << "): \" ;"; \
    if (node->width > 64) {                                                                                         \
      file << "mpz_out_str(stdout, 16, " << node->name << "); ";                                                    \
    } else {                                                                                                        \
      file << "std::cout << std::hex << +" << node->name << ";";                                                    \
    }                                                                                                               \
    file << "std::cout << std::endl;\n}\n";                                                                         \
  } while (0)
#else
#define EMU_LOG(...)
#define WRITE_LOG(...)
#define EMU_LOG2(...)
#endif

static int nodeNum = 0;

void genNodeInit(Node* node, std::ofstream& hfile) {
  switch (node->type) {
      case NODE_READER:
      case NODE_WRITER:
        for (Node* member : node->member) {
          if (member->width > 64) hfile << "mpz_init2(" << member->name << ", " << member->width << ");\n";
          if (member->status == CONSTANT_NODE) {
            if (member->width <= 64) {
              hfile << member->name << " = 0x" << member->workingVal->consVal << ";\n";
            } else {
              if (member->workingVal->consVal.length() <= 16)
                hfile << "mpz_set_ui(" << member->name << ", 0x" << member->workingVal->consVal << ");\n";
              else
                hfile << "mpz_set_str(" << member->name << ", 16, \"" << member->workingVal->consVal << "\");\n";
            }
          }
        }
        break;
      case NODE_REG_SRC:
#ifdef DIFFTEST_PER_SIG
        if (node->width > 64) {
          if (node->dimension.size() == 0) hfile << "mpz_init2(" << node->name << "$prev" << ", " << node->width << ");\n";
          else mpzInitArray(hfile, node);
        }
        if (!node->regSplit) break;
#endif
      default:
        if (node->width > 64) {
          if (node->dimension.size() == 0) hfile << "mpz_init2(" << node->name << ", " << node->width << ");\n";
          else mpzInitArray(hfile, node);
        }
    }
}

void genNodeDef(Node* node, std::ofstream& hfile, std::string& mpz_vals
#ifdef DIFFTEST_PER_SIG
, std::ofstream& sigFile
#endif
) {
  switch (node->type) {
    case NODE_READER:
    case NODE_WRITER:
      for (Node* member : node->member) {
        if (member->width > 64)
          mpz_vals += "mpz_t " + member->name + ";\n";
        else
          hfile << widthUType(member->width) << " " << member->name << ";\n";
      }
      break;
    case NODE_L1_RDATA: break;
    case NODE_REG_SRC:
      if (!node->regSplit) break;
    default:
      if (node->width > 64) {
        mpz_vals += "mpz_t " + node->name;
        for (int idx : node->dimension) mpz_vals += "[" + std::to_string(idx) + "]";
        mpz_vals += ";\n";
      } else {
        hfile << widthUType(node->width) << " " << node->name;
        dispDimension(hfile, node);
        hfile << ";\n";
      }
#ifdef DIFFTEST_PER_SIG
      std::string name = std::string(DST_NAME) + "__DOT__" +
        (node->type == NODE_REG_DST ? node->regNext->name : node->name);
      size_t pos;
      while ((pos = name.find("$")) != std::string::npos) {
        if (name.substr(pos + 1, 2) == "io") {
          name.replace(pos, 1, "_");
        } else
          name.replace(pos, 1, "__DOT__");
      }
      if (node->type == NODE_REG_DST)
        sigFile << node->sign << " " << node->width << " " << node->regNext->name + "$prev"
                << " " << name << std::endl;
      else if (node->type != NODE_REG_SRC)
        sigFile << node->sign << " " << node->width << " " << node->name << " " << name << std::endl;
#endif
  }
}

void genHeader(graph* g, std::string headerFile) {
  std::ofstream hfile(std::string(OBJ_DIR) + "/" + headerFile + ".h");
#ifdef DIFFTEST_PER_SIG
  std::ofstream sigFile(std::string(OBJ_DIR) + "/" + "allSig.h");
#endif

  hfile << "#ifndef " << headerFile << "_H\n";
  hfile << "#define " << headerFile << "\n";
  INCLUDE_LIB(hfile, "iostream");
  INCLUDE_LIB(hfile, "vector");
  INCLUDE_LIB(hfile, "gmp.h");
  INCLUDE_LIB(hfile, "assert.h");
  INCLUDE_LIB(hfile, "cstdint");
  INCLUDE_LIB(hfile, "cstring");
  INCLUDE_LIB(hfile, "ctime");
  INCLUDE(hfile, "functions.h");

  hfile << "#define likely(x) __builtin_expect(!!(x), 1)\n#define unlikely(x) "
           "__builtin_expect(!!(x), 0)\n";
  hfile << "class S" << g->name << "{\n"
        << "public:\n";

  // constructor
  hfile << "S" << g->name << "() {" << std::endl;
  hfile << "init_functions();\n";
  MUX_COUNT(for (size_t i = 0; i < g->superNodes.size(); i ++) hfile << "allNames[" << g->superNodes[i]->id << "] = \"" \
                          << (g->superNodes[i]->setOrder.size() > 0 ? g->superNodes[i]->setOrder[0]->name : "") << "\";\n"; );
  MUX_COUNT(for (size_t i = 0; i < g->sorted.size(); i ++) hfile << "nodeNum[" << g->sorted[i]->master->id << "] = " << g->sorted[i]->clusNodes.size() + 1 << ";\n"; );
  for (int i = 1; i <= g->maxTmp; i++) hfile << "mpz_init(__tmp__" << i << ");\n";
  hfile << "mpz_init(t0);\n";
  hfile << "mpz_init(oldValMpz);\n";
  for (Node* node : g->sorted) {
    genNodeInit(node, hfile);
  }
  hfile << "}\n";

  // active flags
  hfile << "std::vector<bool> activeFlags = "
        << "std::vector<bool>(" << g->superNodes.back()->id << ", true);\n";
  // all sigs
  std::string mpz_vals;
  for (Node* node : g->sorted)
    genNodeDef(node, hfile, mpz_vals
    #ifdef DIFFTEST_PER_SIG
    ,sigFile
    #endif
    );
#ifdef DIFFTEST_PER_SIG
  for (Node* node : g->sources) {
    if (node->width > 64) {
      mpz_vals += "mpz_t " + node->name + "$prev";
      for (int idx : node->dimension) mpz_vals += "[" + std::to_string(idx) + "]";
      mpz_vals += ";\n";
    } else {
      hfile << widthUType(node->width) << " " << node->name << "$prev";
      dispDimension(hfile, node);
      hfile << ";\n";
    }
  }
#endif

  hfile << mpz_vals;
  // tmp variable
  hfile << "mpz_t t0;\n";
  hfile << "mpz_t oldValMpz;\n";
  for (int i = 1; i <= g->maxTmp; i++) hfile << "mpz_t __tmp__" << i << ";\n";
  // memory
  for (Node* n : g->memory) {
    hfile << (n->width == 8 ? "uint8_t " :
             (n->width == 16 ? "uint16_t " :
             (n->width == 32 ? "uint32_t " :
             (n->width == 64 ? "uint64_t " : "struct{uint64_t hi; uint64_t lo;}"))));
    Assert(n->width <= 128, "Data type of memory %s is larger than 128\n", n->name.c_str());
    hfile << n->name << "[" << n->val << "];\n";
  }
  // Test
  MUX_COUNT(hfile << "uint64_t activeNum = 0;\n");
  MUX_COUNT(hfile << "uint64_t posActivate = 0;\n");
  MUX_COUNT(hfile << "double funcTime = 0, activeTime = 0, regsTime = 0, memoryTime = 0;\n"); // ms;
  MUX_COUNT(hfile << "std::vector<uint64_t> allActiveTimes = std::vector<uint64_t>(" << g->superNodes.back()->id << ", 0);\n");
  MUX_COUNT(hfile << "std::vector<uint64_t> posActives = std::vector<uint64_t>(" << g->superNodes.back()->id << ", 0);\n");
  MUX_COUNT(hfile << "std::vector<std::string> allNames = std::vector<std::string>(" << g->superNodes.back()->id << ", \"\");\n");
  MUX_COUNT(hfile << "std::vector<uint64_t> nodeNum = std::vector<uint64_t>(" << g->superNodes.back()->id << ", 0);\n");
  // set functions
  for (Node* node : g->input) {
    if (node->width > 64) {
      hfile << "void set_" << node->name << "(mpz_t val) {\n";
      hfile << "mpz_set(" << node->name << ", val);\n";
    } else {
      hfile << "void set_" << node->name << "(uint64_t val) {\n";
      hfile << node->name << " = val;\n";
    }
    std::set<int>ids;
    for (Node* next : node->master->next) {
      if (next->setOrder.size() != 0)
        ids.insert(next->id);
    }
    for (int id : ids) hfile << "activeFlags[" << id << "] = true;\n";
    hfile << "}\n";
  }
  // step functions
  for (size_t i = 0; i < g->superNodes.size(); i++) { hfile << "void step" << g->superNodes[i]->id << "();\n"; }

  // functions
  hfile << "void step();\n";
  hfile << "};\n";
  hfile << "#endif\n";
  hfile.close();
}

void genNodeInsts(Node* node, std::ofstream& sfile, Node* superNode) {
  Node* activeNode;
  int latency;
  switch (node->type) {
    case NODE_REG_SRC:
    case NODE_REG_DST:
      if (node->insts.size() == 0) return;
      if (!node->regNext->regSplit && node->dimension.size() == 0) setOldVal(sfile, node->regNext);
      DISP_CLUS_INSTS(sfile, node);
      DISP_INSTS(sfile, node);
      Assert(node->type == NODE_REG_DST, "invalid Node %s\n", node->name.c_str());
      activate(sfile, node, node->regNext->regSplit ? node->regNext->next : node->next, 1, superNode);
      if (!node->regNext->regSplit) {
        EMU_LOG(sfile, superNode->id, OLDNAME(node, node->width), node);
      } else {
        EMU_LOG(sfile, superNode->id, node->regNext->name, node);
      }
      break;
    case NODE_OTHERS:
    case NODE_OUT:
    case NODE_L1_RDATA:
      if (node->insts.size() == 0 && node->dimension.size() == 0) return;
      if (node->dimension.size() == 0) setOldVal(sfile, node);
      DISP_CLUS_INSTS(sfile, node);
      DISP_INSTS(sfile, node);
      activeNode = node->type == NODE_REG_DST ? node->regNext : node;
      activate(sfile, activeNode, activeNode->next, 0, superNode);
      EMU_LOG(sfile, superNode->id, OLDNAME(node, node->width), node);
      break;
    case NODE_READER:
      latency = node->parent->latency[0];
      if (latency == 0) setOldVal(sfile, node->member[3]);
      DISP_CLUS_INSTS(sfile, node);
      for (Node* member : node->member) {
        DISP_CLUS_INSTS(sfile, member);
        DISP_INSTS(sfile, member);
      }
      if (latency == 0) {
        MEM_READ(sfile, node->parent->width, node->parent->name, node->member[0], node->member[3]);
        activate(sfile, node->member[3], node->next, 0, superNode);
        EMU_LOG(sfile, superNode->id, OLDNAME(node->member[3], node->parent->width), node->member[3]);
        EMU_LOG2(sfile, superNode->id, node->member[0]);
      }
      break;
    case NODE_WRITER:
      DISP_CLUS_INSTS(sfile, node);
      for (Node* member : node->member) {
        DISP_CLUS_INSTS(sfile, member);
        DISP_INSTS(sfile, member);
      }
      latency = node->parent->latency[1];
      if (latency == 0) {
        sfile << "if(" << nodeEqualsZero(node->member[1]) << "&&" << nodeEqualsZero(node->member[4]) << ") {\n";
        MEM_WRITE(sfile, node->parent->width, node->parent->name, node->member[0], node->member[3]);
        WRITE_LOG(sfile, node->parent->name, node->member[0]->name, node->member[3]->name, node->width);
        sfile << "}\n";
      }
      EMU_LOG2(sfile, node->id, node->member[0]);
      EMU_LOG2(sfile, node->id, node->member[1]);
      EMU_LOG2(sfile, node->id, node->member[3]);
      EMU_LOG2(sfile, node->id, node->member[4]);
      break;
    case NODE_READWRITER:
      if (node->parent->latency[0] == 0) {
        sfile << "if(" << node->member[6]->name << ") ";
        setOldVal(sfile, node->member[3]);
      }
      DISP_CLUS_INSTS(sfile, node);
      for (Node* member : node->member) {
        DISP_CLUS_INSTS(sfile, member);
        DISP_INSTS(sfile, member);
      }
      // write mode
      sfile << "if(" << node->member[6]->name << "){\n";
      if (node->parent->latency[1] == 0) {
        sfile << "if(" << nodeEqualsZero(node->member[1]) << "&&" << nodeEqualsZero(node->member[5]) << ") {\n";
        MEM_WRITE(sfile, node->parent->width, node->parent->name, node->member[0], node->member[4]);
        WRITE_LOG(sfile, node->parent->name, node->member[0]->name, node->member[4]->name, node->width);
        sfile << "}\n";
      }
      // read mode
      sfile << "} else {\n";
      if (node->parent->latency[0] == 0) {
        sfile << "activeFlags[" << node->id << "] = false;\n";
        MEM_READ(sfile, node->parent->width, node->parent->name, node->member[0], node->member[3]);
        activate(sfile, node->member[3], node->next, 0, superNode);
        EMU_LOG(sfile, superNode->id, OLDNAME(node, node->parent->width), node->member[3]);
      }
      break;
    case NODE_INP: return;
    default: Assert(0, "Invalid node %s with type %d (id: %d clusId: %d)\n", node->name.c_str(), node->type, node->id, node->clusId);
  }
}

bool checkValid(Node* n) {
  bool retVal = false;
  for (Node* member : n->setOrder) {
    switch (member->type) {
      case NODE_REG_SRC:
      case NODE_REG_DST:
        if (member->insts.size() == 0) {
          continue;
        }
        retVal = true;
      case NODE_OTHERS:
      case NODE_OUT:
      case NODE_L1_RDATA:
        if (member->insts.size() == 0 && member->dimension.size() == 0) {
          continue;
        }
        retVal = true;
      case NODE_READER:
      case NODE_WRITER:
      case NODE_READWRITER:
        retVal = true;
      case NODE_INP: continue;
      default: Assert(0, "Invalid node %s with type %d (id: %d clusId: %d)\n", member->name.c_str(), member->type, member->id, member->clusId);
    }
  }
  if (!retVal) n->status = VALID_NOT_USE;
  return retVal;
}


void genSrc(graph* g, std::string headerFile, std::string srcFile) {
  std::ofstream sfile(std::string(OBJ_DIR) + "/" + srcFile + ".cpp");
  INCLUDE(sfile, headerFile + ".h");
  sfile << "long cycles = 0;\n";
  for (Node* node : g->superNodes) {
    if (node->status == VALID_NOT_USE) continue;
    // generate function
    STEP_START(sfile, g, node);
    sfile << "activeFlags[" << node->id << "] = false;\n";
    for (Node* n : node->setOrder) {
      genNodeInsts(n, sfile, node);
    }
    MUX_COUNT(sfile << "posActivate += isActive;\n");
    MUX_COUNT(sfile << "posActives[" << node->id << "] += isActive;\n");
    sfile << "}\n";
  }

  sfile << "" << "void S" << g->name << "::step() {\n";

#ifdef DIFFTEST_PER_SIG
  for (Node* node: g->sources) {
    if (node->regNext->insts.size() == 0) continue;
    if (node->width > 64) {
      if (node->dimension.size() == 0)
        sfile << "mpz_set(" << node->name << "$prev, " << node->name << ");\n";
      else {
        std::string indexStr;
        std::string bracket;
        for (size_t depth = 0; depth < node->dimension.size(); depth ++) {
          sfile << "for(int i" << depth << " = 0; i" << depth << " < " << node->dimension[depth] << "; i" << depth << " ++) {\n";
          indexStr += "[i" + std::to_string(depth) + "]";
          bracket += "}\n";
        }
        sfile << "mpz_set(" << node->name << "$prev" << indexStr << ", " << node->name << indexStr << ");\n";
        sfile << bracket;
      }
    }
    else {
      if (node->dimension.size() == 0) {
        sfile << node->name << "$prev = " << node->name << ";\n";
      } else {
        sfile << "memcpy(" << node->name << "$prev, " << node->name << ", sizeof(" << node->name << "));\n";
      }
    }
  }
#endif
  MUX_COUNT(sfile << "clock_t befFunc = std::clock();\n");
  for (Node* superNode : g->superNodes) {
    if (superNode->status == VALID_NOT_USE) continue;
    sfile << "if(unlikely(activeFlags[" << superNode->id << "])) "
          << "step" << superNode->id << "();\n";
  }
  MUX_COUNT(sfile << "clock_t aftFunc = std::clock();\n");
  MUX_COUNT(sfile << "funcTime += (double)(aftFunc - befFunc) * 1000 / CLOCKS_PER_SEC;\n");
  // active nodes
  MUX_COUNT(sfile << "clock_t befActive = std::clock();\n");
  for (Node* n : g->active) {
    DISP_CLUS_INSTS(sfile, n);
    for (size_t i = 0; i < n->insts.size(); i++) sfile << n->insts[i] << "\n";
  }
  MUX_COUNT(sfile << "clock_t aftActive = std::clock();\n");
  MUX_COUNT(sfile << "activeTime += (double)(aftActive - befActive) * 1000 / CLOCKS_PER_SEC;\n");
  // update registers
  MUX_COUNT(sfile << "clock_t befRegs = std::clock();\n");
  for (Node* node : g->sources) {
    if (node->regNext->status == CONSTANT_NODE || !node->regSplit) continue;
    if (node->dimension.size() == 0) {
      if (node->next.size()) {
        activate(sfile, node->regNext, node->next, 2, node->regNext->master);
      }

      if (node->width > 64)
        sfile << "mpz_set(" << node->name << ", " << node->name << "$next);\n";
      else
        sfile << node->name << " = " << node->name << "$next;\n";
    } else {
      for (Node* next : node->next) {
        int activateId = next->id == next->clusId ? next->id : next->clusNodes[0]->id;
        if (next->status == VALID_NODE && next->type != NODE_ACTIVE && activateId > node->id) {
          sfile << "activeFlags[" << activateId << "] = true;\n";
        }
      }
      if (node->width > 64) {
        std::string idxStr;
        std::string outBracket;
        for (size_t dNum = 0; dNum < node->dimension.size(); dNum ++) {
          sfile << "for(int i" << dNum << " = 0; i" << dNum << " < " << node->dimension[dNum] << "; i" << dNum << "++){\n";
          idxStr += "[i" + std::to_string(dNum) + "]";
          outBracket += "}\n";
        }

        sfile << "mpz_set(" << node->name << idxStr << ", " << node->name << "$next" << idxStr << ");\n";
        sfile << outBracket;
      }
      else {
        sfile << "memcpy(" << node->name << ", " << node->name << "$next, sizeof(" << node->name << "));\n";

      }
    }
  }
  MUX_COUNT(sfile << "clock_t aftRegs = std::clock();\n");
  MUX_COUNT(sfile << "regsTime += (double)(aftRegs - befRegs) * 1000 / CLOCKS_PER_SEC;\n");
  // update memory rdata & wdata
  MUX_COUNT(sfile << "clock_t befMemory = std::clock();\n");
  sfile << "uint64_t oldValBasic;\n";
  for (Node* node : g->memory) {
    for (Node* rw : node->member) {
      if (rw->type == NODE_READER && node->latency[0] == 1) {
        Node* data = rw->member[3];
        Assert(data->type == NODE_L1_RDATA, "Invalid data type(%d) for reader(%s) with latency 1\n", data->type,
               rw->name.c_str());
        setOldVal(sfile, data);
        MEM_READ(sfile, node->width, node->name, rw->member[0], data);
        activate(sfile, data, rw->next, 0, rw->master);
      } else if (rw->type == NODE_WRITER && node->latency[1] == 1) {
        sfile << "if(" << nodeEqualsZero(rw->member[1]) << " && " << nodeEqualsZero(rw->member[4]) << ") {\n";
        MEM_WRITE(sfile, node->width, node->name, rw->member[0], rw->member[3]);
        for (Node* reader : rw->parent->member) {
          if (reader->type == NODE_READER || reader->type == NODE_READWRITER)
            sfile << "activeFlags[" << reader->master->id << "] = true;\n";
        }
        WRITE_LOG(sfile, node->name, rw->member[0]->name, rw->member[3]->name, node->width);
        sfile << "}\n";
      } else if (rw->type == NODE_READWRITER) {
        if (node->latency[0] == 1) {
          Node* data = rw->member[3];
          sfile << "if(!" << rw->member[6]->name << "){\n";
          setOldVal(sfile, data);
          MEM_READ(sfile, node->width, node->name, rw->member[0], rw->member[3]);
          activate(sfile, data, rw->next, 0, rw);
          sfile << "}\n";
        }
        if (node->latency[1] == 1) {
          sfile << "if(" << rw->member[6]->name << "){\n";
          sfile << "if(" << nodeEqualsZero(rw->member[1]) << " && " << nodeEqualsZero(rw->member[5]) << ") {\n";
          MEM_WRITE(sfile, node->width, node->name, rw->member[0], rw->member[4]);
          for (Node* reader : rw->parent->member) {
            if (reader->type == NODE_READER || reader->type == NODE_READWRITER)
              sfile << "activeFlags[" << reader->id << "] = true;\n";
          }
          sfile << "}\n";
        }
      }
    }
  }
  MUX_COUNT(sfile << "clock_t aftMemory = std::clock();\n");
  MUX_COUNT(sfile << "memoryTime += (double)(aftMemory - befMemory) * 1000 / CLOCKS_PER_SEC;\n");
  // sfile << "std::cout << \"------\\n\";\n";
  sfile << "cycles ++;\n}\n";
}

void generator(graph* g, std::string header, std::string src) {
  for (Node* n : g->superNodes) {
    checkValid(n);
  }
  genHeader(g, header);
  genSrc(g, header, src);

  std::cout << "nodeNum " << nodeNum << " " << g->sorted.size() << std::endl;
}
