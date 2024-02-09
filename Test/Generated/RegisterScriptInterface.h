
#include <mono/metadata/loader.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

#include <memory>

#include "C:\Users\laszl\source\repos\EasyMono\Test/ScriptTest.h"

namespace EasyMono
{
  ScriptedClass* LoadNativePointer( MonoObject* monoObject );

  namespace Detail
  {
    MonoImage* GetMainMonoImage();

    struct GCHolder
    {
      uint32_t handle = 0;
      GCHolder( uint32_t handle ) : handle( handle ) {}
      ~GCHolder() { if ( handle ) mono_gchandle_free( handle ); }
      GCHolder( const GCHolder& ) = delete;
      GCHolder& operator=( const GCHolder& ) = delete;
      GCHolder( GCHolder&& other ) { handle = other.handle; other.handle = 0; }
      GCHolder& operator=( GCHolder&& other ) { handle = other.handle; other.handle = 0; }
    };

    using GCHolderPtr = std::shared_ptr< GCHolder >;

    static GCHolderPtr Hold( MonoObject* monoObject )
    {
      return std::make_shared< GCHolder >( mono_gchandle_new( monoObject, false ) );
    }

    /* Some explanation. When a function is returning with a structure by value, the structure itself matters
     * in how the function is called on the assembly level. In Mono, all structures are PODs, and therefore
     * if their size is less or equal to 8 bytes, they will be passed differently as opposed to being larger.
     *
     * Our structures are different, as they are based on EasyMono::ScriptedStruct, therefore they are passed
     * differently (basically as if they are larger than 8 bytes). So to match how Mono will call the wrapper
     * functions, for types with size less or equal to 8 bytes, we use uint64_t as a return type, and cast our
     * type to that. Mono will interpret the data correctly.
     */

    template<size_t A, size_t B>
    struct is_less_equal
    {
      static constexpr bool value = A <= B;
    };

    template<size_t A, size_t B>
    struct is_greater
    {
      static constexpr bool value = A > B;
    };

    template< typename T, typename Enable = void >
    struct interop_struct;

    template< typename T >
    struct interop_struct< T, std::enable_if_t< std::is_pointer_v< T > || std::is_reference_v< T > > >
    {
      using type = T;
      static T process( T v ) { return v; }
    };

    template< typename T >
    struct interop_struct< T, std::enable_if_t< !std::is_pointer_v< T > && !std::is_reference_v< T > && is_less_equal< sizeof( T ), 8 >::value > >
    {
      using type = uint64_t;
      static uint64_t process( const T& v ) { return *reinterpret_cast<const uint64_t*>( &v ); }
    };

    template< typename T >
    struct interop_struct< T, std::enable_if_t< !std::is_pointer_v< T > && !std::is_reference_v< T > && is_greater< sizeof( T ), 8 >::value > >
    {
      using type = T;
      static const T& process( const T& v ) { return v; }
    };
  }

  extern thread_local bool creatingFromCS;
}

template< typename T, typename Enable = void >
using IS = EasyMono::Detail::interop_struct< T, Enable >;

