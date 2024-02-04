using System.Numerics;

namespace MonoTest
{
  public static class Testbed
  {
    [EasyMono.ExportToCpp]
    public static int TestInterop( Test.ScriptTest nativeTester, string stringArg, Vector3 inputV )
    {
      Console.WriteLine( "Begin program with arg " + stringArg + " and " + inputV.ToString() );
      Console.WriteLine( "Entering with " + nativeTester.ToString() );
      Console.WriteLine( "Starting with " + nativeTester.GetValue().ToString() );

      var ctorArg = new Vector3( 13, 24, 35 );
      var tester = new Test.ScriptTest( nativeTester.GetValue(), nativeTester.GetString(), ref ctorArg );

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
      var vb = new Vector3( 7, 11, 13 ) + inputV;

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

      var globalStruct = new Test.GlobalStruct { thisValue = 5, thatValue = 10 };
      var localStruct = new Test.ScriptTest.LocalStruct { thisValue = 15, thatValue = 20 };

      tester.SetByGlobalStruct( ref globalStruct );
      Console.WriteLine( "Value by global struct: " + tester.GetValue() );
      Console.WriteLine( "" + tester.GetGlobalStruct().ToString() );

      tester.SetByLocalStruct( ref localStruct );
      Console.WriteLine( "Value by local struct: " + tester.GetValue() );
      Console.WriteLine( "" + tester.GetLocalStruct().ToString() );

      return tmp;
    }
  }
}
