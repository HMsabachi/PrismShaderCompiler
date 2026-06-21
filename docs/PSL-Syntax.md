# PSL (Prism Shader Language) 语法参考

PSL 是 Prism 引擎的着色器描述语言，文件扩展名 `.Shader`。

---

## 1. 文件结构

```psl
Shader "ShaderName"
{
    Properties { ... }       // 材质属性（可选）
    RenderCommand { ... }    // 全局渲染状态（可选）
    SubShader
    {
        LOD 200              // LOD 值（可选，默认 200）
        Pass
        {
            Name "PassName"           // Pass 名称（可选）
            Tags { "Key" = "Value" }  // 标签（可选）
            RenderCommand { ... }     // Pass 级渲染状态，覆盖全局（可选）
            GLSL { ... }              // 着色器代码（必需）
        }
    }
}
```

注释：`// 单行` 和 `/* 多行 */`

---

## 2. Properties（材质属性）

```
变量名 ("显示名称", 类型) = 默认值
```

### 类型一览

| 类型 | GLSL 类型 | 默认值示例 | 说明 |
|------|-----------|-----------|------|
| `Bool` | `bool` | `true` / `false` | 勾选框 |
| `Float` | `float` | `0.5` | 浮点数 |
| `Int` | `int` | `1` | 整数 |
| `Color` | `vec4` | `(1, 1, 1, 1)` | 颜色 RGBA |
| `Color3` | `vec3` | `(1, 1, 1)` | 颜色 RGB |
| `Vector2` | `vec2` | `(1, 0)` | 二维向量 |
| `Vector3` | `vec3` | `(0, 1, 0)` | 三维向量 |
| `Vector4` | `vec4` | `(1, 1, 1, 1)` | 四维向量 |
| `Texture2D` | `sampler2D` | `{}` 或 `"white"` | 2D 纹理 |
| `Texture2DMS` | `sampler2DMS` | `{}` | 多采样纹理 |
| `TextureCube` | `samplerCube` | `{}` | 立方体贴图 |
| `Range(min, max)` | `float` | `0.5` | 带范围滑块 |
| `Enum(A, B, C)` | `int` | `0` | 下拉枚举 |
| `Matrix3` | `mat3` | `(1,0,0, 0,1,0, 0,0,1)` | 3x3 矩阵 |
| `Matrix4` | `mat4` | `(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)` | 4x4 矩阵 |

### 示例

```psl
Properties
{
    u_Color    ("主颜色",     Color)            = (1, 1, 1, 1)
    u_Metallic ("金属度",     Range(0, 1))      = 0.5
    u_MainTex  ("基础贴图",   Texture2D)        = {}
    u_Normal   ("法线贴图",   Texture2D)        = {}
    u_Blend    ("混合模式",   Enum(Opaque, Cutout, Fade)) = 0
}
```

---

## 3. RenderCommand（渲染状态）

全局和 Per-Pass 均可设置，Pass 级会覆盖全局。

| 命令 | 可选值 | 默认 | 说明 |
|------|--------|------|------|
| `Blend` | `Off` 或 `SrcAlpha OneMinusSrcAlpha` 等 | `SrcAlpha OneMinusSrcAlpha` | 混合模式 |
| `Cull` | `Back` / `Front` / `Off` | `Back` | 面剔除 |
| `ZTest` | `LessEqual` / `Less` / `Equal` / `Greater` / `Always` / `Never` / `Off` | `LessEqual` | 深度测试 |
| `ZWrite` | `On` / `Off` | `On` | 深度写入 |
| `ColorMask` | `RGBA` / `RGB` / `R` / `G` / `B` / `A` / `0` | `RGBA` | 颜色通道掩码 |
| `Offset` | `factor, units` | `0, 0` | 深度偏移 |

### Blend 因子

`Zero` `One` `SrcAlpha` `OneMinusSrcAlpha` `DstAlpha` `OneMinusDstAlpha`

### 示例

```psl
RenderCommand
{
    Blend SrcAlpha OneMinusSrcAlpha
    Cull Back
    ZTest LessEqual
    ZWrite On
    ColorMask RGBA
}
```

---

## 4. GLSL 块（着色器代码）

### 顶点属性

```
attribute 类型 变量名 : 语义;
```

| 语义 | Location | 说明 |
|------|----------|------|
| `POSITION` | 0 | 顶点位置 |
| `NORMAL` | 1 | 法线 |
| `TANGENT` | 2 | 切线 |
| `BINORMAL` | 3 | 副法线 |
| `TEXCOORD0` | 4 | UV0 |
| `TEXCOORD1` | 5 | UV1 |
| `BONEINDICES` | 6 | 骨骼索引 |
| `BONEWEIGHTS` | 7 | 骨骼权重 |
| `INSTANCEID` | 8 | 实例 ID |
| `COLOR` | 9 | 顶点色 |
| `INDEX0`-`INDEX1` | 10-11 | 通用索引 |
| `OTHER0`-`OTHER2` | 12-14 | 自定义 |

