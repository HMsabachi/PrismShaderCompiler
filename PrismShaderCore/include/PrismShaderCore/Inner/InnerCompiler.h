#pragma once

#include "InnerTypes.h"
#include "PPFile.h"

namespace PrismShaderCompiler
{

class DiagnosticCollector;

// 独立 GLSL 编译器的门面。
//
// 原料：GLSL 块代码 + Properties 声明 + 文件路径。
// 产出：顶点 GLSL + 片元 GLSL + 变体声明。
//
// 两遍预处理 —— PRISM_VERTEX_SHADER / PRISM_FRAGMENT_SHADER 是立即宏，
// 两个输出阶段看到的活代码不同，条件求值结果也不同。
class InnerCompiler
{
public:
    explicit InnerCompiler(DiagnosticCollector& diag);

    InnerResult Compile(const InnerParams& params);

private:
    // 源缓冲必须在整次改写期间保持稳定：deferred 区是逐字回写原文的
    PPFileTable m_Files;
    DiagnosticCollector& m_Diag;
};

} // namespace PrismShaderCompiler
