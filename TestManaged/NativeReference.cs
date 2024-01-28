using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace EasyMono
{
  public class NativeReference : IEquatable<NativeReference>, IDisposable
  {
    ulong pointer;

    protected NativeReference( ulong nativeObject )
    {
      if ( nativeObject == 0 )
        throw new NullReferenceException( "NativeReference initialized with a zero nativeObject value." );

      pointer = nativeObject;
      AddRef( pointer, this );
    }

    ~NativeReference()
    {
      DetachFromNative();
    }

    public void Dispose()
    {
      DetachFromNative();
    }

    void DetachFromNative()
    {
      if ( pointer != 0 )
        Release( pointer );
      pointer = 0;
    }

    public static bool operator ==( NativeReference thiz, NativeReference other )
    {
      if ( object.ReferenceEquals( thiz, null ) )
        return object.ReferenceEquals( other, null );

      return thiz.Equals( other );
    }

    public static bool operator !=( NativeReference thiz, NativeReference other )
    {
      if ( object.ReferenceEquals( thiz, null ) )
        return !object.ReferenceEquals( other, null );

      return !thiz.Equals( other );
    }

    [MethodImpl( MethodImplOptions.InternalCall )]
    extern static void AddRef( ulong nativeThiz, NativeReference thiz );

    [MethodImpl( MethodImplOptions.InternalCall )]
    extern static void Release( ulong thiz );

    public bool Equals( [AllowNull] NativeReference other )
    {
      if ( object.ReferenceEquals( other, null ) )
        return false;

      return pointer == other.pointer;
    }

    public override bool Equals( object? obj )
    {
      return Equals( obj as NativeReference );
    }

    public override int GetHashCode()
    {
      return (int)( pointer & 0x00000000FFFFFFFF );
    }
  }
}
