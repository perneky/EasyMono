#pragma once

struct Foo0 {};

#include "ScriptedClassBase.h"

struct Foo3 {};

#include <string>
#include <DirectXMath.h>

using namespace DirectX;

namespace Test
{
  class ScriptTest final : public ScriptedClassBase< ScriptTest >
  {
    static MonoClass* GetMonoClass();

  public:
    ScriptTest( int a, const wchar_t* s, const XMFLOAT3& v )
      : ScriptedClassBase( GetMonoClass() )
      , value( a )
      , str( s )
      , vec( v )
    {
    }

    ~ScriptTest()
    {
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

    const wchar_t* ConcatCString( const wchar_t* other )
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

    void MultiplaAdd( int a, int b )
    {
      value = value * a + b;
    }

    int MultiplaAddAndReturn( int a, int b )
    {
      value = value * a + b;
      return value;
    }

  private:
    int value = 0;
    std::wstring str;
    XMFLOAT3 vec;
  };
}