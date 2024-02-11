#pragma once

#include "ScriptedClassBase.h"
#include "TestStruct.h"

#include <string>
#include <functional>
#include <DirectXMath.h>

using namespace DirectX;

namespace Test
{
  class ScriptTest;

  using GlobalDelegate = std::function< const wchar_t*( int a, const wchar_t* s, const Struct1::LargestStruct& l, const ScriptTest& c, Test::Enum::Enum2 e ) >;

  struct GlobalStruct : EasyMono::ScriptedStruct
  {
    int thisValue;
    int thatValue;
  };

  class ScriptTest final : public ScriptedClassBase< ScriptTest >
  {
    static MonoClass* GetMonoClass();

  managed_export:
    struct LocalStruct : EasyMono::ScriptedStruct
    {
      int thisValue;
      int thatValue;
    };

    enum class LocalEnum
    {
      One = 1,
      Two = 2,
      Four = 4,
    };

    ScriptTest( int a, const wchar_t* s, const XMFLOAT3& v )
      : ScriptedClassBase( GetMonoClass() )
      , value( a )
      , str( s )
      , vec( v )
    {
    }

    void SetCallback( GlobalDelegate&& callback )
    {
      globalDelegate = callback;
    }

    const wchar_t* CallCallback( int a, const wchar_t* s, const Struct1::LargestStruct& l, const ScriptTest& c, Test::Enum::Enum2 e )
    {
      if ( globalDelegate )
        return globalDelegate( a, s, l, c, e );
      return nullptr;
    }

    void SetValueByLocalEnum( LocalEnum e )
    {
      value = (int)e;
    }

    void SetValueByGlobalEnum( Test::Enum::Enum2 e )
    {
      value = (int)e;
    }

    void SetByGlobalStruct( const GlobalStruct& s )
    {
      value = s.thisValue + s.thatValue;
    }

    GlobalStruct GetGlobalStruct() const
    {
      return GlobalStruct{ .thisValue = value, .thatValue = 1 };
    }

    void SetByLocalStruct( const LocalStruct& s )
    {
      value = s.thisValue + s.thatValue;
    }

    LocalStruct GetLocalStruct() const
    {
      return LocalStruct{ .thisValue = value, .thatValue = 2 };
    }

    int GetValue() const
    {
      return value;
    }

    void SetValue(int v)
    {
      value = v;
    }

    const wchar_t* GetString() const
    {
      return str.data();
    }

    void SetString( const wchar_t* v )
    {
      str = v;
    }

    const XMFLOAT3& GetVector() const
    {
      return vec;
    }

    void SetVector( const XMFLOAT3& v )
    {
      vec = v;
    }

    XMFLOAT4 GetZeroVector() const
    {
      return XMFLOAT4( 0, 0, 0, 0 );
    }

    void SetWhatever( int v )
    {
      value = v;
    }

    const wchar_t* ConcatString( const wchar_t* other )
    {
      if ( other )
      {
        str += other;
        return str.data();
      }

      return nullptr;
    }

    int ClassPtr( ScriptTest* p )
    {
      return p->DoubleItAndReturn();
    }

    int ClassRef( ScriptTest& p )
    {
      return p.DoubleItAndReturn();
    }

    int ClassCref( const ScriptTest& p )
    {
      return p.value;
    }

    ScriptTest* ClassPass( ScriptTest* p )
    {
      return p;
    }

    XMFLOAT4X4 TransposeMatrixByVal( XMFLOAT4X4 m )
    {
      XMFLOAT4X4 tm;
      XMStoreFloat4x4( &tm, XMMatrixTranspose( XMLoadFloat4x4( &m ) ) );
      return tm;
    }

    XMFLOAT4X4 TransposeMatrixByRef( const XMFLOAT4X4& m )
    {
      XMFLOAT4X4 tm;
      XMStoreFloat4x4( &tm, XMMatrixTranspose( XMLoadFloat4x4( &m ) ) );
      return tm;
    }

    XMFLOAT3 AddVectorsByVal( XMFLOAT3 a, XMFLOAT3 b )
    {
      return XMFLOAT3( a.x + b.x, a.y + b.y, a.z + b.z );
    }

