#include "common.h"
#include "PNode.h"
#include "Node.h"
#include "graph.h"
#include <map>

#define SET_TYPE(x, y) do{ x->width = y->width; x->sign = y->sign;} while(0)
#define COMMU 1
#define NO_COMMU 0
#define SIGN_UINT 0
#define SIGN_SINT 1
#define SIGN_CHILD 2
#define SIGN_NEG_CHILD 3
#define FUNC_NAME(s) (std::string("s_") + s)


static int maxWidth(int a, int b, bool sign = 0) {
  return MAX(a, b);
}

static int minWidth(int a, int b, bool sign = 0) {
  return MAX(a, b);
}

static int maxWidthPlus1(int a, int b, bool sign = 0) {
  return MAX(a, b) + 1;
}

static int sumWidth(int a, int b, bool sign = 0) {
  return a + b;
}

static int minusWidthPos(int a, int b, bool sign = 0) {
  return MAX(1, a - b);
}

static int minusWidth(int a, int b, bool sign = 0) {
  return a - b;
}

static int divWidth(int a, int b, bool sign = 0) {
  return sign? a + 1 : a;
}

static int cvtWidth(int a, int b, bool sign = 0) {
  return sign ? a : a + 1;
}

static int boolWidth(int a, int b, bool sign = 0) {
  return 1;
}

static int dshlWidth(int a, int b, bool sign = 0) {
  return a + 1 << b -1;
}

static int firstWidth(int a, int b, bool sign = 0){
  return a;
}

static int secondWidth(int a, int b, bool sign = 0){
  return b;
}

// 0: uint; 1: child sign
                              // sign  op    widthFunc
std::map<std::string, std::tuple<bool, const char*, bool, int (*)(int, int, bool)>> expr2Map = {
  {"add",   {1, "mpz_add",      COMMU,    maxWidthPlus1}},
  {"sub",   {1, "mpz_sub",      NO_COMMU, maxWidthPlus1}},
  {"mul",   {1, "mpz_mul",      COMMU,    sumWidth}},
  {"div",   {1, "mpz_tdiv_q",   NO_COMMU, divWidth}},
  {"rem",   {1, "mpz_tdiv_r",  NO_COMMU, minWidth}},
  {"lt",    {0, "s_mpz_lt",  NO_COMMU, boolWidth}},
  {"leq",   {0, "s_mpz_leq", NO_COMMU, boolWidth}},
  {"gt",    {0, "s_mpz_gt",  NO_COMMU, boolWidth}},
  {"geq",   {0, "s_mpz_geq", NO_COMMU, boolWidth}},
  {"eq",    {0, "s_mpz_eq", COMMU, boolWidth}},
  {"neq",   {0, "s_mpz_neq", COMMU, boolWidth}},
  {"dshl",  {1, "s_mpz_dshl", NO_COMMU, dshlWidth}},
  {"dshr",  {1, "s_mpz_dshr", NO_COMMU, firstWidth}},
  {"and",   {0, "mpz_and",  COMMU, maxWidth}},
  {"or",    {0, "mpz_ior",  COMMU, maxWidth}},
  {"xor",   {0, "mpz_xor",  COMMU, maxWidth}},
};

                                            // width num
std::map<std::string, std::tuple<bool, int (*)(int, int, bool)>> expr1int1Map = {
  {"pad", {1, maxWidth}},
  {"shl", {1, sumWidth}},
  {"shr", {1, minusWidthPos}},
  {"head", {0, secondWidth}},
  {"tail", {0, minusWidth}},
};

