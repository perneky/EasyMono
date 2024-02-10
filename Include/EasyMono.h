#pragma once

#include <stdint.h>
#include <type_traits>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

#ifndef EASY_MONO_PARSER
  #define managed_export public
#else
  #define managed_export struct __ManagedExport__; public
#endif

#ifndef EASYMONO_ASSERT
# include <cassert>
# define EASYMONO_ASSERT assert
#endif

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
    MonoObject* GetOrCreateMonoObject() const;

  protected:
    ScriptedClass( MonoClass* monoClass );
    virtual ~ScriptedClass();

    virtual void OnScriptAttached() = 0;
    virtual void OnScriptDetached() = 0;

  private:
    void SetMonoObject( MonoObject* monoObject );

    MonoClass* monoClass = nullptr;
    
    mutable uint32_t monoObjectHandle = 0;
  };

  namespace Detail
  {
    ScriptedClass* LoadNativePointer( MonoObject* monoObject );
  }

  template< typename T >
  struct strip
  {
    using type = std::remove_const_t< std::remove_pointer_t< T > >;
  };

  template< typename T >
  struct is_string
  {
    static constexpr bool value = std::is_same_v< T, const wchar_t* >;
  };

  template< typename T >
  struct is_scripted
  {
    static constexpr bool value = std::is_base_of_v< ScriptedClass, typename strip< T >::type >;
  };

  struct ArrayBase
  {
    ArrayBase( MonoArray* monoArray ) : monoArray( monoArray ) {}

    size_t length() const
    {
      return monoArray ? mono_array_length( monoArray ) : 0;
    }

    void* elem( size_t index, size_t elemSize ) const
    {
      return monoArray ? mono_array_addr_with_size( monoArray, elemSize, index ) : nullptr;
    }

    MonoArray* monoArray;
  };

  template< typename T, typename Enable = void >
  struct Array;

  template< typename T >
  struct Array< T, std::enable_if_t< std::is_pod_v< T > && !is_string< T >::value && !is_scripted< T >::value > > sealed : private ArrayBase
  {
    using iterator = T*;
    using const_iterator = const T*;

    Array( MonoArray* monoArray ) : ArrayBase( monoArray ) {}

    iterator begin() { return static_cast<T*>( elem( 0, sizeof( T ) ) ); }
    iterator end() { return static_cast<T*>( elem( length(), sizeof( T ) ) ); }
    const_iterator begin() const { return static_cast<T*>( elem( 0 ), sizeof( T ) ); }
    const_iterator end() const { return static_cast<T*>( elem( size(), sizeof( T ) ) ); }

    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }

    T& operator[] ( size_t index ) { EASYMONO_ASSERT( index < length() ); return at( index ); }
    const T& operator[] ( size_t index ) const { EASYMONO_ASSERT( index < length() ); return at( index ); }

    T& at( size_t index ) { return *static_cast<T*>( elem( index, sizeof( T ) ) ); }
    const T& at( size_t index ) const { return *static_cast<T*>( elem( index, sizeof( T ) ) ); }
  };

  template< typename T >
  struct Array< T, std::enable_if_t< is_string< T >::value > > sealed : private ArrayBase
  {
    struct iterator_impl
    {
      mutable size_t index;
      Array& array;

      iterator_impl( size_t index, Array& array ) : index( index ), array( array ) {}

      T operator * () const { return array[ index ]; }

      bool operator == ( const iterator_impl& other ) const { return index == other.index; }
      bool operator != ( const iterator_impl& other ) const { return index != other.index; }

      iterator_impl& operator ++ () { ++index; return *this; }
      iterator_impl& operator ++ (int) { ++index; return *this; }
    };

    using iterator = iterator_impl;
    using const_iterator = const iterator_impl;

    Array( MonoArray* monoArray ) : ArrayBase( monoArray ) {}

    iterator begin() { return iterator( 0, *this ); }
    iterator end() { return iterator( length(), *this); }
    const_iterator begin() const { return iterator( 0, *this ); }
    const_iterator end() const { return iterator( length(), *this ); }

    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }

    T operator[] ( size_t index ) { EASYMONO_ASSERT( index < length() ); return at( index ); }
    const T operator[] ( size_t index ) const { EASYMONO_ASSERT( index < length() ); return at( index ); }

    T at( size_t index ) { return mono_string_chars( *(MonoString**)elem( index, sizeof( MonoString* ) ) ); }
    const T at( size_t index ) const { return mono_string_chars( *(MonoString**)elem( index, sizeof( MonoString* ) ) ); }
  };

  template< typename T >
  struct Array< T, std::enable_if_t< is_scripted< T >::value > > sealed : private ArrayBase
  {
    struct iterator_impl
    {
      mutable size_t index;
      Array& array;

      iterator_impl( size_t index, Array& array ) : index( index ), array( array ) {}

      T operator * () const { return array[ index ]; }

      bool operator == ( const iterator_impl& other ) const { return index == other.index; }
      bool operator != ( const iterator_impl& other ) const { return index != other.index; }

      iterator_impl& operator ++ () { ++index; return *this; }
      iterator_impl& operator ++ ( int ) { ++index; return *this; }
    };

    using iterator = iterator_impl;
    using const_iterator = const iterator_impl;

    Array( MonoArray* monoArray ) : ArrayBase( monoArray ) {}

    iterator begin() { return iterator( 0, *this ); }
    iterator end() { return iterator( length(), *this ); }
    const_iterator begin() const { return iterator( 0, *this ); }
    const_iterator end() const { return iterator( length(), *this ); }

    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }

    T operator[] ( size_t index ) { EASYMONO_ASSERT( index < length() ); return at( index ); }
    const T operator[] ( size_t index ) const { EASYMONO_ASSERT( index < length() ); return at( index ); }

    T at( size_t index ) { return reinterpret_cast<T>( EasyMono::Detail::LoadNativePointer( *(MonoObject**)elem( index, sizeof( MonoObject* ) ) ) ); }
    const T at( size_t index ) const { return reinterpret_cast<T>( EasyMono::Detail::LoadNativePointer( *(MonoObject**)elem( index, sizeof( MonoObject* ) ) ) ); }
  };

  struct ListBase
  {
    ListBase( MonoObject* monoObject ) : monoObject( monoObject ) {}

    size_t length() const
    {
      if ( !monoObject )
        return 0;

      auto monoClass = mono_object_get_class( monoObject );
      auto get_Count = mono_class_get_method_from_name( monoClass, "get_Count", 0 );

      return *(size_t*)mono_object_unbox( mono_runtime_invoke( get_Count, monoObject, nullptr, nullptr ) );
    }

    MonoObject* elem( size_t index ) const
    {
      if ( !monoObject )
        return nullptr;

      auto monoClass = mono_object_get_class( monoObject );
      auto get_Item = mono_class_get_method_from_name( monoClass, "get_Item", 1 );

      void* args[] = { &index };
      return monoObject ? mono_runtime_invoke( get_Item, monoObject, args, nullptr ) : nullptr;
    }

    MonoObject* monoObject;
  };

  template< typename T, typename Enable = void >
  struct List;

  template< typename T >
  struct List< T, std::enable_if_t< std::is_pod_v< T > && !is_string< T >::value && !is_scripted< T >::value > > sealed : private ListBase
  {
    struct iterator_impl
    {
      mutable size_t index;
      List& list;

      iterator_impl( size_t index, List& list ) : index( index ), list( list ) {}

      T operator * () const { return list[ index ]; }

      bool operator == ( const iterator_impl& other ) const { return index == other.index; }
      bool operator != ( const iterator_impl& other ) const { return index != other.index; }

      iterator_impl& operator ++ () { ++index; return *this; }
      iterator_impl& operator ++ ( int ) { ++index; return *this; }
    };

    using iterator = iterator_impl;
    using const_iterator = const iterator_impl;

    List( MonoObject* monoObject ) : ListBase( monoObject ) {}

    iterator begin() { return iterator( 0, *this ); }
    iterator end() { return iterator( length(), *this ); }
    const_iterator begin() const { return iterator( 0, *this ); }
    const_iterator end() const { return iterator( length(), *this ); }

    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }

    T& operator[] ( size_t index ) { EASYMONO_ASSERT( index < length() ); return at( index ); }
    const T& operator[] ( size_t index ) const { EASYMONO_ASSERT( index < length() ); return at( index ); }

    T& at( size_t index ) { return *static_cast<T*>( mono_object_unbox( elem( index ) ) ); }
    const T& at( size_t index ) const { return *static_cast<T*>( mono_object_unbox( elem( index ) ) ); }
  };

  template< typename T >
  struct List< T, std::enable_if_t< is_string< T >::value > > sealed : private ListBase
  {
    struct iterator_impl
    {
      mutable size_t index;
      List& list;

      iterator_impl( size_t index, List& list ) : index( index ), list( list ) {}

      T operator * () const { return list[ index ]; }

      bool operator == ( const iterator_impl& other ) const { return index == other.index; }
      bool operator != ( const iterator_impl& other ) const { return index != other.index; }

      iterator_impl& operator ++ () { ++index; return *this; }
      iterator_impl& operator ++ ( int ) { ++index; return *this; }
    };

    using iterator = iterator_impl;
    using const_iterator = const iterator_impl;

    List( MonoObject* monoObject ) : ListBase( monoObject ) {}

    iterator begin() { return iterator( 0, *this ); }
    iterator end() { return iterator( length(), *this ); }
    const_iterator begin() const { return iterator( 0, *this ); }
    const_iterator end() const { return iterator( length(), *this ); }

    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }

    T operator[] ( size_t index ) { EASYMONO_ASSERT( index < length() ); return at( index ); }
    const T operator[] ( size_t index ) const { EASYMONO_ASSERT( index < length() ); return at( index ); }

    T at( size_t index ) { return mono_string_chars( (MonoString*)elem( index ) ); }
    const T at( size_t index ) const { return mono_string_chars( (MonoString*)elem( index ) ); }
  };

  template< typename T >
  struct List< T, std::enable_if_t< is_scripted< T >::value > > sealed : private ListBase
  {
    struct iterator_impl
    {
      mutable size_t index;
      List& list;

      iterator_impl( size_t index, List& list ) : index( index ), list( list ) {}

      T operator * () const { return list[ index ]; }

      bool operator == ( const iterator_impl& other ) const { return index == other.index; }
      bool operator != ( const iterator_impl& other ) const { return index != other.index; }

      iterator_impl& operator ++ () { ++index; return *this; }
      iterator_impl& operator ++ ( int ) { ++index; return *this; }
    };

    using iterator = iterator_impl;
    using const_iterator = const iterator_impl;

    List( MonoObject* monoObject ) : ArrayBase( monoObject ) {}

    iterator begin() { return iterator( 0, *this ); }
    iterator end() { return iterator( length(), *this ); }
    const_iterator begin() const { return iterator( 0, *this ); }
    const_iterator end() const { return iterator( length(), *this ); }

    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }

    T operator[] ( size_t index ) { EASYMONO_ASSERT( index < length() ); return at( index ); }
    const T operator[] ( size_t index ) const { EASYMONO_ASSERT( index < length() ); return at( index ); }

    T at( size_t index ) { return reinterpret_cast<T>( EasyMono::Detail::LoadNativePointer( (MonoObject*)elem( index ) ) ); }
    const T at( size_t index ) const { return reinterpret_cast<T>( EasyMono::Detail::LoadNativePointer( (MonoObject*)elem( index ) ) ); }
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

