using System.Numerics;

namespace MonoTest
{
  public static class Testbed
  {
    [EasyMono.ExportToCpp]
    public static int TestInterop( Test.ScriptTest nativeTester, string stringArg, Vector3 inputV )
    {
      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing testing" );
      Console.WriteLine( "=========================================" );

      Console.WriteLine( "Args are " + stringArg + " and " + inputV.ToString() );
      Console.WriteLine( "nativeTester is " + nativeTester.GetValue() + ", " + nativeTester.GetString() + ", " + nativeTester.GetVector() );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing instantiation from the managed side" );
      Console.WriteLine( "===========================================" );

      var ctorArg = new Vector3( 13, 24, 35 );
      var tester = new Test.ScriptTest( nativeTester.GetValue(), stringArg, ref ctorArg );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing classes" );
      Console.WriteLine( "===============" );

      var tester2 = nativeTester.ClassPass( tester );
      Console.WriteLine( "ClassPass tester2 is tester: " + ( tester2 is null ? "it is null" : ( tester == tester2 ) ) );
      var tester3 = tester.ClassPass( nativeTester );
      Console.WriteLine( "ClassPass tester3 is nativeTester: " + ( tester3 is null ? "it is null" : ( nativeTester == tester3 ) ) );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing primitives" );
      Console.WriteLine( "==================" );

      tester.DoubleIt();
      var tmp = tester.DoubleItAndReturn();
      Console.WriteLine( "DoubleItAndReturn result: " + tmp );
      tester.MultiplyAdd( 2, tmp );
      Console.WriteLine( "Value by MultiplyAdd: " + tester.GetValue() );
      tmp = tester.MultiplyAddAndReturn( 3, 10 );
      Console.WriteLine( "MultiplyAddAndReturn result: " + tmp );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing strings" );
      Console.WriteLine( "===============" );

      var strVal = tester.ConcatString( " is" );
      var strRef = tester.ConcatString( " Sparta" );
      var strPtr = tester.ConcatString( "!!!" );

      Console.WriteLine( "Result of ConcatString: " + strPtr ?? "null" );

      var strPtr2 = tester.ConcatString( null );
      Console.WriteLine( "Result of ConcatString with null: " + strPtr2 ?? "null" );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing linear math" );
      Console.WriteLine( "===================" );

      var va = new Vector3( 2, 3, 5 );
      var vb = new Vector3( 7, 11, 13 ) + inputV;

      var sumRef = tester.AddVectorsByRef( ref va, ref vb );
      Console.WriteLine( "Value by AddVectorsByRef: " + sumRef.ToString() );

      var sumVal = tester.AddVectorsByVal( va, vb );
      Console.WriteLine( "Value by AddVectorsByVal: " + sumVal.ToString() );

      var mat = new Matrix4x4( 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 );
      var matTransVal = tester.TransposeMatrixByVal( mat );
      Console.WriteLine( "Value by TransposeMatrixByVal: " + matTransVal.ToString() );

      var matTransRef = tester.TransposeMatrixByRef( ref mat );
      Console.WriteLine( "Value by TransposeMatrixByRef: " + matTransRef.ToString() );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing structures" );
      Console.WriteLine( "==================" );

      var globalStruct = new Test.GlobalStruct { thisValue = 5, thatValue = 10 };
      var localStruct = new Test.ScriptTest.LocalStruct { thisValue = 15, thatValue = 20 };

      tester.SetByGlobalStruct( ref globalStruct );
      Console.WriteLine( "Value by SetByGlobalStruct using GetValue: " + tester.GetValue() );
      Console.WriteLine( "Value by SetByGlobalStruct using GetGlobalStruct: { " + tester.GetGlobalStruct().thisValue + ", " + tester.GetGlobalStruct().thatValue + " }" );

      tester.SetByLocalStruct( ref localStruct );
      Console.WriteLine( "Value by SetByLocalStruct using GetValue: " + tester.GetValue() );
      Console.WriteLine( "Value by SetByLocalStruct using GetLocalStruct: { " + tester.GetLocalStruct().thisValue + ", " + tester.GetLocalStruct().thatValue + " }" );

      /////////////////////////////////////////////////
      Console.WriteLine( "" );
      Console.WriteLine( "Testing enums" );
      Console.WriteLine( "=============" );

      tester.SetValueByLocalEnum( Test.ScriptTest.LocalEnum.One );
      Console.WriteLine( "Value by local enum: " + tester.GetValue() );
      tester.SetValueByGlobalEnum( Test.Enum.Enum2.F );
      Console.WriteLine( "Value by global enum: " + tester.GetValue() );

      return tmp;
    }
  }
}