// 0: uint 1: reverse child sign
std::map<std::string, std::tuple<uint8_t, int (*)(int, int, bool)>> expr1Map = {
  {"asUInt",  {SIGN_UINT, firstWidth}},
  {"asSInt",  {SIGN_SINT, firstWidth}},
  {"asClock", {SIGN_UINT, boolWidth}},
  {"cvt",     {SIGN_SINT, cvtWidth}},
  {"neg",     {SIGN_SINT, maxWidthPlus1}},  // a + 1 (second is zero)
  {"not",     {SIGN_UINT, firstWidth}},
  {"andr",    {SIGN_UINT, boolWidth}},
  {"orr",     {SIGN_UINT, boolWidth}},
  {"xorr",    {SIGN_UINT, boolWidth}},
};

static std::map<std::string, PNode*> moduleMap;
static std::map<std::string, Node*> allSignals;
void visitStmts(std::string prefix, graph* g, PNode* stmts);

std::string tmp = std::string("tmp");
std::string tmp2 = std::string("tmp2");

std::string visitExpr(std::string& name, std::string prefix, Node* n, PNode* expr);
void visitType(Node* n, PNode* ptype);

int p_stoi(const char* str);
std::string cons2str(std::string s);

void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  allSignals[s] = n;
}

void addEdge(Node* src, Node* dst) {
  dst->inEdge ++;
  src->next.push_back(dst);
}

void addEdge(std::string str, Node* dst) {
  Assert(allSignals.find(str) != allSignals.end(), "Signal %s is not defined\n", str.c_str());
  addEdge(allSignals[str], dst);
}

Node* str2Node(std::string str) {
  Assert(allSignals.find(str) != allSignals.end(), "Signal %s is not defined\n", str.c_str());
  return allSignals[str];
}

void visitPorts(std::string prefix, graph* g, PNode* ports) { // treat as node
  for(int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());
    Node* io = new Node();
    visitType(io, port->getChild(0));
    io->name = prefix + port->name;
    addSignal(io->name, io);
  }
}

void visitModule(std::string prefix, graph* g, PNode* module) {
  MUX_DEBUG(std::cout << "visit " << module->name << std::endl);
  visitPorts(prefix, g, module->getChild(0));
  visitStmts(prefix, g, module->getChild(1));
  MUX_DEBUG(std::cout << "leave " << module->name << std::endl);
}

void visitType(Node* n, PNode* ptype) {
  switch(ptype->type) {
    case P_Clock: n->width = 1; break;
    case P_INT_TYPE: SET_TYPE(n, ptype); break;
    default: TODO();
  }
}

std::string visit1Expr1Int(std::string& name, std::string prefix, Node* n, PNode* expr) { // pad|shl|shr|head|tail
  Assert(expr1int1Map.find(expr->name) != expr1int1Map.end(), "Operation %s not found\n", expr->name.c_str());
  std::tuple<bool, int (*)(int, int, bool)>info = expr1int1Map[expr->name];
  std::string str = visitExpr(tmp, prefix, n, expr->getChild(0));
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  expr->width = std::get<1>(info)(expr->getChild(0)->width, p_stoi(expr->getExtra(0).c_str()), false);
  return FUNC_NAME(expr->name) + "(" + name + ", " + str + ", " + cons2str(expr->getExtra(0)) + ")";
}

std::string visit1Expr2Int(std::string& name, std::string prefix, Node* n, PNode* expr){ // bits
  std::string str = visitExpr(tmp, prefix, n, expr->getChild(0));
  expr->sign = 0;
  expr->width = p_stoi(expr->getExtra(0).c_str()) - p_stoi(expr->getExtra(1).c_str()) + 1;
  return FUNC_NAME(expr->name) + "(" + name + ", " + str + ", " + cons2str(expr->getExtra(0)) + ", " + cons2str(expr->getExtra(1)) + ")";
}

