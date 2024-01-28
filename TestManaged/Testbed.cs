using System.Numerics;

namespace MonoTest
{
  public static class Testbed
  {
    [EasyMono.ExportToCpp]
    public static int TestInterop( Test.ScriptTest nativeTester, string stringArg )
    {
      Console.WriteLine( "Begin program with arg " + stringArg );
      Console.WriteLine( "Entering with " + nativeTester.ToString() );
      Console.WriteLine( "Starting with " + nativeTester.GetValue().ToString() );

      var tester = new Test.ScriptTest( nativeTester.GetValue(), nativeTester.GetString() );

      var tester2 = nativeTester.ClassPass( tester );
      var tester3 = tester.ClassPass( nativeTester );

      var strVal = tester.ConcatCString( " is" );
      var strRef = tester.ConcatCString( " Sparta" );
      var strPtr = tester.ConcatCString( "!!!" );

      Console.WriteLine( strPtr ?? "empty" );

      var strCptr2 = tester.ConcatCString( null );
      Console.WriteLine( strCptr2 ?? "empty" );

      tester.DoubleIt();
      var tmp = tester.DoubleItAndReturn();
      Console.WriteLine( "Ptr DoubleItAndReturn " + tmp.ToString() );
      tester.MultiplaAdd( 2, tmp );
      tmp = tester.MultiplaAddAndReturn( 3, 10 );
      Console.WriteLine( "Ptr MultiplaAddAndReturn " + tmp.ToString() );

      var va = new Vector3( 2, 3, 5 );
      var vb = new Vector3( 7, 11, 13 );

      var sumRef = tester.AddVectorsByRef( ref va, ref vb );
      Console.WriteLine( sumRef.ToString() );

      var sumVal = tester.AddVectorsByVal( va, vb );
      Console.WriteLine( sumVal.ToString() );

      var mat = new Matrix4x4( 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 );
      Console.WriteLine( mat.ToString() );

      var matTransVal = tester.TransposeMatrixByVal( mat );
      Console.WriteLine( matTransVal.ToString() );

      var matTransRef = tester.TransposeMatrixByRef( ref mat );
      Console.WriteLine( matTransRef.ToString() );

      return tmp;
    }
  }
}