    XMFLOAT3 AddVectorsByRef( const XMFLOAT3& a, const XMFLOAT3& b )
    {
      return XMFLOAT3( a.x + b.x, a.y + b.y, a.z + b.z );
    }

    void DoubleIt()
    {
      value *= 2;
    }

    int DoubleItAndReturn()
    {
      value *= 2;
      return value;
    }

    void MultiplyAdd( int a, int b )
    {
      value = value * a + b;
    }

    int MultiplyAddAndReturn( int a, int b )
    {
      value = value * a + b;
      return value;
    }

    XMFLOAT2 GetVector2() const
    {
      return XMFLOAT2( 3, 4 );
    }

    void PrintArrayOfInt( EasyMono::Array< int > array )
    {
      std::wcout << L"PrintArrayOfInt called with:";
      for ( auto value : array )
        std::wcout << L" " << std::to_wstring( value );
      std::wcout << std::endl;
    }

    void PrintArrayOfString( EasyMono::Array< const wchar_t* > array )
    {
      std::wcout << L"PrintArrayOfString called with:";
      for ( auto value : array )
        std::wcout << L" " << value;
      std::wcout << std::endl;
    }

    void PrintArrayOfObjects( EasyMono::Array< ScriptTest* > array )
    {
      std::wcout << L"PrintArrayOfObjects called with:";
      for ( auto value : array )
        std::wcout << L" " << ( value ? value->GetString() : L"null" );
      std::wcout << std::endl;
    }

    void PrintArrayOfStructs( EasyMono::Array< Struct1::LargestStruct > array )
    {
      std::wcout << L"PrintArrayOfStructs called with:";
      for ( auto value : array )
        std::wcout << L" " << std::to_wstring( value.a ) << ", " << std::to_wstring( value.b ) << ", " << std::to_wstring( value.c ) << ", " << std::to_wstring( value.d ) << " |";
      std::wcout << std::endl;
    }

    void PrintListOfInt( EasyMono::List< int > list )
    {
      std::wcout << L"PrintListOfInt called with:";
      for ( auto value : list )
        std::wcout << L" " << std::to_wstring( value );
      std::wcout << std::endl;
    }

    void PrintListOfString( EasyMono::List< const wchar_t* > list )
    {
      std::wcout << L"PrintListOfString called with:";
      for ( auto value : list )
        std::wcout << L" " << value;
      std::wcout << std::endl;
    }

    void PrintListOfObjects( EasyMono::List< ScriptTest* > list )
    {
      std::wcout << L"PrintListOfObjects called with:";
      for ( auto value : list )
        std::wcout << L" " << ( value ? value->GetString() : L"null" );
      std::wcout << std::endl;
    }

    void PrintListOfStructs( EasyMono::List< Struct1::LargestStruct > list )
    {
      std::wcout << L"PrintListOfStructs called with:";
      for ( auto value : list )
        std::wcout << L" " << std::to_wstring( value.a ) << ", " << std::to_wstring( value.b ) << ", " << std::to_wstring( value.c ) << ", " << std::to_wstring( value.d ) << " |";
      std::wcout << std::endl;
    }

    void TestDictionaryValVal( EasyMono::Dictionary< int, int > dic )
    {
      std::wstring null( L"null" );
      int value1, value2, value3, value4;

      std::wcout << L"TestDictionaryValVal called with size " << std::to_wstring( dic.size() ) << std::endl;

      bool has1 = dic.tryGetValue( 1, value1 );
      bool has2 = dic.tryGetValue( 2, value2 );
      bool has3 = dic.tryGetValue( 3, value3 );
      bool has4 = dic.tryGetValue( 4, value4 );
      std::wcout << L"TestDictionaryValVal 1 -> " << ( has1 ? std::to_wstring( value1 ) : null ) << std::endl;
      std::wcout << L"TestDictionaryValVal 2 -> " << ( has2 ? std::to_wstring( value2 ) : null ) << std::endl;
      std::wcout << L"TestDictionaryValVal 3 -> " << ( has3 ? std::to_wstring( value3 ) : null ) << std::endl;
      std::wcout << L"TestDictionaryValVal 4 -> " << ( has4 ? std::to_wstring( value4 ) : null ) << std::endl;
    }