std::string visit2Expr(std::string& name, std::string prefix, Node* n, PNode* expr) { // add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat
  Assert(expr->getChildNum() == 2, "Invalid childNum for expr %s\n", expr->name.c_str());
  std::string left = visitExpr(tmp, prefix, n, expr->getChild(0));
  std::string right = visitExpr(tmp2, prefix, n, expr->getChild(1));
  Assert(expr2Map.find(expr->name) != expr2Map.end(), "Operation %s not found\n", expr->name.c_str());
  std::tuple<bool, const char*, bool, int (*)(int, int, bool)>info = expr2Map[expr->name];
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  expr->width = std::get<3>(info)(expr->getChild(0)->width, expr->getChild(1)->width, expr->getChild(0)->sign);
  if(std::get<2>(info) && (expr->getChild(1)->width > expr->getChild(0)->width))
    return std::string(std::get<1>(info)) + "(" + name + ", " + right + ", " + left + ")";
  else
    return std::string(std::get<1>(info)) + "(" + name + ", " + left + ", " + right + ")";
}

std::string visit1Expr(std::string& name, std::string prefix, Node* n, PNode* expr) { // asUInt|asSInt|asClock|cvt|neg|not|andr|orr|xorr
  std::string ret = FUNC_NAME(expr->name) + "(" + name + ", " + visitExpr(tmp, prefix, n, expr->getChild(0)) + ")";
  std::tuple<uint8_t, int (*)(int, int, bool)>info = expr1Map[expr->name];
  expr->sign = std::get<0>(info);
  expr->width = std::get<1>(info)(expr->getChild(0)->width, 0, expr->getChild(0)->sign);
  return ret;
}

std::string visitReference(std::string prefix, PNode* expr) { // return ref name
  std::string ret;
  if(expr->getChildNum() == 0) {
    ret = prefix + expr->name;
    // addEdge(ret, n);
    return ret;
  } else {
    PNode* child = expr->getChild(0);
    SET_TYPE(expr, expr->getChild(0));
    switch(child->type) {
      case P_REF_DOT: 
        ret = prefix + expr->name + "_" + visitReference("", child);
        // addEdge(ret, n);
        return ret;
      default: Assert(0, "TODO: invalid ref child type for %s\n", expr->name.c_str());
    }
  }
  Assert(expr->getChildNum() == 0, "TODO: expr %s with childNum %d\n", expr->name, expr->getChildNum());
}

std::string visitMux(std::string& name, std::string prefix, Node* n, PNode* mux) {
  Assert(mux->getChildNum() == 3 && mux->getChild(0)->type == P_REF, "Invalid childNum or child0 for Mux\n");

  // std::string expr = visitExpr(tmp, prefix, n, mux->getChild(0))
  std::string ret = "(mpz_cmp_ui(" + visitReference(prefix, mux->getChild(0)) + ", 0)? " + visitExpr(name, prefix, n, mux->getChild(1)) + " : " + visitExpr(name, prefix, n, mux->getChild(2)) + ")";
  SET_TYPE(mux, mux->getChild(1));
  return ret;
}

std::string cons2str(std::string s) {
  if (s.length() <= 1) return s;
  std::string ret;
  int idx = 1;
  if (s[1] == '-') {
    ret += "-";
    idx = 2;
  }
  if (s[0] == 'b') ret += "0b";
  else if(s[0] == 'o') ret += "0";
  else if(s[0] == 'h') ret += "0x";
  else idx = 0;
  ret += s.substr(idx);
  return ret;
}

std::string visitExpr(std::string& name, std::string prefix, Node* n, PNode* expr) { // return op & update connect
  std::string ret;
  switch(expr->type) {
    case P_1EXPR1INT: return visit1Expr1Int(name, prefix, n, expr);
    case P_2EXPR: return visit2Expr(name, prefix, n, expr);
    case P_REF: ret = visitReference(prefix, expr); addEdge(ret, n); SET_TYPE(expr, allSignals[ret]); return ret;
    case P_EXPR_MUX: return visitMux(name, prefix, n, expr);
    case P_EXPR_INT_INIT: return (expr->sign ? "SInt<" : "UInt<") + std::to_string(expr->width) + ">(" + cons2str(expr->getExtra(0).substr(1, expr->getExtra(0).length()-2)) + ")";
    case P_1EXPR: return visit1Expr(name, prefix, n, expr);
    case P_1EXPR2INT: return visit1Expr2Int(name, prefix, n, expr);
    default: std::cout << expr->type << std::endl; TODO();
  }

}