### Varying 变量

顶点着色器输出到片元着色器的数据。**必须写成单一结构体**（一个 Pass 只允许一个），大小写均可：

```psl
varying VertexOutput
{
    vec3 WorldPosition;
    vec3 Normal;
    vec2 TexCoord;
    mat3 WorldNormals;
} v2f;
```

成员支持数组：

```psl
varying VertexOutput
{
    vec3  WorldPosition;
    mat3  WorldNormals;
    vec4  ShadowMapCoords[4];   // 数组成员
} v2f;
```

> **提示**：每个 varying 成员会占用若干个 location 槽位（向量 1 个，`mat3` 3 个，`mat4` 4 个，数组 ×元素数）。过多数组或矩阵会迅速耗尽硬件 varying 上限（通常 ~30 个 location）。大型数据（如骨骼矩阵数组 `mat4[128]`）请使用 UBO / SSBO 传递，不要走 varying。

### 片元输出

片元着色器的输出必须使用标准 GLSL `layout(location = N) out` 声明

```psl
layout(location = 0) out vec4 FragColor;
```

支持多渲染目标（MRT）——声明多个不同 location 的输出即可：

```psl
layout(location = 0) out vec4 g_Albedo;
layout(location = 1) out vec4 g_Normal;
layout(location = 2) out vec4 g_MetalRough;
```

### 入口函数

```psl
void vert() { ... }    // 顶点着色器
void frag() { ... }    // 片元着色器
```

### #include

```psl
#include "Common.glsl"          // 搜索路径由 -I 参数指定
```

### #pragma 关键字

```psl
#pragma shader_feature ALBEDO_MAP NORMAL_MAP      // 主变体关键字
#pragma multi_compile FOG_OFF FOG_LINEAR           // 多编译关键字
```

GLSL 代码中使用：

```psl
void frag()
{
#ifdef ALBEDO_MAP
    vec3 albedo = texture(u_AlbedoTexture, vs_Output.TexCoord).rgb;
#else
    vec3 albedo = u_AlbedoColor;
#endif
}
```

### 完整示例

```psl
GLSL
{
    attribute vec3 a_Position : POSITION;
    attribute vec2 a_TexCoord : TEXCOORD0;

    layout(location = 0) out vec4 FragColor;

    VARYING vec2 v_TexCoord;

    void vert()
    {
        gl_Position = Prism_ViewProjection * Prism_Model * vec4(a_Position, 1.0);
        v_TexCoord = a_TexCoord;
    }

    void frag()
    {
        FragColor = texture(u_MainTex, v_TexCoord) * u_Color;
    }
}
```

---

## 5. 引擎内置 Uniform

引擎编译时自动注入以下头文件（通过 `-E` 参数指定目录）：

```
Bindings.glsl            Binding 号常量定义
PrismFrame.glsl          Per-Frame UBO
PrismObject.glsl         Per-Object UBO
PrismShadow.glsl         阴影贴图 + 采样函数
PrismEngineTextures.glsl 引擎内置纹理
```

---

### 5.1 Binding 号常量（Bindings.glsl）

| 常量 | 值 | 说明 |
|------|----|------|
| `PRISM_BINDING_FRAME` | 0 | Per-Frame UBO |
| `PRISM_BINDING_OBJECT` | 1 | Per-Object UBO |
| `PRISM_BINDING_MATERIAL` | 2 | 材质 UBO（可在 CompilerConfig 中修改） |
| `PRISM_SHADOW_MAP0` | 3 | 阴影贴图 Cascade 0 |
| `PRISM_SHADOW_MAP1` | 4 | 阴影贴图 Cascade 1 |
| `PRISM_SHADOW_MAP2` | 5 | 阴影贴图 Cascade 2 |
| `PRISM_SHADOW_MAP3` | 6 | 阴影贴图 Cascade 3 |
| `PRISM_GEOMETRY_PASS_TEXTURE` | 7 | Geometry Pass 纹理（MSAA） |
| `PRISM_BINDING_TEXTURE` | 16 | 材质纹理起始 slot |

---

### 5.2 PrismFrame（Binding = 0）

相机、时间、光照、阴影参数。

