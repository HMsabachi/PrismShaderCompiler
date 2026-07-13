# CSL (Compute Shader Language) 语法参考

CSL 是 Prism 引擎的计算着色器描述语言，文件扩展名 `.ComputeShader`。底层为标准 GLSL compute shader，由编译器解析后生成 IR / SPIR-V / GLSL / HLSL / MSL 并输出反射数据。

一个 `.ComputeShader` 文件可包含多个 kernel。

---

## 1. 文件结构

```glsl
#version 450

#pragma kernel KernelName [VariantDefines...]

layout(...) uniform sampler/image/buffer 资源;
layout(location = N) uniform 类型 u_Param;

// 共享代码（函数、常量、结构体等，所有 kernel 可见）

[numthreads(x, y, z)]
void KernelName()
{
    // kernel body
}
```

注释：`// 单行` 和 `/* 多行 */`（同 GLSL）

---

## 2. Kernel 声明

每个 kernel 必须先声明后定义。声明用 `#pragma kernel`，定义用 `[numthreads] void Name()`。

### 声明

```glsl
#pragma kernel CSEquirectToCube
#pragma kernel CSIrradiance
```

### 定义

```glsl
[numthreads(32, 32, 1)]
void CSEquirectToCube()
{
    // ...
}
```

- `numthreads(x, y, z)` - 线程组大小，三个常量
- 函数签名固定为 `void Name()`，无参数
- **声明与定义必须一一对应**：定义时若未在 `#pragma kernel` 中声明，编译报错

### 多 Kernel

一个文件可声明多个 kernel，共享顶部的资源、uniform 和辅助函数。编译器为每个 kernel 单独输出一份文件，其余 kernel 的定义会被替换为 `#line` 占位（不参与编译），仅当前 kernel 被设为 `main` 入口。

---

## 3. 变体关键字（可选）

`#pragma kernel` 后可附加变体定义，同一行内空格分隔：

```glsl
#pragma kernel CSMain FOG_LINEAR USE_HIGH_QUALITY
```

变体定义会收集到 kernel 的 `VariantDefines` 中（反射数据可见）。kernel body 内可用 `#ifdef` 等预处理判断。

---

## 4. 资源声明

CSL 解析资源声明用于反射，支持 sampler / image / buffer 三类。声明语法与标准 GLSL 一致。

### Sampler（采样纹理）

```glsl
layout(binding = 0) uniform sampler2D u_Tex;
layout(binding = 1) uniform samplerCube u_Env;
```

支持的 sampler 类型：

| GLSL 类型 | ResourceKind |
|-----------|--------------|
| `sampler2D` | Sampler2D |
| `sampler2DMS` | Sampler2DMS |
| `sampler2DShadow` | Sampler2DShadow |
| `sampler2DArray` | Sampler2DArray |
| `sampler2DArrayShadow` | Sampler2DArrayShadow |
| `sampler3D` | Sampler3D |
| `samplerCube` | SamplerCube |
| `samplerCubeShadow` | SamplerCubeShadow |

### Image（存储图像 / UAV）

```glsl
layout(rgba16f, binding = 2) uniform imageCube o_Output;
layout(r32ui, binding = 3) readonly uniform image2D u_Counter;
```

| GLSL 类型 | ResourceKind |
|-----------|--------------|
| `image2D` | Image2D |
| `image3D` | Image3D |
| `imageCube` | ImageCube |

支持的 image format（layout 限定符内）：

```
rgba32f rgba16f rg32f rg16f r32f r16f
rgba32i rgba16i rgba8i rg32i rg16i rg8i r32i r16i r8i
rgba32ui rgba16ui rgba8ui rg32ui rg16ui rg8ui r32ui r16ui r8ui
rgba16 rgb10_a2 rgba8 rgba8_snorm rg16 rg8 rg8_snorm r16 r8 r8_snorm
r16f_depth r32f_depth
```

可选内存限定符：`readonly` / `writeonly` / `restrict`（位于 `uniform` 之前）。

### Buffer（SSBO）

```glsl
layout(std430, binding = 0) buffer ParticleBuffer
{
    vec4 Particles[];
} u_Particles;
```

- `std140` / `std430` 内存布局限定符
- `buffer` 关键字声明存储缓冲区，块体内容原样透传（不解析成员，仅记录块名与实例名）

---

## 5. 普通 Uniform

普通（非资源）uniform 必须带 `layout(location = N)`：

```glsl
layout(location = 0) uniform float u_Roughness;
layout(location = 1) uniform vec3 u_Color;
```

**裸 `uniform`（无 layout）会报错**：

```
普通 uniform 必须带 layout(location=N)，如: layout(location=0) uniform float u_Param;
```

普通 uniform 进入单独的反射列表（按 location 索引），与资源（按 binding 索引）区分。

---

## 6. 共享代码

