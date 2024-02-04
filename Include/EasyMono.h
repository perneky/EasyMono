#pragma once

#include <stdint.h>
#include <mono/metadata/object-forward.h>

namespace EasyMono
{
  void Initialize( bool allowDebugger
                 , const char* monoAssemblyDirectory
                 , const char* monoConfigDirectory
                 , const char** binariesToLoad
                 , size_t binaryCount );

  void Teardown();

  void GarbageCollect( bool fast );

  namespace Detail
  {
    void __stdcall NAddRef( uint64_t nativePtr, MonoObject* monoObject );
    void __stdcall NRelease( uint64_t nativePtr );
  }

  struct ScriptedStruct {};

  class ScriptedClass
  {
    friend void Detail::NAddRef( uint64_t nativePtr, MonoObject* monoObject );
    friend void Detail::NRelease( uint64_t nativePtr );

  public:
    MonoObject* GetOrCreateMonoObject();

  protected:
    ScriptedClass( MonoClass* monoClass );
    virtual ~ScriptedClass();

    virtual void OnScriptAttached() = 0;
    virtual void OnScriptDetached() = 0;

  private:
    void SetMonoObject( MonoObject* monoObject );

    MonoClass* monoClass = nullptr;
    uint32_t monoObjectHandle = 0;
  };
}

#ifdef IMPLEMENT_EASY_MONO
#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/debug-helpers.h>

#include <vector>
#include <string>

#ifndef EASYMONO_ASSERT
# include <cassert>
# define EASYMONO_ASSERT assert
#endif

#ifndef EASYMONO_PRINT_EXCEPTION
# include <iostream>
# define EASYMONO_PRINT_EXCEPTION( e ) ( std::wcerr << e )
#endif

namespace EasyMono
{
  thread_local bool creatingFromCS = false;

  ScriptedClass::ScriptedClass( MonoClass* monoClass )
    : monoClass( monoClass )
  {
  }

  ScriptedClass::~ScriptedClass()
  {
    if ( monoObjectHandle )
      mono_gchandle_free( monoObjectHandle );
  }

  MonoObject* ScriptedClass::GetOrCreateMonoObject()
  {
    auto monoObject = monoObjectHandle ? mono_gchandle_get_target( monoObjectHandle ) : nullptr;
    if ( monoObject )
      return monoObject;

    monoObject = mono_object_new( mono_domain_get(), monoClass );

    auto typeValue = reinterpret_cast< uint64_t >( this );

    auto monoCtorDesc = mono_method_desc_new( ":.ctor(ulong)", false );
    EASYMONO_ASSERT( monoCtorDesc );
    auto monoCtorFunc = mono_method_desc_search_in_class( monoCtorDesc, monoClass );
    EASYMONO_ASSERT( monoCtorFunc );
    mono_method_desc_free( monoCtorDesc );

    void* args[] = { &typeValue };
    mono_runtime_invoke( monoCtorFunc, monoObject, args, nullptr );

    monoObjectHandle = mono_gchandle_new_weakref( monoObject, false );

    return monoObject;
  }

  void ScriptedClass::SetMonoObject( MonoObject* monoObject )
  {
    if ( monoObject )
    {
      EASYMONO_ASSERT( !monoObjectHandle );
      monoObjectHandle = mono_gchandle_new_weakref( monoObject, false );
      OnScriptAttached();
    }
    else
    {
      EASYMONO_ASSERT( monoObjectHandle );
      mono_gchandle_free( monoObjectHandle );
      monoObjectHandle = 0;
      OnScriptDetached();
    }
  }

  ScriptedClass* LoadNativePointer( MonoObject* monoObject )
  {
    if ( !monoObject )
      return nullptr;

    auto monoClass = mono_object_get_class( monoObject );
    assert( monoClass );
    auto pointerField = mono_class_get_field_from_name( monoClass, "pointer" );
    assert( pointerField );

    uint64_t rawPointer;
    mono_field_get_value( monoObject, pointerField, &rawPointer );

    return reinterpret_cast< ScriptedClass* >( rawPointer );
  }

  struct Binary
  {
    Binary() = default;
    Binary( MonoDomain* domain, const char* path )
    {
      assembly = mono_domain_assembly_open( domain, path );
      assert( assembly );
      image = mono_assembly_get_image( assembly );
      assert( image );
    }

    MonoAssembly* assembly = nullptr;
    MonoImage* image = nullptr;
  };

  static std::vector< std::pair< std::string, Binary > > binaries;

  namespace Detail
  {
    MonoImage* GetMainMonoImage()
    {
      return binaries.front().second.image;
    }

    void PrintException( MonoObject* e )
    {
      if ( !e )
        return;

      auto monoClass = mono_object_get_class( e ); EASYMONO_ASSERT( monoClass );
      auto monoDesc = mono_method_desc_new( ":ToString()", false ); EASYMONO_ASSERT( monoDesc );
      auto monoFunc = mono_method_desc_search_in_class( monoDesc, monoClass ); EASYMONO_ASSERT( monoFunc );
      mono_method_desc_free( monoDesc );

      auto monoString = reinterpret_cast<MonoString*>( mono_runtime_invoke( monoFunc, nullptr, nullptr, nullptr ) );
      EASYMONO_PRINT_EXCEPTION( mono_string_chars( monoString ) );
    }

    void PrintException( MonoException* e )
    {
      PrintException( reinterpret_cast< MonoObject* >( e ) );
    }

    static void __stdcall NAddRef( uint64_t nativePtr, MonoObject* monoObject )
    {
      assert( nativePtr );
      assert( monoObject );

      auto nativeThis = reinterpret_cast<ScriptedClass*>( nativePtr );
      nativeThis->SetMonoObject( monoObject );
    }
    static void __stdcall NRelease( uint64_t nativePtr )
    {
      assert( nativePtr );
      reinterpret_cast<ScriptedClass*>( nativePtr )->SetMonoObject( nullptr );
    }
  }

  static MonoDomain* monoDomain = nullptr;

  void Initialize( bool allowDebugger
                 , const char* monoAssemblyDirectory
                 , const char* monoConfigDirectory
                 , const char** binariesToLoad
                 , size_t binaryCount )
  {
    if ( allowDebugger )
    {
      const char* options[] = { "--soft-breakpoints", "--debugger-agent=transport=dt_socket,server=y,address=127.0.0.1:55555" };
      mono_jit_parse_options( _countof( options ), (char**)options );
      mono_debug_init( MONO_DEBUG_FORMAT_MONO );
    }

    mono_set_dirs( monoAssemblyDirectory, monoConfigDirectory );

    monoDomain = mono_jit_init( "scripting" );

    if ( allowDebugger )
      mono_debug_domain_create( monoDomain );

    for ( size_t binaryIndex = 0; binaryIndex < binaryCount; ++binaryIndex )
      binaries.emplace_back( binariesToLoad[ binaryIndex ], Binary( monoDomain, binariesToLoad[ binaryIndex ] ) );

    mono_add_internal_call( "EasyMono.NativeReference::AddRef", Detail::NAddRef );
    mono_add_internal_call( "EasyMono.NativeReference::Release", Detail::NRelease );
  }

  void Teardown()
  {
    if ( monoDomain )
      mono_jit_cleanup( monoDomain );

    monoDomain = nullptr;
  }

  void GarbageCollect( bool fast )
  {
    mono_gc_collect( fast ? 0 : mono_gc_max_generation() );
  }
}
#endif