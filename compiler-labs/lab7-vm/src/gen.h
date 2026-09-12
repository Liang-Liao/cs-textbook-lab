/*
 * gen.h —— AST → 三地址码 翻译接口
 */
#ifndef GEN_H
#define GEN_H

#include "ast.h"
#include "ir.h"

/* 把检查通过的 AST 翻译成四元式（每函数一段 + 主段）。
 * 只应在语义检查通过（nerr==0）后调用——树上不应有 A_ERR/TY_VOID 之
 * 类的病态节点。返回值生命周期到下次调用为止（内部静态单例）。 */
IrProgram *gen_program(const AstNode *prog);

#endif /* GEN_H */
