struct Foo1 {};

#include <Include/EasyMono.h>
#include <iostream>

struct Foo2 {};

template< typename Impl >
class ScriptedClassBase;

template< typename Impl >
void ScriptedClassDeleter( ScriptedClassBase< Impl >* s );

template< typename Impl >
using ScriptedClassDeleterFn = void( * )( ScriptedClassBase< Impl >* );

template< typename Impl >
using ScriptedPtr = std::unique_ptr< Impl, ScriptedClassDeleterFn< Impl > >;

// This is a generic class to handle the lifecycle management for scripted classes.
// A simple reference counted object.
template< typename Impl >
class ScriptedClassBase : public EasyMono::ScriptedClass
{
public:
  template< typename ...Args >
  static ScriptedPtr< Impl > CreateUnique( Args&&... args )
  {
    ScriptedPtr< Impl > instance( new Impl( std::forward< Args >( args )... ), ScriptedClassDeleter );
    instance->AddRef();
    return instance;
  }

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

template< typename Impl >
inline void ScriptedClassDeleter( ScriptedClassBase< Impl >* s )
{
  s->Release();
}
