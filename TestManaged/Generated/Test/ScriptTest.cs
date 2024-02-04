
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Test
{
  public sealed class ScriptTest : EasyMono.NativeReference
  {

    [StructLayout(LayoutKind.Sequential)]
    public struct LocalStruct
    {
      public int thisValue;
      public int thatValue;

    }


    public enum LocalEnum : int
    {
      One = 1,
      Two = 2,
      Four = 4,

    }

    ScriptTest( ulong thiz ) : base( thiz )
    {
    }

    public ScriptTest( int a, string? s, ref readonly System.Numerics.Vector3 v ) : base( Ctor( a, s, in v ) )
    {
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    extern static UInt64 Ctor( int a, string? s, ref readonly System.Numerics.Vector3 v );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetValueByLocalEnum( Test.ScriptTest.LocalEnum e );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetValueByGlobalEnum( Test.Enum.Enum2 e );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetByGlobalStruct( ref readonly Test.GlobalStruct s );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern Test.GlobalStruct GetGlobalStruct(  );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern void SetByLocalStruct( ref readonly Test.ScriptTest.LocalStruct s );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern Test.ScriptTest.LocalStruct GetLocalStruct(  );

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
    public extern string? ConcatString( string? other );

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
    public extern void MultiplyAdd( int a, int b );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern int MultiplyAddAndReturn( int a, int b );

    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern System.Numerics.Vector2 GetVector2(  );

  }
}