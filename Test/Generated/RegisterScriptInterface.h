
#include <mono/metadata/loader.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

#include "C:\Users\laszl\source\repos\EasyMono\Test/ScriptTest.h"

namespace EasyMono
{
  ScriptedClass* LoadNativePointer( MonoObject* monoObject );

  namespace Detail
  {
    MonoImage* GetMainMonoImage();
  }

  extern thread_local bool creatingFromCS;
}

void RegisterScriptInterface()
{

  struct Test__ScriptTest
  {

    static uint64_t __stdcall ScriptTest( int a, MonoString* s, const ::DirectX::XMFLOAT3& v )
    {
      EasyMono::creatingFromCS = true;

      auto thiz = reinterpret_cast< uint64_t >( new Test::ScriptTest( a, s ? mono_string_chars( s ) : nullptr, v ) );
    
      EasyMono::creatingFromCS = false;
      return thiz;
    }

    static int __stdcall GetValue( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->GetValue(  );

    }

    static void __stdcall SetValue( MonoObject* thiz, int v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->SetValue( v );

    }

    static MonoString* __stdcall GetString( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      auto monoRetStr = nativeThis->GetString(  );
      return monoRetStr ? mono_string_new_utf16( mono_get_root_domain(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;
    }

    static void __stdcall SetString( MonoObject* thiz, MonoString* v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->SetString( v ? mono_string_chars( v ) : nullptr );

    }

    static const ::DirectX::XMFLOAT3& __stdcall GetVector( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->GetVector(  );

    }

    static void __stdcall SetVector( MonoObject* thiz, const ::DirectX::XMFLOAT3& v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->SetVector( v );

    }

    static ::DirectX::XMFLOAT4 __stdcall GetZeroVector( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->GetZeroVector(  );

    }

    static void __stdcall SetWhatever( MonoObject* thiz, int v )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->SetWhatever( v );

    }

    static MonoString* __stdcall ConcatCString( MonoObject* thiz, MonoString* other )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      auto monoRetStr = nativeThis->ConcatCString( other ? mono_string_chars( other ) : nullptr );
      return monoRetStr ? mono_string_new_utf16( mono_get_root_domain(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;
    }

    static int __stdcall ClassPtr( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->ClassPtr( reinterpret_cast< ::Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) );

    }

    static int __stdcall ClassRef( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->ClassRef( *reinterpret_cast< ::Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) );

    }

    static int __stdcall ClassCref( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->ClassCref( *reinterpret_cast< ::Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) );

    }

    static MonoObject* __stdcall ClassPass( MonoObject* thiz, MonoObject* p )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->ClassPass( reinterpret_cast< ::Test::ScriptTest* >( EasyMono::LoadNativePointer( p ) ) )->GetOrCreateMonoObject();

    }

    static ::DirectX::XMFLOAT4X4 __stdcall TransposeMatrixByVal( MonoObject* thiz, ::DirectX::XMFLOAT4X4 m )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->TransposeMatrixByVal( m );

    }

    static ::DirectX::XMFLOAT4X4 __stdcall TransposeMatrixByRef( MonoObject* thiz, const ::DirectX::XMFLOAT4X4& m )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->TransposeMatrixByRef( m );

    }

    static ::DirectX::XMFLOAT3 __stdcall AddVectorsByVal( MonoObject* thiz, ::DirectX::XMFLOAT3 a, ::DirectX::XMFLOAT3 b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->AddVectorsByVal( a, b );

    }

    static ::DirectX::XMFLOAT3 __stdcall AddVectorsByRef( MonoObject* thiz, const ::DirectX::XMFLOAT3& a, const ::DirectX::XMFLOAT3& b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->AddVectorsByRef( a, b );

    }

    static void __stdcall DoubleIt( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->DoubleIt(  );

    }

    static int __stdcall DoubleItAndReturn( MonoObject* thiz )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->DoubleItAndReturn(  );

    }

    static void __stdcall MultiplaAdd( MonoObject* thiz, int a, int b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->MultiplaAdd( a, b );

    }

    static int __stdcall MultiplaAddAndReturn( MonoObject* thiz, int a, int b )
    {
      auto nativeThis = reinterpret_cast< Test::ScriptTest* >( EasyMono::LoadNativePointer( thiz ) );
      return nativeThis->MultiplaAddAndReturn( a, b );

    }

  };

  mono_add_internal_call( "Test.ScriptTest::Ctor", Test__ScriptTest::ScriptTest );
  mono_add_internal_call( "Test.ScriptTest::GetValue", Test__ScriptTest::GetValue );
  mono_add_internal_call( "Test.ScriptTest::SetValue", Test__ScriptTest::SetValue );
  mono_add_internal_call( "Test.ScriptTest::GetString", Test__ScriptTest::GetString );
  mono_add_internal_call( "Test.ScriptTest::SetString", Test__ScriptTest::SetString );
  mono_add_internal_call( "Test.ScriptTest::GetVector", Test__ScriptTest::GetVector );
  mono_add_internal_call( "Test.ScriptTest::SetVector", Test__ScriptTest::SetVector );
  mono_add_internal_call( "Test.ScriptTest::GetZeroVector", Test__ScriptTest::GetZeroVector );
  mono_add_internal_call( "Test.ScriptTest::SetWhatever", Test__ScriptTest::SetWhatever );
  mono_add_internal_call( "Test.ScriptTest::ConcatCString", Test__ScriptTest::ConcatCString );
  mono_add_internal_call( "Test.ScriptTest::ClassPtr", Test__ScriptTest::ClassPtr );
  mono_add_internal_call( "Test.ScriptTest::ClassRef", Test__ScriptTest::ClassRef );
  mono_add_internal_call( "Test.ScriptTest::ClassCref", Test__ScriptTest::ClassCref );
  mono_add_internal_call( "Test.ScriptTest::ClassPass", Test__ScriptTest::ClassPass );
  mono_add_internal_call( "Test.ScriptTest::TransposeMatrixByVal", Test__ScriptTest::TransposeMatrixByVal );
  mono_add_internal_call( "Test.ScriptTest::TransposeMatrixByRef", Test__ScriptTest::TransposeMatrixByRef );
  mono_add_internal_call( "Test.ScriptTest::AddVectorsByVal", Test__ScriptTest::AddVectorsByVal );
  mono_add_internal_call( "Test.ScriptTest::AddVectorsByRef", Test__ScriptTest::AddVectorsByRef );
  mono_add_internal_call( "Test.ScriptTest::DoubleIt", Test__ScriptTest::DoubleIt );
  mono_add_internal_call( "Test.ScriptTest::DoubleItAndReturn", Test__ScriptTest::DoubleItAndReturn );
  mono_add_internal_call( "Test.ScriptTest::MultiplaAdd", Test__ScriptTest::MultiplaAdd );
  mono_add_internal_call( "Test.ScriptTest::MultiplaAddAndReturn", Test__ScriptTest::MultiplaAddAndReturn );


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

