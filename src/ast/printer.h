#ifndef MOCKER_PRINTER_H
#define MOCKER_PRINTER_H

#include <iostream>

#include "fwd.h"
#include "visitor.h"


namespace mocker {
namespace ast {

class Printer : public ConstVisitor {
public:
  Printer(std::ostream & out);

  void operator()(const BuiltinType &node) const override;
  void operator()(const UserDefinedType &node) const override;
  void operator()(const ArrayType &node) const override;

  void operator()(const IntLitExpr &node) const override;
  void operator()(const BoolLitExpr &node) const override;
  void operator()(const StringLitExpr &node) const override;
  void operator()(const NullLitExpr &node) const override;
  void operator()(const IdentifierExpr &node) const override;
  void operator()(const UnaryExpr &disp) const override;
  void operator()(const BinaryExpr &disp) const override;
  void operator()(const FuncCallExpr &disp) const override;
  void operator()(const NewExpr &disp) const override;
private:
  // \t and \b are used to indicate the indention
  class IndentingBuf : public std::streambuf {
  public:
    explicit IndentingBuf(std::ostream & out)
      : out(out), originalBuf(*out.rdbuf()) {
      out.rdbuf(this);
    }

    ~IndentingBuf() override {
      out.rdbuf(&originalBuf);
    }

  protected:
    int overflow(int ch) override {
      if (bol && ch != '\n') {
        for (int i = 0; i < identSize; ++i)
          originalBuf.sputc(' ');
      }
      bol = (ch == '\n');
      if (ch == '\t') {
        identSize += 2;
        return 0;
      }
      else if (ch == '\b') {
        identSize -= 2;
        return 0;
      }
      return originalBuf.sputc((char)ch);
    }

  private:
    std::ostream & out;
    std::streambuf & originalBuf;

    bool bol = true;
    std::size_t identSize = 0;
  };

  std::ostream & out;
  IndentingBuf buf;
};

void print(std::ostream & out, const ASTNode & node);

} // namespace ast
} // namespace mocker

#endif // MOCKER_PRINTER_H
