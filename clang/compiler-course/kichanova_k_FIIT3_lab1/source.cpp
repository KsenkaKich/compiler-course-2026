#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <string>

namespace {

struct Resources {
  clang::SourceLocation loc;
  std::string type;

  bool operator<(const Resources &other) const {
    if (loc.getRawEncoding() != other.loc.getRawEncoding())
      return loc.getRawEncoding() < other.loc.getRawEncoding();
    return type < other.type;
  }
};

class ResourceVisitor final
    : public clang::RecursiveASTVisitor<ResourceVisitor> {
public:
  explicit ResourceVisitor(clang::ASTContext *context,
                           clang::DiagnosticsEngine &diag)
      : m_context(context), m_diag(diag),
        m_sourceManager(context->getSourceManager()) {
    m_memDiag = m_diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                       "resource leak at line %0 - memory");
    m_fileDiag = m_diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                        "resource leak at line %0 - file");
  }

  // поиск вызовов функций
  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *func = call->getDirectCallee();
    if (!func)
      return true;

    std::string name = func->getNameInfo().getName().getAsString();
    clang::SourceLocation loc = call->getExprLoc();

    if (!m_sourceManager.isInMainFile(loc))
      return true;

    if (name == "malloc" || name == "calloc" || name == "realloc") {
      m_resources.insert({loc, "memory"});
    } else if (name == "fopen") {
      m_resources.insert({loc, "file"});
    } else if (name == "free" || name == "fclose") {
      auto it = m_resources.begin();
      while (it != m_resources.end()) {
        if ((name == "free" && it->type == "memory") ||
            (name == "fclose" && it->type == "file")) {
          it = m_resources.erase(it);
          break;
        } else {
          ++it;
        }
      }
    }

    return true;
  }

  // поиск new
  bool VisitCXXNewExpr(clang::CXXNewExpr *newExpr) {
    clang::SourceLocation loc = newExpr->getExprLoc();
    if (m_sourceManager.isInMainFile(loc)) {
      m_resources.insert({loc, "memory"});
    }
    return true;
  }

  // поиск delete
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *deleteExpr) {
    auto it = m_resources.begin();
    while (it != m_resources.end()) {
      if (it->type == "memory") {
        it = m_resources.erase(it);
        break;
      } else {
        ++it;
      }
    }
    return true;
  }

  void printResults() {
    for (const auto &res : m_resources) {
      if (m_sourceManager.isInMainFile(res.loc)) {
        unsigned lineNum = m_sourceManager.getSpellingLineNumber(res.loc);

        if (res.type == "memory") {
          m_diag.Report(res.loc, m_memDiag) << lineNum;
        } else if (res.type == "file") {
          m_diag.Report(res.loc, m_fileDiag) << lineNum;
        }
      }
    }
  }

private:
  clang::SourceManager &m_sourceManager;
  std::multiset<Resources> m_resources;
  clang::ASTContext *m_context;
  clang::DiagnosticsEngine &m_diag;
  unsigned m_memDiag;
  unsigned m_fileDiag;
};

class ResourceConsumer final : public clang::ASTConsumer {
public:
  explicit ResourceConsumer(clang::ASTContext *context,
                            clang::DiagnosticsEngine &diag)
      : m_visitor(context, diag) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.printResults();
  }

private:
  ResourceVisitor m_visitor;
};

class ResourceAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<ResourceConsumer>(&ci.getASTContext(),
                                              ci.getDiagnostics());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<ResourceAction>
    X("analyzer_kichanova_plugin", "resource leak analyzer");