    void TestDictionaryValString( EasyMono::Dictionary< int, const wchar_t* > dic )
    {
      const wchar_t *value1, *value2, *value3, *value4;

      std::wcout << L"TestDictionaryValString called with size " << std::to_wstring( dic.size() ) << std::endl;

      bool has1 = dic.tryGetValue( 1, value1 );
      bool has2 = dic.tryGetValue( 2, value2 );
      bool has3 = dic.tryGetValue( 3, value3 );
      bool has4 = dic.tryGetValue( 4, value4 );
      std::wcout << L"TestDictionaryValString 1 -> " << ( has1 ? value1 : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryValString 2 -> " << ( has2 ? value2 : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryValString 3 -> " << ( has3 ? value3 : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryValString 4 -> " << ( has4 ? value4 : L"null" ) << std::endl;
    }

    void TestDictionaryValSC( EasyMono::Dictionary< int, ScriptTest* > dic )
    {
      ScriptTest *value1, *value2, *value3, *value4;

      std::wcout << L"TestDictionaryValSC called with size " << std::to_wstring( dic.size() ) << std::endl;

      bool has1 = dic.tryGetValue( 1, value1 );
      bool has2 = dic.tryGetValue( 2, value2 );
      bool has3 = dic.tryGetValue( 3, value3 );
      bool has4 = dic.tryGetValue( 4, value4 );
      std::wcout << L"TestDictionaryValSC 1 -> " << ( ( has1 && value1 ) ? value1->GetString() : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryValSC 2 -> " << ( ( has2 && value2 ) ? value2->GetString() : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryValSC 3 -> " << ( ( has3 && value3 ) ? value3->GetString() : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryValSC 4 -> " << ( ( has4 && value4 ) ? value4->GetString() : L"null" ) << std::endl;
    }

    void TestDictionaryStringString( EasyMono::Dictionary< const wchar_t*, const wchar_t* > dic )
    {
      const wchar_t *value1, *value2, *value3, *value4;

      std::wcout << L"TestDictionaryStringString called with size " << std::to_wstring( dic.size() ) << std::endl;

      bool has1 = dic.tryGetValue( L"foo", value1 );
      bool has2 = dic.tryGetValue( L"bar", value2 );
      bool has3 = dic.tryGetValue( L"baz", value3 );
      std::wcout << L"TestDictionaryStringString foo -> " << ( ( has1 && value1 ) ? value1 : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryStringString bar -> " << ( ( has2 && value2 ) ? value2 : L"null" ) << std::endl;
      std::wcout << L"TestDictionaryStringString baz -> " << ( ( has3 && value3 ) ? value3 : L"null" ) << std::endl;
    }

    void TestDictionarySCString( EasyMono::Dictionary< ScriptTest*, const wchar_t* > dic )
    {
      const wchar_t* value;

      std::wcout << L"TestDictionarySCString called with size " << std::to_wstring( dic.size() ) << std::endl;

      bool has = dic.tryGetValue( this, value );
      std::wcout << L"TestDictionarySCString this -> " << ( ( has && value ) ? value : L"null" ) << std::endl;
    }

    static ScriptTest* CreateInstance( int a, const wchar_t* s, const XMFLOAT3& v )
    {
      return new ScriptTest( a, s, v );
    }

  public:
    int GetSqTheValue() const
    {
      return value * value;
    }

  private:
    int GetDoubleTheValue() const
    {
      return value * 2;
    }

    ~ScriptTest()
    {
    }

    int value = 0;
    std::wstring str;
    XMFLOAT3 vec;
    GlobalDelegate globalDelegate;
  };

  class ScriptTestVirtual : public ScriptedClassBase< ScriptTest >
  {
    static MonoClass* GetMonoClass();

  managed_export:
    virtual void RandomVirtualMethod() {}
  };

  class ScriptTestOverride final : public ScriptTestVirtual
  {
    static MonoClass* GetMonoClass();

  managed_export:
    void RandomVirtualMethod() override {}
  };
}