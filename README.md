# psc — Prism Shader Compiler

Prism 着色器编译器，将 PSL (Prism Shader Language) 编译为 GLSL / HLSL / MSL / SPIR-V 并输出反射数据。

---

## 构建

```bash
# 首次构建（编译依赖库 + 生成项目）
BuildDeps.bat
GenerateProjects.bat

# 后续只需
GenerateProjects.bat
```

---

## 用法

```
psc [选项] <输入文件>
```

### 基本用法

```bash
# 编译为 GLSL（默认）
psc PBR.Shader

# 指定输出目录
psc PBR.Shader -o build/

# 指定搜索路径
psc PBR.Shader -I Assets/Include -E Assets/Engine
```

### 目标语言

```bash
psc PBR.Shader -g            # GLSL（默认）
psc PBR.Shader -l            # HLSL
psc PBR.Shader -m            # Metal Shading Language
psc PBR.Shader -s            # SPIR-V 二进制
psc PBR.Shader -g -s         # 组合：GLSL + SPIR-V
psc PBR.Shader -a            # 全部（含 JSON 元数据）
```

### 输出元数据

```bash
psc PBR.Shader -j            # 只输出 JSON 元数据
psc PBR.Shader -g -j         # GLSL + JSON
psc PBR.Shader -a            # 全部目标 + JSON
```

### Shader 变体

```bash
psc PBR.Shader -D ALBEDO_MAP -D NORMAL_MAP
```

### 查看帮助

```bash
psc --help
```

---

## 选项

| 选项 | 说明 |
|------|------|
| `input` | 输入文件（必需） |
| `-o, --output DIR` | 输出目录，默认当前目录 |
| `-I DIR` | Include 搜索路径，默认 `Assets/Include` |
| `-E DIR` | 引擎头文件路径，默认 `Assets/Engine` |
| `-g, --glsl` | 生成 GLSL |
| `-l, --hlsl` | 生成 HLSL |
| `-m, --msl` | 生成 Metal Shading Language |
| `-s, --spirv` | 生成 SPIR-V 二进制 |
| `-j, --json` | 输出元数据 JSON |
| `-a, --all` | 全部目标 + JSON |
| `-D KEYWORD` | 定义 shader 关键字（可重复） |
| `-v, --verbose` | 详细输出 |

---

## 输入格式

PSL (Prism Shader Language) 文件 — `.Shader` 扩展名：

```psl
Shader "MyShader"
{
    Properties
    {
        u_Color("Color", Color) = (1, 1, 1, 1)
        u_MainTex("MainTex", Texture2D) = {}
    }
    SubShader
    {
        Pass
        {
            GLSL
            {
                attribute vec3 a_Position: POSITION;
                VARYING vec3 v_WorldPos;
                void vert() { ... }
                void frag() { ... }
            }
        }
    }
}
```

---

---

## 引擎集成

将静态库 `PrismShaderCore` 引入引擎项目：

### premake5

```lua
group "Dependencies"
    include "PrismShaderCompiler/PrismShaderCore"
group ""

project "Prism"
    includedirs { "PrismShaderCompiler/PrismShaderCore/include" }
    links { "PrismShaderCore" }
```

### 代码

```cpp
#include <PrismShaderCore/Compiler.h>
#include <PrismShaderCore/Metadata.h>

using namespace PrismShaderCompiler;

// 初始化（引擎启动时一次）
CompilerConfig config;
config.IncludeRoot = "Assets/Include";
config.EngineRoot  = "Assets/Engine";
config.OnLog = [](LogLevel lv, const std::string& msg) {
    if (lv == LogLevel::Error) PR_CORE_ERROR("{}", msg);
    else                       PR_CORE_WARN("{}", msg);
};
config.ReadFile = Callbacks::ReadFileFromDisk;

ShaderCompiler compiler(config);

// 加载 Shader
auto shader = compiler.CompileFile("path/to/Shader.shader");

// 读取元数据
shader.ShaderName;          // Shader 名称
shader.Uniforms;            // 属性列表（面板用）
shader.Passes;              // Pass 信息
shader.Keywords;            // Variant 关键字 (vector<KeywordDef>，含 IsMultiCompile 标志：true=multi_compile 全量，false=shader_feature 按需)
shader.MaterialLayout;      // UBO 布局
shader.RenderState;         // 渲染状态
std::string json = ToJson(shader);  // JSON 序列化

// 按需生成 Shader 代码
PassOutput out = compiler.GenerateGLSL(shader, passIndex, keywords);
Shader::Create(out.VertexShader, out.FragmentShader);  // OpenGL 编译
```

---

## 输出文件

| 目标 | 文件 |
|------|------|
| GLSL | `ShaderName.vert`, `ShaderName.frag` |
| HLSL | `ShaderName.vert.hlsl`, `ShaderName.frag.hlsl` |
| MSL | `ShaderName.vert.metal`, `ShaderName.frag.metal` |
| SPIR-V | `ShaderName.vert.spv`, `ShaderName.frag.spv` |
| JSON | `ShaderName.meta.json` |
