using System;
using System.Collections.Generic;
using System.Reflection;
using System.IO;
using System.Linq;

const string fileTemplate = """
#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>

namespace Scripting
{{
  namespace Detail
  {{
    MonoImage* GetMainMonoImage();
    void PrintException( MonoObject* e );
  }}
}}

namespace {0}::{1}
{{
{2}
}}
""";

const string ctorTemplate = """
  inline void {0}( {1} )
  {{
    auto monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "{2}", "{3}" ); assert( monoClass );
    auto monoDesc  = mono_method_desc_new( ":ctor({4})", false ); assert( monoDesc );
    auto monoFunc  = mono_method_desc_search_in_class( monoDesc, monoClass ); assert( monoFunc );
    mono_method_desc_free( monoDesc );
    {5}
    MonoObject* monoException = nullptr;
    mono_runtime_invoke( monoFunc, nullptr, {6}, &monoException );
    if ( monoException )
    {{
      EasyMono::Detail::PrintException( monoException );
      assert( false );
    }}
  }}

""";

const string methodTemplate = """
  inline {0} {1}( {2} )
  {{
    auto monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "{3}", "{4}" ); assert( monoClass );
    auto monoDesc  = mono_method_desc_new( ":{5}({6})", false ); assert( monoDesc );
    auto monoFunc  = mono_method_desc_search_in_class( monoDesc, monoClass ); assert( monoFunc );
    mono_method_desc_free( monoDesc );
    {7}
    MonoObject* monoException = nullptr;
    {8}mono_runtime_invoke( monoFunc, nullptr, {9}, &monoException );
    if ( monoException )
    {{
      EasyMono::Detail::PrintException( monoException );
      assert( false );
    }}{10}
  }}

""";

const string transformedArgumentListTemplate = """

    const void* args[] = {{ {0} }};

""";

const BindingFlags methodFilter = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;

bool IsScriptedClass( Type? clazz )
{
  if ( clazz == null )
    return false;
  if ( clazz.Name == "NativeReference" )
    return true;
  return IsScriptedClass( clazz.BaseType );
}

bool IsToBeExported( CustomAttributeData attrib )
{
  return attrib.ToString().Contains( "ExportToCpp" );
}

bool CtorNeedsExporting( ConstructorInfo ctor )
{
  foreach ( var attrib in ctor.CustomAttributes )
    if ( IsToBeExported( attrib ) )
      return true;

  return false;
}

bool MethodNeedsExporting( MethodInfo method )
{
  foreach ( var attrib in method.CustomAttributes )
    if ( IsToBeExported( attrib ) )
      return true;

  return false;
}

string ToCppNamespace( string ns )
{
  return ns.Replace( ".", "::" );
}

bool IsKnownStructure( ParameterInfo info, List<Tuple<string, string>> knownStructures, out string name )
{
  var knownStructure = knownStructures.Find( ( t ) => info.ParameterType.ToString() == t.Item2 );
  if ( knownStructure != null )
  {
    name = "const " + knownStructure.Item1 + "&";
    return true;
  }

  name = "";
  return false;
}

string ToCpp( ParameterInfo info, List<Tuple<string, string>> knownStructures )
{
  string result;

  if ( info.ParameterType == typeof( void ) )
    result = "void";
  else if ( info.ParameterType == typeof( Boolean ) )
    result = "bool";
  else if ( info.ParameterType == typeof( Byte ) )
    result = "uint8_t";
  else if ( info.ParameterType == typeof( SByte ) )
    result = "int8_t";
  else if ( info.ParameterType == typeof( Char ) )
    result = "wchar_t";
  else if ( info.ParameterType == typeof( Decimal ) )
    result = "Decimal_is_not_supported";
  else if ( info.ParameterType == typeof( Double ) )
    result = "double";
  else if ( info.ParameterType == typeof( Single ) )
    result = "float";
  else if ( info.ParameterType == typeof( Int16 ) )
    result = "int16_t";
  else if ( info.ParameterType == typeof( UInt16 ) )
    result = "uint16_t";
  else if ( info.ParameterType == typeof( Int32 ) )
    result = "int";
  else if ( info.ParameterType == typeof( UInt32 ) )
    result = "unsigned";
  else if ( info.ParameterType == typeof( IntPtr ) )
    result = "IntPtr_is_not_supported";
  else if ( info.ParameterType == typeof( UIntPtr ) )
    result = "UIntPtr_is_not_supported";
  else if ( info.ParameterType == typeof( Int64 ) )
    result = "int64_t";
  else if ( info.ParameterType == typeof( UInt64 ) )
    result = "uint64_t";
  else if ( info.ParameterType == typeof( Exception ) )
    result = "MonoException";
  else if ( info.ParameterType == typeof( object ) )
    result = "void";
  else if ( info.ParameterType == typeof( string ) )
    return "const wchar_t*";
  else if ( IsKnownStructure( info, knownStructures, out string name ) )
    result = name;
  else
    result = ( info.ParameterType.Namespace + "::" + info.ParameterType.Name ).Replace( ".", "::" );

  if ( !info.ParameterType.IsValueType )
    result += "*";

  return result;
}

string ToSig( ParameterInfo info )
{
  if ( info.ParameterType == typeof( string ) )
    return "string";
  return info.ParameterType.Name;
}

string ToCppArg( ParameterInfo info )
{
  if ( info.ParameterType.IsValueType )
    return "&" + info.Name;
  else if ( info.ParameterType == typeof( string ) )
    return info.Name + " ? mono_string_from_utf16( (mono_unichar2*)" + info.Name + " ) : nullptr";
  else if ( IsScriptedClass( info.ParameterType ) )
    return info.Name + "->GetOrCreateMonoObject()";
  else
    return info.Name ?? "NamelessParameter";
}

