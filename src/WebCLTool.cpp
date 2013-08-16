#include "WebCLAction.hpp"
#include "WebCLTool.hpp"

#include "clang/Tooling/CompilationDatabase.h"

#include <iostream>

WebCLTool::WebCLTool(int argc, char const **argv,
                     char const *input, char const *output)
    : compilations_(NULL), paths_(), tool_(NULL), output_(output)
{
    compilations_ =
        clang::tooling::FixedCompilationDatabase::loadFromCommandLine(argc, argv);
    if (!compilations_) {
        std::cerr << "Internal error. Can't create compilation database." << std::endl;
        return;
    }

    paths_.push_back(input);
    tool_ = new clang::tooling::ClangTool(*compilations_, paths_);
    if (!tool_) {
        std::cerr << "Internal error. Can't create tool." << std::endl;
        return;
    }
}

WebCLTool::~WebCLTool()
{
    delete tool_;
    tool_ = NULL;
    delete compilations_;
    compilations_ = NULL;
}

int WebCLTool::run()
{
    if (!compilations_ || !tool_)
        return EXIT_FAILURE;
    return tool_->run(this);
}

WebCLPreprocessorTool::WebCLPreprocessorTool(int argc, char const **argv,
                                             char const *input, char const *output)
    : WebCLTool(argc, argv, input, output)
{
}

WebCLPreprocessorTool::~WebCLPreprocessorTool()
{
}

clang::FrontendAction *WebCLPreprocessorTool::create()
{
    return new WebCLPreprocessorAction(output_);
}

WebCLMatcherTool::WebCLMatcherTool(int argc, char const **argv,
                                   char const *input, char const *output)
    : WebCLTool(argc, argv, input, output)
{
}

WebCLMatcherTool::~WebCLMatcherTool()
{
}

clang::FrontendAction *WebCLMatcherTool::create()
{
    return new WebCLMatcherAction(output_);
}

WebCLValidatorTool::WebCLValidatorTool(int argc, char const **argv,
                                       char const *input)
    : WebCLTool(argc, argv, input)
{
}

WebCLValidatorTool::~WebCLValidatorTool()
{
}

clang::FrontendAction *WebCLValidatorTool::create()
{
    return new WebCLValidatorAction;
}