```glsl
layout(std140, binding = 0) uniform PrismFrame
{
    mat4   Prism_ViewProjection;          // VP 矩阵
    mat4   Prism_InverseViewProjection;   // VP 逆矩阵
    mat4   Prism_View;                    // View 矩阵
    mat4   Prism_Projection;              // Projection 矩阵

    vec4   Prism_Time;                    // 时间（x=总时间, y=sin(time), z=cos(time)）
    vec3   Prism_CameraPosition;          // 相机世界坐标
    float  Prism_DeltaTime;               // 帧间隔

    vec2   Prism_Resolution;              // 屏幕分辨率
    float  Prism_AspectRatio;             // 屏幕宽高比

    Prism_Light Prism_Lights[1];          // 光源数组
        // Prism_Light { vec3 Direction; vec3 Radiance; float Multiplier; }

    mat4   Prism_ShadowMatrices[4];       // Cascade 阴影矩阵
    vec4   Prism_CascadeSplits;           // Cascade 分割距离
    vec4   Prism_ShadowParams;            // 阴影参数（x=bias, y=normalBias, z=cascadeCount, w=...）
};
```

---

### 5.3 PrismObject（Binding = 1）

每个物体更新一次，包含模型矩阵和骨骼。

```glsl
const int PRISM_MAX_BONES = 128;

layout(std140, binding = 1) uniform PrismObject
{
    mat4 Prism_Model;                     // 模型矩阵
    mat4 Prism_PreviousModel;             // 上一帧模型矩阵（TAA/MotionBlur）
    mat4 Prism_Bones[128];                // 骨骼矩阵数组
};
```

---

### 5.4 PrismMaterial（Binding = 2）

 Properties 块中的非纹理变量。

```glsl
layout(std140, binding = 2) uniform PrismMaterial
{
    vec4  u_Color;
    float u_Metallic;
    float u_Roughness;
    // ... 其他 Properties 变量 ...
};
```

---

### 5.5 Prism_ShadowMap（Binding = 3~6）

Cascaded Shadow Map 四级级联，每级一个 `sampler2DShadow`。同时提供阴影采样函数：

```glsl
layout(binding = 3) uniform sampler2DShadow Prism_ShadowMap0;
layout(binding = 4) uniform sampler2DShadow Prism_ShadowMap1;
layout(binding = 5) uniform sampler2DShadow Prism_ShadowMap2;
layout(binding = 6) uniform sampler2DShadow Prism_ShadowMap3;

// 内置阴影采样：自动选择 Cascade 并在 16 点 Poisson 盘上采样
float Prism_GetShadow(vec3 worldPos, float viewDepth);
```

---

### 5.6 Prism_GeometryPassTexture（Binding = 7）

```glsl
layout(binding = 7) uniform sampler2DMS Prism_GeometryPassTexture;  // MSAA 纹理
```

---

### 5.7 材质纹理（Binding = 16+）

Properties 中声明的 `Texture2D`/`TextureCube` 等从 Binding 16 开始自动分配，按声明顺序递增：

```glsl
layout(binding = 16) uniform sampler2D u_MainTex;
layout(binding = 17) uniform sampler2D u_NormalMap;
// ...
```

---

## 6. 完整 Shader 示例

```psl
Shader "Custom/PBR"
{
    Properties
    {
        u_Color      ("颜色",      Color)         = (1, 1, 1, 1)
        u_Metallic   ("金属度",    Range(0, 1))   = 0.5
        u_Roughness  ("粗糙度",    Range(0, 1))   = 0.5
        u_AlbedoMap  ("Albedo",    Texture2D)     = {}
        u_NormalMap  ("Normal",    Texture2D)     = {}
        u_BlendMode  ("混合模式",  Enum(Opaque, Cutout)) = 0
    }

    RenderCommand
    {
        Cull Back
        ZTest LessEqual
        ZWrite On
    }

    SubShader
    {
        Pass
        {
            Name "Forward"
            Tags { "LightMode" = "Forward" }

            GLSL
            {
                attribute vec3 a_Position : POSITION;
                attribute vec2 a_TexCoord : TEXCOORD0;
                attribute vec3 a_Normal   : NORMAL;

                layout(location = 0) out vec4 FragColor;

                varying VertexOutput
                {
                    vec3 WorldPos;
                    vec2 TexCoord;
                    vec3 Normal;
                } v2f;

                #pragma shader_feature ALBEDO_MAP NORMAL_MAP

                void vert()
                {
                    vec4 worldPos = Prism_Model * vec4(a_Position, 1.0);
                    gl_Position = Prism_ViewProjection * worldPos;
                    v2f.WorldPos = worldPos.xyz;
                    v2f.TexCoord = a_TexCoord;
                    v2f.Normal = mat3(Prism_Model) * a_Normal;
                }

                void frag()
                {
                #ifdef ALBEDO_MAP
                    vec3 albedo = texture(u_AlbedoMap, v2f.TexCoord).rgb * u_Color.rgb;
                #else
                    vec3 albedo = u_Color.rgb;
                #endif
                    FragColor = vec4(albedo, u_Color.a);
                }
            }
        }
    }
}
```
