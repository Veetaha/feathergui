// Copyright (c)2022 Fundament Software
// For conditions of distribution and use, see copyright notice in "fgOpenGL.h"

#ifndef GL__PROGRAM_OBJECT_H
#define GL__PROGRAM_OBJECT_H

#include "GLRef.h"
#include <string>

namespace GL {
  static constexpr bool IsProgramObj(GLuint i) noexcept { return glIsProgram(i) == GL_TRUE; };
  static constexpr void DeleteProgramObj(GLuint i) noexcept { glDeleteProgram(i); };
  struct ShaderObject;

  struct ProgramObject : GLRef<IsProgramObj, DeleteProgramObj>
  {
    explicit constexpr ProgramObject(GLuint shader) noexcept : GLRef(shader) {}
    explicit constexpr ProgramObject(void* shader) noexcept : GLRef(shader) {}
    constexpr ProgramObject() noexcept                      = default;
    constexpr ProgramObject(ProgramObject&& right) noexcept = default;
    constexpr ProgramObject(const ProgramObject&)         = delete;
    constexpr ~ProgramObject() noexcept                   = default;
    GLExpected<void> attach(ShaderObject&& shader) noexcept;
    GLExpected<void> link() noexcept;
    bool is_valid() const noexcept;
    GLExpected<std::string> log() const noexcept;

    ProgramObject& operator=(const ProgramObject&) = delete;
    ProgramObject& operator=(ProgramObject&& right) noexcept = default;

    static GLExpected<ProgramObject> create() noexcept;
  };
}

#endif
