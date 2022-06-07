/* cpptest - C++ test of feather GUI backends
Copyright (c)2022 Fundament Software

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "backend.h"
#include "resource.h"
#include "linmath.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#define BACKEND fgOpenGL
#define TEST(x)                      \
  if(!(x))                           \
  {                                  \
    printf("Failed test: %s\n", #x); \
  }

extern "C" FG_Backend* BACKEND(void* root, FG_Log log, FG_Behavior behavior);

const char* LEVELS[] = { "FATAL: ", "ERROR: ", "WARNING: ", "NOTICE: ", "DEBUG: " };

// A logging function that just forwards everything to printf
void FakeLog(void* root, FG_Level level, const char* f, ...)
{
  if(level >= 0)
    printf("%s", LEVELS[level]);

  va_list args;
  va_start(args, f);
  vprintf(f, args);
  va_end(args);
  printf("\n");
}

// This assembles a custom projection matrix specifically designed for 2D drawing.
void mat4x4_proj(mat4x4 M, float l, float r, float b, float t, float n, float f)
{
  memset(M, 0, sizeof(mat4x4));

  M[0][0] = 2.0f / (r - l);
  M[0][1] = 0.f;
  M[0][2] = 0.f;
  M[0][3] = 0.f;

  M[1][0] = 0.f;
  M[1][1] = 2.0f / (t - b);
  M[1][2] = 0.f;
  M[1][3] = 0.f;

  M[2][0] = 0.f;
  M[2][1] = 0.f;
  M[2][2] = -((f + n) / (f - n));
  M[2][3] = -1.0f;

  M[3][0] = -(r + l) / (r - l);
  M[3][1] = -(t + b) / (t - b);
  M[3][2] = -((2.f * f * n) / (f - n));
  M[3][3] = 0.f;

  mat4x4_translate_in_place(M, 0, 0, -1.0f);
}

// This "mock element" serves as a fake UI object that contains our state and will do our drawing.
struct MockElement
{
  FG_Resource* image;
  void* shader;
  FG_Resource* vertices;
  void* pipeline;
  uint64_t flags;
  bool close;
};

static constexpr auto WindowDim = FG_Vec2{ 800.f, 600.f };

  // The behavior function simply processes all window messages from the host OS.
FG_Result behavior(FG_Element* element, FG_Context* w, void* ui, FG_Msg* m)
{
  static int counter = 0;
  MockElement& e     = *static_cast<MockElement*>(element);
  if(m->kind == FG_Kind_DRAW)
  {
    FG_Backend* b = *(FG_Backend**)ui;
    (b->beginDraw)(b, w, nullptr);
    void* commands = (b->createCommandList)(b, false);

    // Try drawing a custom shader.
    float proj[4 * 4];
    mat4x4_proj(proj, 0, WindowDim.x, WindowDim.y, 0, 0.2f, 1000.0f);
    FG_ShaderValue values[2];
    values[0].pf32          = (float*)proj;
    values[1].asset         = e.image;
    FG_Command shader       = { FG_Category_SHADER };
    shader.shader.shader    = e.shader;
    shader.shader.input     = e.input;
    shader.shader.primitive = FG_Primitive_TRIANGLE_STRIP;
    shader.shader.count     = 4;
    shader.shader.values    = values;

    (b->execute)(b, w, commands);
    (b->destroyCommandList)(b, commands);

    return FG_Result{ 0 };
  }

  // These are simply some input handling boilerplate code
  if(m->kind == FG_Kind_GETWINDOWFLAGS)
  {
    return e.close ? FG_Result{ FG_WindowFlag_CLOSED } : FG_Result{ 0 };
  }
  if(m->kind == FG_Kind_SETWINDOWFLAGS)
  {
    e.close = e.close || ((m->setWindowFlags.flags & FG_WindowFlag_CLOSED) != 0);
  }
  // We normally close when any keypress happens, but we don't want to close if the user is trying to take a screenshot.
  if((m->kind == FG_Kind_KEYDOWN && m->keyDown.key != FG_Keys_LMENU && m->keyDown.scancode != 84 &&
      m->keyDown.scancode != 88) ||
     m->kind == FG_Kind_MOUSEDOWN)
  {
    e.close = true;
  }
  return FG_Result{ -1 };
}

int main(int argc, char* argv[])
{
  FG_Backend* ui;
  auto b = BACKEND(&ui, FakeLog, behavior);
  if(!b)
  {
    printf("Failed to load backend!\n");
    return -1;
  }

  ui            = b;
  auto e        = MockElement{};

  const char* shader_vs = "#version 110\n"
                          "uniform mat4 MVP;\n"
                          "attribute vec2 vPos;\n"
                          "attribute vec2 vUV;\n"
                          "varying vec2 uv;\n"
                          "void main() { gl_Position = MVP * vec4(vPos.xy, -0.75, 1.0); uv = vUV.xy; }";

  const char* shader_fs = "#version 110\n"
                          "varying vec2 uv;\n"
                          "uniform sampler2D texture;\n"
                          "void main() { gl_FragColor = texture2D(texture, uv).rgba; }";

  float verts[4][4] = {
    {
      850.f,
      10.f,
      0.0f,
      0.0f,
    },
    {
      1050.f,
      10.f,
      1.0f,
      0.0f,
    },
    {
      850.f,
      210.f,
      0.0f,
      1.0f,
    },
    {
      1050.f,
      210.f,
      1.0f,
      1.0f,
    },
  };

  FG_Blend PREMULTIPLY_BLEND = {
    FG_BLEND_ONE,
    FG_BLEND_INV_SRC_ALPHA,
    FG_BLEND_OP_ADD,
    FG_BLEND_ONE,
    FG_BLEND_INV_SRC_ALPHA,
    FG_BLEND_OP_ADD,
    0b1111,
  };

  const char* name;
  uint32_t length;
  uint32_t multi;
  uint8_t type; // enum FG_ShaderType
  uint8_t index;
  bool per_instance; // false if per_vertex
  uint32_t step;
  uint32_t offset;

  FG_ShaderParameter params[]     = { { "MVP", 4, 4, FG_ShaderType_FLOAT }, { 0, 0, 0, FG_ShaderType_TEXTURE } };
  FG_ShaderParameter vertparams[] = { { "vPos", 2, 0, FG_ShaderType_FLOAT }, { "vUV", 2, 0, FG_ShaderType_FLOAT } };

  e.flags    = FG_WindowFlag_RESIZABLE;
  //e.image    = (b->createTexture)(b, 0, (const char*)EXAMPLE_PNG_ARRAY, sizeof(EXAMPLE_PNG_ARRAY), FG_Format_PNG, 0);
  auto fs_shader = (b->compileShader)(b, FG_ShaderStage_Pixel, shader_fs);
  auto vs_shader = (b->compileShader)(b, FG_ShaderStage_Vertex, shader_vs);
  

  auto pos = FG_Vec2{ 200.f, 100.f };
  auto dim = FG_Vec2{ 800.f, 600.f };
  auto w   = (b->createWindow)(b, &e, nullptr, &pos, &dim, "Feather Test", e.flags);
  TEST(w != nullptr);

  if(!w)
  {
    printf("failed to create window!\n");
    return -1;
  }

  e.vertices = (b->createBuffer)(b, w, verts, sizeof(verts), FG_PixelFormat_Vertex);

  auto pipeline = FG_PipelineState{};
  void* (*createPipelineState)(FG_Backend * self, FG_Context * context, FG_PipelineState * pipelinestate,
                               FG_Resource * rendertargets, uint32_t n_targets, FG_Blend * blends,
                               FG_Resource * *vertexbuffer, uint32_t n_buffers, FG_ShaderParameter * attributes,
                               uint32_t n_attributes, FG_Resource * indexbuffer);
  e.pipeline = (b->createPipelineState)(b, w->context, &pipeline, 0, 0, &PREMULTIPLY_BLEND, &e.vertices, 1, vertparams, 2);
  e.close    = false;

  TEST((b->setCursor)(b, w, FG_Cursor_CROSS) == 0);

  TEST((b->setWindow)(b, w, nullptr, nullptr, nullptr, nullptr, nullptr, FG_WindowFlag_RESIZABLE) == 0);
  TEST((b->setWindow)(b, w, &e, nullptr, nullptr, nullptr, "Feather Test Changed", FG_WindowFlag_RESIZABLE) == 0);

  TEST((b->processMessages)(b, 0) != 0);

  const char TEST_TEXT[] = "testtext";

  TEST((b->processMessages)(b, 0) != 0);
  TEST((b->clearClipboard)(b, w, FG_Clipboard_ALL) == 0);
  TEST((b->checkClipboard)(b, w, FG_Clipboard_TEXT) == false);
  TEST((b->checkClipboard)(b, w, FG_Clipboard_WAVE) == false);
  TEST((b->checkClipboard)(b, w, FG_Clipboard_ALL) == false);
  TEST((b->putClipboard)(b, w, FG_Clipboard_TEXT, TEST_TEXT, 9) == 0);
  TEST((b->checkClipboard)(b, w, FG_Clipboard_TEXT) == true);
  TEST((b->checkClipboard)(b, w, FG_Clipboard_WAVE) == false);
  TEST((b->checkClipboard)(b, w, FG_Clipboard_ALL) == true);

  char hold[10];

  TEST((b->getClipboard)(b, w, FG_Clipboard_TEXT, hold, 10) == 9)
  for(int i = 0; i < 8; ++i)
  {
    TEST(TEST_TEXT[i] == hold[i]);
  }

  TEST((b->getClipboard)(b, w, FG_Clipboard_WAVE, hold, 10) == 0)

  while((b->processMessages)(b, 0) != 0 && e.close == false)
  {
  }

  TEST((b->destroyWindow)(b, w) == 0);
  TEST((b->destroyResource)(b, e.image) == 0);
  TEST(FG_DestroyShaderInput(b, e.input) == 0);
  (*b->destroy)(b);
}