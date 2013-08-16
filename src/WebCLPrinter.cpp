#include "WebCLPrinter.hpp"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

WebCLPrinter::WebCLPrinter(clang::Rewriter &rewriter)
    : rewriter_(rewriter)
{
}

WebCLPrinter::~WebCLPrinter()
{
}

bool WebCLPrinter::print(llvm::raw_ostream &out, const std::string &comment)
{
    // Insert a comment at the top of the main source file. This is to
    // ensure that at least some modifications are made so that
    // rewrite buffer becomes available.
    clang::SourceManager &manager = rewriter_.getSourceMgr();
    clang::FileID file = manager.getMainFileID();
    clang::SourceLocation start = manager.getLocForStartOfFile(file);
    rewriter_.InsertText(start, comment, true, true);

    const clang::RewriteBuffer *buffer = rewriter_.getRewriteBufferFor(file);
    if (!buffer) {
        // You'll end up here if don't do any transformations.
        return false;
    }

    out << std::string(buffer->begin(), buffer->end());
    out.flush();
    return true;
}

WebCLValidatorPrinter::WebCLValidatorPrinter(
    clang::CompilerInstance &instance, clang::Rewriter &rewriter)
    : WebCLPrinter(rewriter)
    , WebCLTransformingVisitor(instance)
{
}

WebCLValidatorPrinter::~WebCLValidatorPrinter()
{
}

bool WebCLValidatorPrinter::handleTranslationUnitDecl(clang::TranslationUnitDecl *decl)
{
    // Apply transformer modifications.
    WebCLTransformer &transformer = getTransformer();
    if (!transformer.rewrite())
        return false;

    if (!print(llvm::outs(), "// WebCL Validator: validation stage.\n")) {
        fatal("Can't print validator output.");
        return false;
    }

    return true;
}