string ExportParameters( IEnumerable< ParameterInfo > parameters, bool ctor, List<Tuple<string, string>> knownStructures )
{
  var code = ctor ? "MonoObject* self" : "";
  foreach ( var parameter in parameters )
    code += string.Format( "{0} {1}, ", ToCpp( parameter, knownStructures ), parameter.Name );
  return code.TrimEnd().TrimEnd( ',' );
}

string ExportDeclarationArgumentList( IEnumerable< ParameterInfo > parameters )
{
  var code = "";
  foreach ( var parameter in parameters )
    code += ToSig( parameter ) + ",";
  code = code.TrimEnd( ',' );
  return code;
}

string ExportTransformedArgumentList( IEnumerable< ParameterInfo > parameters )
{
  if ( parameters.Count() == 0 )
    return "";

  string arguments = "";
  foreach ( var parameter in parameters )
    arguments += string.Format( "{0}, ", ToCppArg( parameter ) );

  return string.Format( transformedArgumentListTemplate, arguments.TrimEnd().TrimEnd( ',' ) );
}

string ExportPassTransformedArgumentList( IEnumerable< ParameterInfo > parameters )
{
  return parameters.Count() > 0 ? "(void**)args" : "nullptr";
}

string ExportReturnValue( ParameterInfo parameter )
{
  if ( parameter.ParameterType == typeof( void ) )
    return "";

  return "MonoObject* retVal = ";
}

string ExportReturnReturnValue( ParameterInfo parameter )
{
  if ( parameter.ParameterType == typeof( void ) )
    return "";

  if ( parameter.ParameterType.IsValueType )
    return "\n\n    return *(int32_t*)mono_object_unbox( retVal );";
  else if ( parameter.ParameterType == typeof( string ) )
    return "\n\n    return mono_string_chars( (MonoString*)retVal );";
  else
    return "\n\n    return retVal;";
}

string ExportCtor( ConstructorInfo ctor, Type klass, List<Tuple<string, string>> knownStructures )
{
  return string.Format( ctorTemplate
                      , ctor.Name
                      , ExportParameters( ctor.GetParameters(), true, knownStructures )
                      , klass.Namespace
                      , klass.Name
                      , ExportDeclarationArgumentList( ctor.GetParameters() )
                      , ExportTransformedArgumentList( ctor.GetParameters() )
                      , ExportPassTransformedArgumentList( ctor.GetParameters() ) );
}

string ExportMethod( MethodInfo method, Type klass, List<Tuple<string, string>> knownStructures )
{
  return string.Format( methodTemplate
                      , ToCpp( method.ReturnParameter, knownStructures )
                      , method.Name
                      , ExportParameters( method.GetParameters(), false, knownStructures )
                      , klass.Namespace
                      , klass.Name
                      , method.Name
                      , ExportDeclarationArgumentList( method.GetParameters() )
                      , ExportTransformedArgumentList( method.GetParameters() )
                      , ExportReturnValue( method.ReturnParameter )
                      , ExportPassTransformedArgumentList( method.GetParameters() )
                      , ExportReturnReturnValue( method.ReturnParameter ) );
}

bool NeedTranslation( Type type )
{
  if ( type.Namespace == null )
    return false;

  foreach ( var ctor in type.GetConstructors( methodFilter ) )
    if ( CtorNeedsExporting( ctor ) )
      return true;

  foreach ( var method in type.GetMethods( methodFilter ) )
    if ( MethodNeedsExporting( method ) )
      return true;

  return false;
}

if ( args.Length < 3)
{
  Console.WriteLine( "Invalid number of parameters. Usage:" );
  Console.WriteLine( "CPPtoCSInteropGenerator.exe CSAssemblyPath DictionaryPath OutputDirectoryPath" );
  return;
}

var assemblyPath = Path.GetFullPath( args[ 0 ] );
var assembly = Assembly.LoadFile( assemblyPath );

var dictionaryPath = Path.GetFullPath( args[ 1 ] );

var knownStructures = new List< Tuple< string, string > >();
var dictionaryText = File.ReadAllText( dictionaryPath );
var dictionaryTokens = dictionaryText.Split( new char[] { ' ', '\t', '\n', '\r', ',', ';' }, StringSplitOptions.RemoveEmptyEntries );

if ( ( dictionaryTokens.Length % 2 ) != 0 )
{
  Console.WriteLine( "The dictionary is invalid!" );
  return;
}

for ( var i = 0; i < dictionaryTokens.Length; i += 2 )
  knownStructures.Add( new Tuple<string, string>( dictionaryTokens[ i ], dictionaryTokens[ i + 1 ] ) );

foreach ( var type in assembly.GetTypes() )
{
  if ( !NeedTranslation( type ) )
    continue;

  string functions = "";

  foreach ( var ctor in type.GetConstructors( methodFilter ) )
    if ( CtorNeedsExporting( ctor ) )
      functions += ExportCtor( ctor, type, knownStructures );

  foreach ( var method in type.GetMethods( methodFilter ) )
    if ( MethodNeedsExporting( method ) )
      functions += ExportMethod( method, type, knownStructures );

  var fileContents = string.Format( fileTemplate, ToCppNamespace( type.Namespace ?? "#MissingNamespace" ), type.Name, functions );

  var path = args[ 2 ];
  
  var namespaceElements = type.Namespace?.Split( '.' );
  if ( namespaceElements != null )
    foreach ( var elem in namespaceElements )
      path = Path.Combine( path, elem );
  
  Directory.CreateDirectory( path );
  path = Path.Combine( path, type.Name + ".h" );
  File.WriteAllText( path, fileContents );
}
