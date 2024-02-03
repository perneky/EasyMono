
using System.Runtime.CompilerServices;

namespace Test
{
  public sealed class ScriptTest : EasyMono.NativeReference
  {
    ScriptTest( ulong thiz ) : base( thiz )
    {
    }

    public int Value
    {
      get => GetValue();
      set => SetValue( value );
    }

    public string? String
    {
      get => GetString();
      set => SetString( value );
    }

    public System.Numerics.Vector3 Vector
    {
      get => GetVector();
      set => SetVector( in value );
    }

    public System.Numerics.Vector4 ZeroVector => GetZeroVector();

    public int Whatever { set => SetWhatever( value ); }


    public ScriptTest( int a, string? s, ref readonly System.Numerics.Vector3 v ) : base( Ctor( a, s, in v ) )
    {
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    extern static UInt64 Ctor( int a, string? s, ref readonly System.Numerics.Vector3 v );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int GetValue(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetValue( int v );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern string? GetString(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetString( string? v );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern ref readonly System.Numerics.Vector3 GetVector(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetVector( ref readonly System.Numerics.Vector3 v );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern System.Numerics.Vector4 GetZeroVector(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetWhatever( int v );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern string? ConcatCString( string? other );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int ClassPtr( Test.ScriptTest? p );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int ClassRef( Test.ScriptTest p );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int ClassCref( Test.ScriptTest p );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern Test.ScriptTest? ClassPass( Test.ScriptTest? p );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern System.Numerics.Matrix4x4 TransposeMatrixByVal( System.Numerics.Matrix4x4 m );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern System.Numerics.Matrix4x4 TransposeMatrixByRef( ref readonly System.Numerics.Matrix4x4 m );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern System.Numerics.Vector3 AddVectorsByVal( System.Numerics.Vector3 a, System.Numerics.Vector3 b );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern System.Numerics.Vector3 AddVectorsByRef( ref readonly System.Numerics.Vector3 a, ref readonly System.Numerics.Vector3 b );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void DoubleIt(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int DoubleItAndReturn(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void MultiplaAdd( int a, int b );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int MultiplaAddAndReturn( int a, int b );

  }
}