void RegisterScriptInterface()
{

  struct Test__Test__ScriptTest
  {

    static uint64_t __stdcall ScriptTest( int32_t a, MonoString* s, const DirectX::XMFLOAT3& v )
    {
      EasyMono::creatingFromCS = true;

      auto thiz = reinterpret_cast< uint64_t >( new Test::ScriptTest( a, s ? mono_string_chars( s ) : nullptr, v ) );
    
      EasyMono::creatingFromCS = false;
      return thiz;
    }

    static void __stdcall SetCallback( MonoObject* thiz, MonoObject* callback )
    {
      auto callback__wrapper = [gchandle = EasyMono::Detail::Hold( callback )]( int32_t a, const wchar_t* s, const Test::Struct1::LargestStruct& l, const Test::ScriptTest& c, Test::Enum::Enum2 e )
      {
        auto monoDelegateObject = mono_gchandle_get_target( gchandle->handle ); assert( monoDelegateObject );

        const void* delegate_args[] =
        {
          &a,
          s ? mono_string_from_utf16( (mono_unichar2*)s ) : nullptr,
          &l,
          c.GetOrCreateMonoObject(),
          &e,
        };

        MonoObject* monoException = nullptr;
        MonoObject* delegate_return = mono_runtime_delegate_invoke( monoDelegateObject, (void**)&delegate_args, &monoException );
        if ( monoException )
        {
          EasyMono::Detail::PrintException( monoException );
          assert( false );
        }
        return mono_string_chars( (MonoString*)delegate_return );
      };

      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetCallback( std::move( callback__wrapper ) ) );
    }

    static MonoString* __stdcall CallCallback( MonoObject* thiz, int32_t a, MonoString* s, const Test::Struct1::LargestStruct& l, MonoObject* c, Test::Enum::Enum2 e )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      auto monoRetStr = ( nativeThis->CallCallback( a, s ? mono_string_chars( s ) : nullptr, l, *reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( c ) ), e ) );
      return monoRetStr ? mono_string_new_utf16( mono_domain_get(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;
    }

    static void __stdcall SetValueByLocalEnum( MonoObject* thiz, Test::ScriptTest::LocalEnum e )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetValueByLocalEnum( e ) );
    }

    static void __stdcall SetValueByGlobalEnum( MonoObject* thiz, Test::Enum::Enum2 e )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetValueByGlobalEnum( e ) );
    }

    static void __stdcall SetByGlobalStruct( MonoObject* thiz, const Test::GlobalStruct& s )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetByGlobalStruct( s ) );
    }

    static IS<Test::GlobalStruct>::type __stdcall GetGlobalStruct( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< Test::GlobalStruct>::process( nativeThis->GetGlobalStruct(  ) );
    }

    static void __stdcall SetByLocalStruct( MonoObject* thiz, const Test::ScriptTest::LocalStruct& s )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetByLocalStruct( s ) );
    }

    static IS<Test::ScriptTest::LocalStruct>::type __stdcall GetLocalStruct( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< Test::ScriptTest::LocalStruct>::process( nativeThis->GetLocalStruct(  ) );
    }

    static int32_t __stdcall GetValue( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->GetValue(  ) );
    }

    static void __stdcall SetValue( MonoObject* thiz, int32_t v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetValue( v ) );
    }

    static MonoString* __stdcall GetString( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      auto monoRetStr = ( nativeThis->GetString(  ) );
      return monoRetStr ? mono_string_new_utf16( mono_domain_get(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;
    }

    static void __stdcall SetString( MonoObject* thiz, MonoString* v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetString( v ? mono_string_chars( v ) : nullptr ) );
    }

    static IS<const DirectX::XMFLOAT3&>::type __stdcall GetVector( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT3>::process( nativeThis->GetVector(  ) );
    }

    static void __stdcall SetVector( MonoObject* thiz, const DirectX::XMFLOAT3& v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetVector( v ) );
    }

    static IS<DirectX::XMFLOAT4>::type __stdcall GetZeroVector( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT4>::process( nativeThis->GetZeroVector(  ) );
    }

    static void __stdcall SetWhatever( MonoObject* thiz, int32_t v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->SetWhatever( v ) );
    }

    static MonoString* __stdcall ConcatString( MonoObject* thiz, MonoString* other )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      auto monoRetStr = ( nativeThis->ConcatString( other ? mono_string_chars( other ) : nullptr ) );
      return monoRetStr ? mono_string_new_utf16( mono_domain_get(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;
    }

    static int32_t __stdcall ClassPtr( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->ClassPtr( reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) ) );
    }

    static int32_t __stdcall ClassRef( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->ClassRef( *reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) ) );
    }

    static int32_t __stdcall ClassCref( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->ClassCref( *reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) ) );
    }

    static MonoObject* __stdcall ClassPass( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->ClassPass( reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) )->GetOrCreateMonoObject() );
    }

    static IS<DirectX::XMFLOAT4X4>::type __stdcall TransposeMatrixByVal( MonoObject* thiz, DirectX::XMFLOAT4X4 m )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT4X4>::process( nativeThis->TransposeMatrixByVal( m ) );
    }

    static IS<DirectX::XMFLOAT4X4>::type __stdcall TransposeMatrixByRef( MonoObject* thiz, const DirectX::XMFLOAT4X4& m )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT4X4>::process( nativeThis->TransposeMatrixByRef( m ) );
    }

    static IS<DirectX::XMFLOAT3>::type __stdcall AddVectorsByVal( MonoObject* thiz, DirectX::XMFLOAT3 a, DirectX::XMFLOAT3 b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT3>::process( nativeThis->AddVectorsByVal( a, b ) );
    }

    static IS<DirectX::XMFLOAT3>::type __stdcall AddVectorsByRef( MonoObject* thiz, const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT3>::process( nativeThis->AddVectorsByRef( a, b ) );
    }

    static void __stdcall DoubleIt( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->DoubleIt(  ) );
    }

    static int32_t __stdcall DoubleItAndReturn( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->DoubleItAndReturn(  ) );
    }

    static void __stdcall MultiplyAdd( MonoObject* thiz, int32_t a, int32_t b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->MultiplyAdd( a, b ) );
    }

    static int32_t __stdcall MultiplyAddAndReturn( MonoObject* thiz, int32_t a, int32_t b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return ( nativeThis->MultiplyAddAndReturn( a, b ) );
    }

    static IS<DirectX::XMFLOAT2>::type __stdcall GetVector2( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return IS< DirectX::XMFLOAT2>::process( nativeThis->GetVector2(  ) );
    }

  };

  mono_add_internal_call( "Test.ScriptTest::Ctor", Test__Test__ScriptTest::ScriptTest );
  mono_add_internal_call( "Test.ScriptTest::SetCallback", Test__Test__ScriptTest::SetCallback );
  mono_add_internal_call( "Test.ScriptTest::CallCallback", Test__Test__ScriptTest::CallCallback );
  mono_add_internal_call( "Test.ScriptTest::SetValueByLocalEnum", Test__Test__ScriptTest::SetValueByLocalEnum );
  mono_add_internal_call( "Test.ScriptTest::SetValueByGlobalEnum", Test__Test__ScriptTest::SetValueByGlobalEnum );
  mono_add_internal_call( "Test.ScriptTest::SetByGlobalStruct", Test__Test__ScriptTest::SetByGlobalStruct );
  mono_add_internal_call( "Test.ScriptTest::GetGlobalStruct", Test__Test__ScriptTest::GetGlobalStruct );
  mono_add_internal_call( "Test.ScriptTest::SetByLocalStruct", Test__Test__ScriptTest::SetByLocalStruct );
  mono_add_internal_call( "Test.ScriptTest::GetLocalStruct", Test__Test__ScriptTest::GetLocalStruct );
  mono_add_internal_call( "Test.ScriptTest::GetValue", Test__Test__ScriptTest::GetValue );
  mono_add_internal_call( "Test.ScriptTest::SetValue", Test__Test__ScriptTest::SetValue );
  mono_add_internal_call( "Test.ScriptTest::GetString", Test__Test__ScriptTest::GetString );
  mono_add_internal_call( "Test.ScriptTest::SetString", Test__Test__ScriptTest::SetString );
  mono_add_internal_call( "Test.ScriptTest::GetVector", Test__Test__ScriptTest::GetVector );
  mono_add_internal_call( "Test.ScriptTest::SetVector", Test__Test__ScriptTest::SetVector );
  mono_add_internal_call( "Test.ScriptTest::GetZeroVector", Test__Test__ScriptTest::GetZeroVector );
  mono_add_internal_call( "Test.ScriptTest::SetWhatever", Test__Test__ScriptTest::SetWhatever );
  mono_add_internal_call( "Test.ScriptTest::ConcatString", Test__Test__ScriptTest::ConcatString );
  mono_add_internal_call( "Test.ScriptTest::ClassPtr", Test__Test__ScriptTest::ClassPtr );
  mono_add_internal_call( "Test.ScriptTest::ClassRef", Test__Test__ScriptTest::ClassRef );
  mono_add_internal_call( "Test.ScriptTest::ClassCref", Test__Test__ScriptTest::ClassCref );
  mono_add_internal_call( "Test.ScriptTest::ClassPass", Test__Test__ScriptTest::ClassPass );
  mono_add_internal_call( "Test.ScriptTest::TransposeMatrixByVal", Test__Test__ScriptTest::TransposeMatrixByVal );
  mono_add_internal_call( "Test.ScriptTest::TransposeMatrixByRef", Test__Test__ScriptTest::TransposeMatrixByRef );
  mono_add_internal_call( "Test.ScriptTest::AddVectorsByVal", Test__Test__ScriptTest::AddVectorsByVal );
  mono_add_internal_call( "Test.ScriptTest::AddVectorsByRef", Test__Test__ScriptTest::AddVectorsByRef );
  mono_add_internal_call( "Test.ScriptTest::DoubleIt", Test__Test__ScriptTest::DoubleIt );
  mono_add_internal_call( "Test.ScriptTest::DoubleItAndReturn", Test__Test__ScriptTest::DoubleItAndReturn );
  mono_add_internal_call( "Test.ScriptTest::MultiplyAdd", Test__Test__ScriptTest::MultiplyAdd );
  mono_add_internal_call( "Test.ScriptTest::MultiplyAddAndReturn", Test__Test__ScriptTest::MultiplyAddAndReturn );
  mono_add_internal_call( "Test.ScriptTest::GetVector2", Test__Test__ScriptTest::GetVector2 );


}

MonoClass* Test::ScriptTest::GetMonoClass()
{
  static MonoClass* monoClass = nullptr;
  if ( !monoClass )
  {
    monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "Test", "ScriptTest" );
    assert( monoClass );
  }
  
  return monoClass;
}

