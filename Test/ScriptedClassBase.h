#include <Include/EasyMono.h>
#include <iostream>

// This is a generic class to handle the lifecycle management for scripted classes.
// A simple reference counted object.
class ScriptedClassBase : public EasyMono::ScriptedClass
{
public:
  void AddRef()
  {
    ++refCount;
  }

  void Release()
  {
    if ( --refCount == 0 )
      delete this;
  }

protected:
  ScriptedClassBase( MonoClass* monoClass )
    : ScriptedClass( monoClass )
  {
    std::cout << "ScriptedClassBase " + std::to_string( reinterpret_cast<uintptr_t>( this ) ) << std::endl;
  }

  ~ScriptedClassBase()
  {
    std::cout << "~ScriptedClassBase " + std::to_string( reinterpret_cast<uintptr_t>( this ) ) << std::endl;
  }

private:
  void OnScriptAttached() override
  {
    std::cout << "OnScriptAttached " + std::to_string( reinterpret_cast< uintptr_t >( this ) ) << std::endl;
    AddRef();
  }

  void OnScriptDetached() override
  {
    std::cout << "OnScriptDeatched " + std::to_string( reinterpret_cast<uintptr_t>( this ) ) << std::endl;
    Release();
  }

  int refCount = 0;
};
