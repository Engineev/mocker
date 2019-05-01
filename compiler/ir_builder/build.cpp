#include "build.h"

#include "builder.h"
#include "builder_context.h"
#include "preprocessor.h"

namespace mocker {

ir::Module buildIR(const std::shared_ptr<ast::ASTRoot> &ast,
                   const SemanticContext &semanticCtx) {
  renameASTIdentifiers(ast, semanticCtx.scopeResiding,
                       semanticCtx.associatedDecl, semanticCtx.exprType);

  ir::BuilderContext IRCtx(semanticCtx.exprType);
  ast->accept(ir::Builder(IRCtx));
  auto res = IRCtx.getResult();

  // TODO: In current version, function inline must be enabled.
  // implement some simple builtin functions
  { // #_array_#size
    ir::FunctionModule func("#_array_#size", {"ptr"});
    auto bb = func.pushBackBB();
    auto ptr = std::make_shared<ir::Reg>("0");
    auto tmp = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::ArithBinaryInst>(
        tmp, ir::ArithBinaryInst::Sub, ptr,
        std::make_shared<ir::IntLiteral>(8)));
    auto funcRes = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::Load>(funcRes, tmp));
    bb->appendInst(std::make_shared<ir::Ret>(funcRes));
    res.overwriteFunc(func.getIdentifier(), func);
  }
  { // #string#length
    ir::FunctionModule func("#string#length", {"ptr"});
    auto bb = func.pushBackBB();
    auto ptr = std::make_shared<ir::Reg>("0");
    auto tmp = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::ArithBinaryInst>(
        tmp, ir::ArithBinaryInst::Sub, ptr,
        std::make_shared<ir::IntLiteral>(8)));
    auto funcRes = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::Load>(funcRes, tmp));
    bb->appendInst(std::make_shared<ir::Ret>(funcRes));
    res.overwriteFunc(func.getIdentifier(), func);
  }
  /*
  { // #string#ord ( this pos )
    ir::FunctionModule func("#string#ord", {"ptr", "pos"});
    auto bb = func.pushBackBB();
    auto ptr = std::make_shared<ir::Reg>("0");
    auto pos = std::make_shared<ir::Reg>("1");
    auto tmp = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::ArithBinaryInst>(
        tmp, ir::ArithBinaryInst::Add, ptr, pos));
    auto loadRes = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::Load>(loadRes, tmp));
    auto funcRes = func.makeTempLocalReg();
    bb->appendInst(std::make_shared<ir::ArithBinaryInst>(
        funcRes, ir::ArithBinaryInst::BitAnd, loadRes,
        std::make_shared<ir::IntLiteral>(255)));
    bb->appendInst(std::make_shared<ir::Ret>(funcRes));
    res.overwriteFunc(func.getIdentifier(), func);
  }
  */
  return res;
}

} // namespace mocker
