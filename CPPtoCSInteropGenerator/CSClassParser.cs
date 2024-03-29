﻿using System;
using System.Collections.Generic;
using System.Reflection;
using System.IO;
using System.Linq;

const string fileTemplate = """
#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>

namespace EasyMono
{{
  namespace Detail
  {{
    MonoImage* GetMainMonoImage();
    void PrintException( MonoObject* e );
  }}
}}

namespace {0}
{{
  class {1} final
  {{
{2}
{3}
  }};
}}
""";

const string selfInteropTemplate = """
    uint32_t monoObjectHandle = 0;
  public:
    {0}( MonoObject* monoObject )
    {{
      monoObjectHandle = mono_gchandle_new( monoObject, false );
    }}
    ~{1}()
    {{
      mono_gchandle_free( monoObjectHandle );
    }}
""";

const string ctorTemplate = """
    {0}( {1} )
    {{
      auto monoClass  = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "{2}", "{3}" ); assert( monoClass );
      auto monoObject = mono_object_new( mono_domain_get(), monoClass ); assert( monoObject );
      auto monoDesc   = mono_method_desc_new( ":.ctor({4})", false ); assert( monoDesc );
      auto monoFunc   = mono_method_desc_search_in_class( monoDesc, monoClass ); assert( monoFunc );
      mono_method_desc_free( monoDesc );
      {5}
      MonoObject* monoException = nullptr;
      mono_runtime_invoke( monoFunc, monoObject, {6}, &monoException );
      if ( monoException )
      {{
        EasyMono::Detail::PrintException( monoException );
        assert( false );
      }}
      monoObjectHandle = mono_gchandle_new( monoObject, false );
    }}

""";

const string methodTemplate = """
    {0}{1} {2}( {3} )
    {{{4}
      auto monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "{5}", "{6}" ); assert( monoClass );
      auto monoDesc  = mono_method_desc_new( ":{7}({8})", false ); assert( monoDesc );
      auto monoFunc  = mono_method_desc_search_in_class( monoDesc, monoClass ); assert( monoFunc );
      mono_method_desc_free( monoDesc );
      {9}
      {10}
      MonoObject* monoException = nullptr;
      {11}mono_runtime_invoke( monoFunc, {12}, {13}, &monoException );
      if ( monoException )
      {{
        EasyMono::Detail::PrintException( monoException );
        assert( false );
      }}{14}
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

Dictionary< Type, string > typeAlias = new Dictionary< Type, string >
{
  { typeof(bool), "bool" },
  { typeof(byte), "byte" },
  { typeof(char), "char" },
  { typeof(decimal), "decimal" },
  { typeof(double), "double" },
  { typeof(float), "single" },
  { typeof(int), "int" },
  { typeof(long), "long" },
  { typeof(object), "object" },
  { typeof(sbyte), "sbyte" },
  { typeof(short), "short" },
  { typeof(string), "string" },
  { typeof(uint), "uint" },
  { typeof(ulong), "ulong" },
  { typeof(void), "void" }
};

string ToSig( ParameterInfo info )
{
  if ( typeAlias.TryGetValue( info.ParameterType, out string result ) )
    return result;
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

string ExportParameters( IEnumerable< ParameterInfo > parameters, List<Tuple<string, string>> knownStructures )
{
  var code = "";
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

string ExportReturnReturnValue( ParameterInfo parameter, List<Tuple<string, string>> knownStructures )
{
  if ( parameter.ParameterType == typeof( void ) )
    return "";

  if ( parameter.ParameterType.IsValueType )
    return "\n\n      return *(int32_t*)mono_object_unbox( retVal );";
  else if ( parameter.ParameterType == typeof( string ) )
    return "\n\n      return mono_string_chars( (MonoString*)retVal );";
  else
  {
    var returnType = ToCpp( parameter, knownStructures );
    if ( returnType.EndsWith( '*' ) )
      returnType = returnType.Remove( returnType.Length - 1 );
    return "\n\n      return new " + returnType + "(retVal);";
  }
}

string ExportCtor( ConstructorInfo ctor, Type klass, List<Tuple<string, string>> knownStructures )
{
  return string.Format( ctorTemplate
                      , klass.Name
                      , ExportParameters( ctor.GetParameters(), knownStructures )
                      , klass.Namespace
                      , klass.Name
                      , ExportDeclarationArgumentList( ctor.GetParameters() )
                      , ExportTransformedArgumentList( ctor.GetParameters() )
                      , ExportPassTransformedArgumentList( ctor.GetParameters() ) );
}

string ExportGettingSelf(MethodInfo method)
{
  return method.IsStatic ? "" : "\n      auto monoObject = mono_gchandle_get_target( monoObjectHandle );";
}

string ExportVirtualCallHandler( MethodInfo method, Type klass )
{
  return ( method.IsVirtual || method.IsAbstract || klass.IsInterface ) ? "\n      monoFunc = mono_object_get_virtual_method( monoObject, monoFunc );" : "";
}

string ExportMethod( MethodInfo method, Type klass, List<Tuple<string, string>> knownStructures )
{
  return string.Format( methodTemplate
                      , method.IsStatic ? "static " : ""
                      , ToCpp( method.ReturnParameter, knownStructures )
                      , method.Name
                      , ExportParameters( method.GetParameters(), knownStructures )
                      , ExportGettingSelf( method )
                      , klass.Namespace
                      , klass.Name
                      , method.Name
                      , ExportDeclarationArgumentList( method.GetParameters() )
                      , ExportTransformedArgumentList( method.GetParameters() )
                      , ExportVirtualCallHandler( method, klass )
                      , ExportReturnValue( method.ReturnParameter )
                      , method.IsStatic ? "nullptr" : "monoObject"
                      , ExportPassTransformedArgumentList( method.GetParameters() )
                      , ExportReturnReturnValue( method.ReturnParameter, knownStructures) );
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

var configPath = Path.GetFullPath( args[ 1 ] );

var knownStructures = new List< Tuple< string, string > >();
var configText = File.ReadAllText( configPath );
var configTokens = configText.Split( new char[] { ' ', '\t', '\n', '\r', ',', ';' }, StringSplitOptions.RemoveEmptyEntries );

for ( var i = 0; i < configTokens.Length; )
{
  if (configTokens[ i ] == "map" )
  {
    knownStructures.Add( new Tuple< string, string >(configTokens[ i + 1 ], configTokens[ i + 2 ] ) );
    i += 3;
  }
  else
    i++;
}

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

  var fileContents = string.Format( fileTemplate
                                  , ToCppNamespace( type.Namespace ?? "#MissingNamespace" )
                                  , type.Name
                                  , type.IsAbstract && type.IsSealed ? "  public:" : string.Format( selfInteropTemplate, type.Name, type.Name )
                                  , functions );

  var path = args[ 2 ];

  var namespaceElements = type.Namespace?.Split( '.' );
  if ( namespaceElements != null )
    foreach ( var elem in namespaceElements )
      path = Path.Combine( path, elem );

  Directory.CreateDirectory( path );
  path = Path.Combine( path, type.Name + ".h" );
  File.WriteAllText( path, fileContents );
}
