# EasyMono
A header only library and a set of tools, making embedding Mono easy.

# The goal
Embedding a scripting engine into a C++ program, like a game is pain. Initializing an embedded library like Mono is easy to start with, but then comes the hard part, going back and forth between the native and the managed world.

There are several approaches. For start, calling to the managed world, it is usually made by manually getting function objects, putting the arguments together then passing them. And getting called from the managed world is similar. But one feels there is a better way, so one start building up a template or macro library but usually a mix of those. And it doesn't matter how smart they are, it is getting out of hand.

The templates can be very smart. Through a set of rules, they can generate code for calling a managed function, but it always gets harder and harder to maintain. They can never know things like argument names, so you have to invoke them from within macros. And finally, as you try to match a managed signature from the native world or the other way around, they need to be kept in sync manually all the time.

EasyMono aims to solve this by two of its tools.

# CPPtoCSInteropGenerator
This is a small program, written in C#. It takes a managed library (a .dll file) and looks for methods with an `ExportToCpp` attribute. For all functions it finds, it generates a C++ function. The native code can just call this native function and it is going to handle calling the managed function properly.

# CStoCPPInteropGenerator
This is an other small program which depends on libclang. It takes C++ header files and examine them. For every class it finds, derived from the `EasyMono::ScriptedClass`class, it generates a C# class which can be used in the managed world, and registers C++ wrapper functions around its public methods, so they can be called from C# effortlessly.

With these two tools, EasyMono creates a seamless interop between the two worlds. All you need to do is to run them as your scripted C++ classes or your managed exported methods change, and the matching interop mechanism will be generated for you. Always up to date.

# The dictionary
Both tools take a path to a dictionary file. This file contains type name pairs. The first is the native name, while the second is the managed name. If the tools encounter these types on interfaces, they will generate code to pass the variable as a structure. It will be a constant reference on the native side, and a `ref readonly` on the managed side.

# EasyMono.h
This single header contains the native code which you need to get mono and the integration up and running. You can include it anywhere where you need to use its functions directly, or the `EasyMono::ScriptedClass` class. But you have to include it into a single CPP file after defining `IMPLEMENT_EASY_MONO` to have the implementation.
You have to have a mono installation which you need to pass to the `EasyMono::Initialize` function. You also need to link against the Mono libraries, see the Test project on how it is set up.

But let's see how it looks:
```
#define IMPLEMENT_EASY_MONO 1
#include <Include/EasyMono.h>

#include "ScriptTest.h"
#include "Generated/RegisterScriptInterface.h"
#include "CSInterop/MonoTest/Testbed.h"

int main()
{
  std::vector< const char* > binaries = { "../TestManaged/bin/Debug/net8.0/TestManaged.dll" };

  EasyMono::Initialize( false, "C:/Program Files/Mono/lib", "C:/Program Files/Mono/etc", binaries.data(), binaries.size() );

  RegisterScriptInterface();

  auto testObject = new Test::ScriptTest( 69, L"Test object" );
  MonoTest::Testbed::TestInterop( testObject, L"This" );

  EasyMono::Teardown();

  return 0;
}
```
The code above is initializing the Mono library, using the installed Mono location and the `TestManaged.dll` managed library, which holds the scripts we want to run.

By running the generator tools before, the `CSInterop/MonoTest/Testbed.h` is being generated, and through it we can call a static managed function `MonoTest::Testbed::TestInterop`. Calling this C++ function actually calls the managed version.
The `Generated/RegisterScriptInterface.h` is also generated to handle calling C++ functions from the managed world. `RegisterScriptInterface();` is a generated function, and it registers all the generated C++ wrappers to Mono, to make this happen.

There is no magic here. The generated C++ and C# files are all human readable, debuggable even if needed (try that with a template library).

And now comes the fun part. `Test::ScriptTest` is a native class, derived from `EasyMono::ScriptedClass`. As a consequence, as an instance is generated from it, it automatically instantiate a wrapper in the managed world. The tools made sure that you can pass a native `Test::ScriptTest` object to a C# function taking one. Under the hood, it will pass the managed wrapper to it. And when it is time to call one of the native methods of the test class from C#, it will correctly make the call happen.

# Scripted class lifecycles
When a native object is created from the C++ side, the managed counterpart will be created on demand. In this case the native object will hold a weak reference to the managed object.

If it is created from managed code, it will automatically create the native object. In this case the weak reference is created immediately.

Also as the managed object is created in any way, it calls `EasyMono::ScriptedClass::OnScriptAttached` on the native object. When its finalizer is called by the garbage collector, it will call  `EasyMono::ScriptedClass::OnScriptDetached` on the native object again. This is made like this so one can decide how the lifecycle of the native class should be handled. C++ has many ways to deal with this, like implementing a reference counter to all the scripted classes.

For this purpose, the test application has the `ScriptedClassBase` class, implementing very basic reference counting. The `Test::ScriptTest` class is actually based on this.

Also as a point of interest, the `ScriptedClassBase` also implements a `CreateUnique` function to show how to integrate this concept with unique pointers.

# Strings
Strings are always a special type. On the interop interfaces, the tools recognize wide strings (wchar_t). The choice is made as the mono strings are utf16 strings so strings can be passed on the border without conversion.

# Project files
Wanted to do this with CMake, I even did. But due to a bug in CMake, if a generated solution has both C++ and C# projects, the C++ project can't be built, due to not having nuget dependencies. Maybe this will be fixed one day, but for now, it is what it is. But the project is really small enough that if you want to use your favorite build system, making this work is really just a matter of minutes.

# Features
EasyMono is work in progress, I make it to support my game, which allows it to be modified later using managed libraries.

The roadmap (to be expanded):
 - [x] Being able to call native functions from the managed world
 - [x] Being able to call managed function from the native world
 - [x] Unique pointer integration showcase
 - [x] Configurable structure types
 - [ ] Automatic handling of structures not in the dictionary
 - [ ] Handling enums in the tools
 - [ ] Handling callbacks in the tools
 - [ ] Adding convenience attributes over get* and set* functions in the generated C# classes