文件中不属于 `#pragma kernel`、资源声明、kernel 定义的代码（辅助函数、常量、结构体等）会被收集为共享源，所有 kernel 可见：

```glsl
const float PI = 3.141592;

vec3 sampleHammersley(uint i, uint N)
{
    // ...
}
```

编译器在生成单 kernel IR 时，把共享源作为主体，kernel 定义通过 `#line` 指令精确映射回原始行号，便于报错定位。

---

## 7. 内置变量

kernel body 内可直接使用标准 GLSL compute 内置变量：

| 变量 | 类型 | 说明 |
|------|------|------|
| `gl_GlobalInvocationID` | `uvec3` | 全局线程 ID |
| `gl_LocalInvocationID` | `uvec3` | 组内线程 ID |
| `gl_WorkGroupID` | `uvec3` | 线程组 ID |
| `gl_LocalInvocationIndex` | `uint` | 组内线性索引 |

图像尺寸查询 `imageSize()`、纹理采样 `texture()` / `textureLod()` 等标准 GLSL 函数均可使用。

---

## 8. 反射数据

编译产物 `CompiledComputeShader` 包含：

| 字段 | 说明 |
|------|------|
| `ShaderName` | 文件名 stem（如 `Environment.ComputeShader` -> `Environment`） |
| `Kernels` | kernel 列表：名称、线程组大小、变体定义、函数源 |
| `Resources` | 资源列表：类型、格式、set/binding、读写标志 |
| `Uniforms` | 普通 uniform 列表：类型、location |
| `Bindings` | 绑定信息：set/binding/name/kind（直接从 Parser 反射，不依赖 SPIR-V） |

JSON 输出（`-j` / `-a`）包含上述全部信息，字段含 `kernels` / `resources` / `uniforms` / `bindings`，`ResourceKind` 与 `ImageFormat` 枚举序列化。

---

## 9. 限制

### HLSL 与 imageCube

HLSL 没有 cube-map 的 UAV 类型（`RWTextureCube`）。声明了 `imageCube`（cube 存储图像）的 CSL 文件在 cross-compile 到 HLSL 时会失败，编译器将其降级为 warning（不阻塞其他目标），HLSL 输出文件跳过：

```
[warning] [HLSL] HLSL cross-compilation failed: RWTextureCube does not exist in HLSL.
```

GLSL / SPIR-V / MSL 不受影响。

### 其他

- kernel 函数签名固定为 `void Name()`，不支持参数
- 普通 uniform 必须指定 `layout(location=N)`
- kernel 定义必须先在 `#pragma kernel` 中声明

---

## 10. 完整示例

```glsl
#version 450

#pragma kernel CSEquirectToCube
#pragma kernel CSIrradiance
#pragma kernel CSMipFilter

layout(binding = 0) uniform sampler2D u_EquirectangularTex;
layout(binding = 1) uniform samplerCube u_InputCubeMap;
layout(rgba16f, binding = 2) uniform imageCube o_OutputCube;

layout(location = 0) uniform float u_Roughness;

const float PI = 3.141592;
const float TwoPI = 2.0 * PI;

vec3 GetCubeMapDirection()
{
    vec2 size = vec2(imageSize(o_OutputCube));
    vec2 st = vec2(gl_GlobalInvocationID.xy) / size;
    vec2 uv = 2.0 * vec2(st.x, 1.0 - st.y) - vec2(1.0);

    vec3 ret;
    uint face = gl_GlobalInvocationID.z;
    if      (face == 0) ret = vec3( 1.0, uv.y, -uv.x);
    else if (face == 1) ret = vec3(-1.0, uv.y,  uv.x);
    else if (face == 2) ret = vec3( uv.x,  1.0, -uv.y);
    else if (face == 3) ret = vec3( uv.x, -1.0,  uv.y);
    else if (face == 4) ret = vec3( uv.x, uv.y,   1.0);
    else if (face == 5) ret = vec3(-uv.x, uv.y,  -uv.y);
    return normalize(ret);
}

[numthreads(32, 32, 1)]
void CSEquirectToCube()
{
    vec3 cubeTC = GetCubeMapDirection();
    float phi = atan(cubeTC.z, cubeTC.x);
    float theta = acos(cubeTC.y);
    vec2 uv = vec2(phi / TwoPI + 0.5, theta / PI);

    vec4 color = texture(u_EquirectangularTex, uv);
    imageStore(o_OutputCube, ivec3(gl_GlobalInvocationID), color);
}

[numthreads(32, 32, 1)]
void CSIrradiance()
{
    // ...
}
```

编译输出（`psc Environment.ComputeShader -a`）：

```
Environment.CSEquirectToCube.comp.ir
Environment.CSEquirectToCube.comp.spv
Environment.CSEquirectToCube.comp.glsl
Environment.CSEquirectToCube.comp.metal        # HLSL 因 imageCube 跳过（warning）
Environment.CSIrradiance.comp.ir
...
Environment.meta.json
```