Node* visitNode(std::string prefix, PNode* node) { // generate new node and connect
  Node* newSig = new Node();
  newSig->name = prefix + node->name;
  addSignal(newSig->name, newSig);
  Assert(node->getChildNum() >= 1, "Invalid childNum for node %s\n", node->name.c_str());
  newSig->op = visitExpr(newSig->name, prefix, newSig, node->getChild(0));
  SET_TYPE(newSig, node->getChild(0));
  return newSig;
}

void visitConnect(std::string prefix, PNode* connect) {
  Assert(connect->getChildNum() == 2, "Invalid childNum for connect %s\n", connect->name.c_str());
  std::string strDst = visitReference(prefix, connect->getChild(0));
  Node* dst = str2Node(strDst);
  if(dst->type == NODE_REG_SRC) dst = dst->regNext;
  dst->op = visitExpr(dst->name, prefix, dst, connect->getChild(1));
}

void visitRegDef(std::string prefix, graph* g, PNode* reg) {
  Node* newReg = new Node();
  newReg->name = prefix + reg->name;
  newReg->type = NODE_REG_SRC;
  visitType(newReg, reg->getChild(0));
  Node* nextReg = new Node();
  nextReg->name = newReg->name + "_next";
  nextReg->type = NODE_REG_DST;
  SET_TYPE(nextReg, newReg);
  newReg->regNext = nextReg;
  nextReg->regNext = newReg;
  addSignal(newReg->name, newReg);
  addSignal(nextReg->name, nextReg);
  g->sources.push_back(newReg);
}

void visitStmts(std::string prefix, graph* g, PNode* stmts) {
  for (int i = 0; i < stmts->getChildNum(); i++) {
    PNode* stmt = stmts->getChild(i);
    switch(stmt->type) {
      case P_INST : 
        Assert(stmt->getExtraNum() >= 1 && moduleMap.find(stmt->getExtra(0)) != moduleMap.end(), "Module %s is not defined!\n", stmt->name.c_str());
        visitModule(prefix + stmt->name + "_", g, moduleMap[stmt->getExtra(0)]);
        break;
      case P_NODE :
        visitNode(prefix, stmt);
        break;
      case P_CONNECT :
        visitConnect(prefix, stmt);
        break;
      case P_REG_DEF:
        visitRegDef(prefix, g, stmt);
        break;
      default: Assert(0, "Invalid stmt %s\n", stmt->name.c_str());
    }
  }
}

void visitTopPorts(graph* g, PNode* ports) {
  for(int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());
    Node* io = new Node();
    visitType(io, port->getChild(0));
    io->name = port->name;
    if(port->type == P_INPUT) {
      g->input.push_back(io);
    } else if(port->type == P_OUTPUT) {
      g->output.push_back(io);
    } else {
      Assert(0, "Invalid port %s with type %d\n", port->name.c_str(), port->type);
    }
    addSignal(io->name, io);
  }
}

void visitTopModule(graph* g, PNode* topModule) {
  Assert(topModule->getChildNum() >= 2 && topModule->getChild(0)->type == P_PORTS, "Invalid top module %s\n", topModule->name.c_str());
  visitTopPorts(g, topModule->getChild(0));
  visitStmts("", g, topModule->getChild(1));
}

graph* AST2Garph(PNode* root) {
  graph* g = new graph();
  g->name = root->name;
  PNode* topModule = NULL;
  for (int i = 0; i < root->getChildNum(); i++) {
    PNode* module = root->getChild(i);
    if(module->name == root->name) topModule = module;
    moduleMap[module->name] = module;
  }
  Assert(topModule, "Top module can not be NULL\n");
  visitTopModule(g, topModule);
  return g;
}
