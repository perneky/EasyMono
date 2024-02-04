#pragma once

#include <Include/EasyMono.h>

namespace Test::Struct1
{
  struct LargerStruct : EasyMono::ScriptedStruct
  {
    int a;
    int b;
    int c;
  };

  struct LargestStruct : EasyMono::ScriptedStruct
  {
    int a;
    int b;
    int c;
    int d;
  };
}

namespace Test::Struct2
{
  struct LargerStruct : EasyMono::ScriptedStruct
  {
    int a;
    int b;
    int c;
  };

  struct LargestStruct : EasyMono::ScriptedStruct
  {
    int a;
    int b;
    int c;
    int d;
  };
}

namespace Test::Enum
{
  enum Enum1
  {
    A,
    B,
    C,
  };

  enum class Enum2 : int64_t
  {
    D,
    E,
    F,
  };
}