#ifndef EASYMONO_PRINT_EXCEPTION
# include <iostream>
# define EASYMONO_PRINT_EXCEPTION( e ) ( std::wcerr << e << std::endl )
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

  MonoObject* ScriptedClass::GetOrCreateMonoObject() const
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

      return reinterpret_cast<ScriptedClass*>( rawPointer );
    }

    MonoImage* GetMainMonoImage()
    {
      return binaries.front().second.image;
    }

    void PrintException( MonoObject* e )
    {
      if ( !e )
        return;

      EASYMONO_PRINT_EXCEPTION( mono_class_get_name( mono_object_get_class( e ) ) );

      auto monoExceptionClass = mono_class_from_name( mono_get_corlib(), "System", "Exception" );
      auto monoMessageMethod = mono_class_get_method_from_name( monoExceptionClass, "get_Message", 0 );
      auto monoStackMethod = mono_class_get_method_from_name( monoExceptionClass, "get_StackTrace", 0 );

      auto monoString = reinterpret_cast<MonoString*>( mono_runtime_invoke( monoMessageMethod, e, nullptr, nullptr ) );
      EASYMONO_PRINT_EXCEPTION( mono_string_chars( monoString ) );
      monoString = reinterpret_cast<MonoString*>( mono_runtime_invoke( monoStackMethod, e, nullptr, nullptr ) );
      if ( monoString )
        EASYMONO_PRINT_EXCEPTION( mono_string_chars( monoString ) );
      else
        EASYMONO_PRINT_EXCEPTION( L"No stack available." );
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