#include "clang-c/Index.h"
#include <iostream>
#include <vector>
#include <map>
#include <filesystem>
#include <cassert>
#include <Windows.h>

auto csFileTemplate = LR"(
using System.Runtime.CompilerServices;

namespace @
{
  public @class @ : @
  {
    @( ulong thiz ) : base( thiz )
    {
    }
@
  }
})";

auto csCtorTemplate = LR"(
    public @( @ ) : base( Ctor( @ ) )
    {
    }
)";

auto csExportedCtorTemplate = LR"(
    [MethodImpl(MethodImplOptions.InternalCall)]
    extern static UInt64 Ctor( @ );
)";

auto csExportedMethodTemplate = LR"(
    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern @ @( @ );
)";

auto cppFileTemplate = LR"(
#include <mono/metadata/loader.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

@
namespace EasyMono
{
  ScriptedClass* LoadNativePointer( MonoObject* monoObject );

  namespace Detail
  {
    MonoImage* GetMainMonoImage();
  }

  extern thread_local bool creatingFromCS;
}

void RegisterScriptInterface()
{
@

}
@
)";

auto cppClassTemplate = LR"(
  struct @
  {
@
  };
@
)";

auto cppClassGetterTemplate = LR"(
MonoClass* @::GetMonoClass()
{
  static MonoClass* monoClass = nullptr;
  if ( !monoClass )
  {
    monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "@", "@" );
    assert( monoClass );
  }
  
  return monoClass;
}
)";

auto cppCtorTemplate = LR"(
    static uint64_t __stdcall @( @ )
    {
      EasyMono::creatingFromCS = true;

      auto thiz = reinterpret_cast< uint64_t >( new @::@( @ ) );
    
      EasyMono::creatingFromCS = false;
      return thiz;
    }
)";

auto cppMethodTemplate = LR"(
    static @ __stdcall @( MonoObject* thiz@@ )
    {
      auto nativeThis = reinterpret_cast< @::@* >( EasyMono::LoadNativePointer( thiz ) );
      @ nativeThis->@( @ )@;
@
    }
)";

auto cppMethodRegistrationTemplate = LR"(
  mono_add_internal_call( "@::@", @::@ );)";

auto cppStringArgTranslationTemplate = LR"(
      @ @CStr = @ ? @( mono_string_chars( @ ) ) : @();
)";

template< typename T >
std::wstring ToString( T t )
{
  return std::to_wstring( t );
}

template<>
std::wstring ToString( std::wstring t )
{
  return t;
}

template<>
std::wstring ToString( const wchar_t* t )
{
  return t;
}

template< typename ... T >
static std::wstring FormatString( const wchar_t* s, T&& ... args )
{
  std::wstring out;

  ( [ & ]
    {
      while ( *s && *s != L'@' )
      {
        out += *s;
        ++s;
      }

      out += ToString( args );

      ++s;
    } ( ), ... );

  if ( *s )
    out += s;

  return out;
}

inline std::wstring W( const char* s )
{
  if ( !s || *s == 0 )
    return std::wstring();

  int len = int( strlen( s ) );

  auto wideCharCount = MultiByteToWideChar( CP_UTF8, 0, s, len, nullptr, 0 );
  std::vector< wchar_t > wideChars( wideCharCount );
  MultiByteToWideChar( CP_UTF8, 0, s, len, wideChars.data(), int( wideChars.size() ) );
  return std::wstring( wideChars.data(), wideChars.size() );
}

struct TypeDesc
{
  enum class Kind
  {
    None,
    Unknown,
    Primitive,
    Struct,
    Class,
    String,
  };

  Kind kind;
  std::wstring name;
  std::wstring csSpecificName;
};

struct ClassDesc
{
  struct MethodDesc
  {
    struct ArgumentDesc
    {
      std::wstring name;
      TypeDesc type;
      bool isConst;
      bool isReference;
      bool isPointer;
    };

    std::wstring name;
    ArgumentDesc returnValue;
    std::vector< ArgumentDesc > arguments;
  };

  std::wstring nameSpace;
  std::wstring name;

  std::wstring header;

  bool isFinal;

  std::vector< MethodDesc > methods;

  std::vector< std::wstring > baseClasses;
};

std::map< std::wstring, ClassDesc > classes;

std::vector<CXCursor> stack;

const ClassDesc* GetScriptedBase( const ClassDesc& desc )
{
  for ( auto& base : desc.baseClasses )
  {
    auto iter = classes.find( base );
    if ( iter != classes.end() )
    {
      if ( iter->second.name == L"ScriptedClass" )
        return &iter->second;
      if ( GetScriptedBase( iter->second ) )
        return &iter->second;
    }
  }

  return nullptr;
}

std::wstring ToString( CXString&& s )
{
  auto str = W( clang_getCString( s ) );
  clang_disposeString( s );
  return str;
}

std::wstring ToString( CXCursor cursor )
{
  return ToString( clang_getCursorSpelling( cursor ) );
}

std::wstring ToString( CXType type )
{
  auto str = ToString( clang_getTypeSpelling( clang_getNonReferenceType( type ) ) );

  // There has to be a better way of getting the type without const and pointer...
  if ( auto constLoc = str.find( L"const " ); constLoc != std::string::npos )
    str.erase( constLoc, 6 );
  if ( auto ptrLoc = str.find( L" *" ); ptrLoc != std::string::npos )
    str.erase( ptrLoc, 2 );
  return str;
}

std::wstring GetFullName( CXCursor cursor )
{
  if ( cursor.kind == CXCursor_TranslationUnit )
    return L"";

  if ( cursor.kind == CXCursor_ClassDecl || cursor.kind == CXCursor_StructDecl || cursor.kind == CXCursor_Namespace )
    return GetFullName( clang_getCursorSemanticParent( cursor ) ) + L"::" + ToString( cursor );

  return L"";
}

std::wstring GetFullName( CXType type )
{
  return GetFullName( clang_getTypeDeclaration( clang_getCanonicalType( type ) ) );
}

bool Contains( const std::wstring& s, const wchar_t* sub )
{
  return s.find( sub ) != std::string::npos;
}

void TranslateKnownStructureNames( TypeDesc& arg )
{
  if ( arg.kind == TypeDesc::Kind::String )
    arg.csSpecificName = L"string";
  else if ( Contains( arg.name, L"XMFLOAT2" ) )
    arg.csSpecificName = L"System.Numerics.Vector2";
  if ( Contains( arg.name, L"XMFLOAT3" ) )
    arg.csSpecificName = L"System.Numerics.Vector3";
  if ( Contains( arg.name, L"XMFLOAT4" ) )
    arg.csSpecificName = L"System.Numerics.Vector4";
  if ( Contains( arg.name, L"XMFLOAT4X4" ) )
    arg.csSpecificName = L"System.Numerics.Matrix4x4";
}

ClassDesc::MethodDesc::ArgumentDesc ParsePrimitiveArgument( const wchar_t* name, bool isConst, bool isPointer, bool isReference )
{
  ClassDesc::MethodDesc::ArgumentDesc arg;
  arg.type.kind   = TypeDesc::Kind::Primitive;
  arg.type.name   = name;
  arg.isConst     = isConst;
  arg.isPointer   = false;
  arg.isReference = false;
  return arg;
}

TypeDesc::Kind GetTypeKind( CXType type )
{
  if ( type.kind == CXType_WChar )
    return TypeDesc::Kind::String;

  auto decl = clang_getTypeDeclaration( clang_getCanonicalType( type ) );
  switch ( decl.kind )
  {
  case CXCursor_ClassDecl: return TypeDesc::Kind::Class;
  case CXCursor_StructDecl: return TypeDesc::Kind::Struct;
  default: return TypeDesc::Kind::Primitive;
  }
}

ClassDesc::MethodDesc::ArgumentDesc ParseArgument( CXType type, bool isConst )
{
  switch ( type.kind )
  {
  case CXType_Void: return ParsePrimitiveArgument( L"void", isConst, false, false );
  case CXType_Bool: return ParsePrimitiveArgument( L"bool", isConst, false, false );
  case CXType_Char_S: return ParsePrimitiveArgument( L"char", isConst, false, false );
  case CXType_UChar: return ParsePrimitiveArgument( L"byte", isConst, false, false );
  case CXType_UShort: return ParsePrimitiveArgument( L"ushort", isConst, false, false );
  case CXType_UInt: return ParsePrimitiveArgument( L"uint", isConst, false, false );
  case CXType_ULong: return ParsePrimitiveArgument( L"uint", isConst, false, false );
  case CXType_ULongLong: return ParsePrimitiveArgument( L"ulong", isConst, false, false );
  case CXType_SChar: return ParsePrimitiveArgument( L"sbyte", isConst, false, false );
  case CXType_Short: return ParsePrimitiveArgument( L"short", isConst, false, false );
  case CXType_Int: return ParsePrimitiveArgument( L"int", isConst, false, false );
  case CXType_Long: return ParsePrimitiveArgument( L"int", isConst, false, false );
  case CXType_LongLong: return ParsePrimitiveArgument( L"long", isConst, false, false );
  case CXType_Float: return ParsePrimitiveArgument( L"float", isConst, false, false );
  case CXType_Double: return ParsePrimitiveArgument( L"double", isConst, false, false );
  case CXType_Pointer:
  {
    ClassDesc::MethodDesc::ArgumentDesc arg;
    arg.type.kind   = GetTypeKind( clang_getPointeeType( type ) );
    arg.type.name   = GetFullName( clang_getPointeeType( type ) );
    arg.isConst     = isConst;
    arg.isPointer   = true;
    arg.isReference = false;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  case CXType_LValueReference:
  {
    ClassDesc::MethodDesc::ArgumentDesc arg;
    arg.type.kind   = GetTypeKind( clang_getNonReferenceType( type ) );
    arg.type.name   = GetFullName( clang_getNonReferenceType( type ) );
    arg.isConst     = isConst;
    arg.isPointer   = false;
    arg.isReference = true;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  case CXType_Elaborated:
  {
    ClassDesc::MethodDesc::ArgumentDesc arg;
    arg.type.kind   = GetTypeKind( clang_Type_getNamedType( type ) );
    arg.type.name   = GetFullName( clang_Type_getNamedType( type ) );
    arg.isConst     = isConst;
    arg.isPointer   = false;
    arg.isReference = false;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  }

  ClassDesc::MethodDesc::ArgumentDesc arg;
  arg.type.kind   = TypeDesc::Kind::Unknown;
  arg.type.name   = L"unknown";
  arg.isConst     = false;
  arg.isPointer   = false;
  arg.isReference = false;
  return arg;
}

void ParseStruct( CXCursor cursor )
{
}

ClassDesc::MethodDesc ParseMethod( CXCursor cursor )
{
  ClassDesc::MethodDesc desc;

  desc.name = ToString( cursor );

  bool isCtor = clang_getCursorKind( cursor ) == CXCursor_Constructor;

  if ( isCtor )
  {
    desc.returnValue.type.kind = TypeDesc::Kind::None;
  }
  else
  {
    auto retType = clang_getCursorResultType( cursor );
    auto retIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( retType ) );
    desc.returnValue = ParseArgument( retType, retIsConst );
  }

  int numArgs = clang_Cursor_getNumArguments( cursor );
  for ( int arg = 0; arg < numArgs; ++arg )
  {
    auto argCursor = clang_Cursor_getArgument( cursor, arg );
    auto argType = clang_getCursorType( argCursor );
    auto argIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( argType ) );
    desc.arguments.emplace_back( ParseArgument( argType, argIsConst ) );
    desc.arguments.back().name = ToString( argCursor );
  }

  return desc;
}

const wchar_t* ignoredNamespaces[] =
{
  L"std", L"ATL"
};

void ParseClass( CXCursor cursor )
{
  // No point in parsing some known namespaces
  if ( !stack.empty() )
    if ( auto name = ToString( stack.front() ); std::find( std::begin( ignoredNamespaces ), std::end( ignoredNamespaces ), name ) != std::end( ignoredNamespaces ) )
      return;

  std::wstring nameSpace;
  for ( auto& s : stack )
  {
    if ( !nameSpace.empty() )
      nameSpace += L"::";
    nameSpace += ToString( s );
  }

  auto name = ToString( cursor );
  auto key = nameSpace.empty() ? name : ( nameSpace + L"::" + name );

  auto iter = classes.find( key );
  if ( iter != classes.end() )
    return;

  auto& klass = classes[ key ];

  klass.name = name;
  klass.nameSpace = nameSpace;

  auto classExtent = clang_getCursorExtent( cursor );
  auto classStart  = clang_getRangeStart( classExtent );

  CXFile* filePath;
  clang_getExpansionLocation( classStart, (CXFile*)&filePath, nullptr, nullptr, nullptr );

  klass.header = ToString( clang_getFileName( filePath ) );

  struct ClassContext
  {
    ClassDesc* klass;
    bool isPublic = false;
  } ctx;

  ctx.klass = &klass;

  // We are not interested in fields and such. Only in what we can export to the interop.
  clang_visitChildren( cursor, []( CXCursor cursor, CXCursor parent, CXClientData client_data )
  {
    auto& ctx = *static_cast< ClassContext* >( client_data );

    auto kind = clang_getCursorKind( cursor );
    switch ( kind )
    {
    case CXCursor_CXXAccessSpecifier:
      ctx.isPublic = clang_getCXXAccessSpecifier( cursor ) == CX_CXXPublic;
      break;
    case CXCursor_CXXMethod:
    case CXCursor_Constructor:
      if ( ctx.isPublic )
        ctx.klass->methods.emplace_back( ParseMethod( cursor ) );
      break;
    case CXCursor_CXXFinalAttr:
      ctx.klass->isFinal = true;
      break;
    case CXCursor_CXXBaseSpecifier:
      ctx.klass->baseClasses.emplace_back( ToString( cursor ) );
      break;
    default:
      break;
    }

    return CXChildVisit_Continue;
  }, &ctx );
}

CXChildVisitResult visitTranslationUnit( CXCursor cursor, CXCursor parent, CXClientData client_data )
{
  auto parentKind = clang_getCursorKind( parent );

  if ( parentKind == CXCursor_TranslationUnit )
    while ( !stack.empty() )
      stack.pop_back();
  else if ( parentKind == CXCursor_Namespace )
    while ( !stack.empty() && !clang_equalCursors( parent, stack.back() ) )
      stack.pop_back();
  else
    assert( false );

  auto kind = clang_getCursorKind( cursor );
  switch ( kind )
  {
  case CXCursor_Namespace:
    stack.push_back( cursor );
    return CXChildVisit_Recurse;
  case CXCursor_ClassDecl:
    ParseClass( cursor );
    return CXChildVisit_Continue;
  case CXCursor_StructDecl:
    ParseStruct( cursor );
    return CXChildVisit_Continue;
  default:
    break;
  }

  return CXChildVisit_Continue;
}

void parseHeader( std::filesystem::path filePath, const char** clangArguments, int numClangArguments )
{
  std::cout << "Parsing " << filePath << std::endl;

  std::vector< const char* > args = { "-x", "c++" };
  for ( int argIx = 0; argIx < numClangArguments; ++argIx )
    args.push_back( clangArguments[ argIx ] );

  CXIndex index = clang_createIndex( 0, 1 );
  CXTranslationUnit unit = clang_parseTranslationUnit( index, filePath.string().data(), args.data(), int( args.size() ), nullptr, 0, CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing );

  if ( !unit )
  {
    std::cerr << "Unable to parse translation unit. Quitting.\n";
    return;
  }

  CXCursor cursor = clang_getTranslationUnitCursor( unit );
  clang_visitChildren( cursor, visitTranslationUnit, nullptr );
}

/////////////////////////////////////////////////////////////
/// Output formatting
/////////////////////////////////////////////////////////////

std::wstring ChangeNamespaceSeparator( const std::wstring ns, const wchar_t* c )
{
  auto t = ns;
  while ( true )
  {
    auto sep = t.find( L"::" );
    if ( sep == std::wstring::npos )
      break;
    t.replace( sep, 2, c );
  }
  return t;
}

std::wstring ExportCSTypeName( const std::wstring& name )
{
  auto csName = name;
  if ( csName.size() > 1 && csName[ 0 ] == L':' && csName[ 1 ] == L':' )
    csName = csName.substr( 2 );

  return ChangeNamespaceSeparator( csName, L"." );
}

std::wstring ExportCSArgumentType( const ClassDesc::MethodDesc::ArgumentDesc& argDesc )
{
  std::wstring s;

  if ( argDesc.type.kind != TypeDesc::Kind::Class && argDesc.type.kind != TypeDesc::Kind::String )
  {
    if ( argDesc.isReference || argDesc.isPointer )
      s += L"ref ";

    if ( argDesc.isConst )
      s += L"readonly ";
  }

  s += argDesc.type.csSpecificName.empty() ? ExportCSTypeName( argDesc.type.name ) : argDesc.type.csSpecificName;

  if ( argDesc.isPointer )
    s += L"?";

  return s;
}

std::wstring ExportCSArgument( const ClassDesc::MethodDesc::ArgumentDesc& argDesc )
{
  return ExportCSArgumentType( argDesc ) + L" " + argDesc.name;
}

std::wstring ExportCPPArgumentType( const ClassDesc::MethodDesc::ArgumentDesc& argDesc )
{
  std::wstring s;

  if ( argDesc.type.kind == TypeDesc::Kind::String )
  {
    s += L"MonoString*";
  }
  else if ( argDesc.type.kind == TypeDesc::Kind::Class )
  {
    if ( argDesc.isPointer || argDesc.isReference )
      s += L"MonoObject*";
    else
      s += L"ClassesShouldBePointersOrReferences";
  }
  else
  {
    if ( argDesc.isConst )
      s += L"const ";

    s += argDesc.type.name;

    if ( argDesc.isReference )
      s += L"&";
    else if ( argDesc.isPointer )
      s += L"*";
  }

  return s;
}

std::wstring ExportCPPArgument( const ClassDesc::MethodDesc::ArgumentDesc& argDesc )
{
  return ExportCPPArgumentType( argDesc ) + L" " + argDesc.name;
}

std::wstring ExportBaseName( const ClassDesc& scriptedBase, const wchar_t* customBaseName )
{
  bool isSC = scriptedBase.name == ( customBaseName ? customBaseName : L"ScriptedClass" );
  return isSC ? L"EasyMono.NativeReference" : ( L"public " + scriptedBase.nameSpace + L"." + scriptedBase.name );
}

std::wstring ExportClassQualifier( const ClassDesc& desc )
{
  return desc.isFinal ? L"sealed " : L"";
}

std::wstring ExportCSMethodArguments( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : methodDesc.arguments )
    code += ExportCSArgument( argDesc ) + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportCSMethodArgumentNames( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : methodDesc.arguments )
    code += argDesc.name + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportCSMethods( const ClassDesc& desc )
{
  std::wstring code;

  // Write the constructors first
  for ( auto& methodDesc : desc.methods )
    if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::None )
      code += FormatString( csCtorTemplate
                          , desc.name
                          , ExportCSMethodArguments( methodDesc )
                          , ExportCSMethodArgumentNames( methodDesc ) );

  for ( auto& methodDesc : desc.methods )
    if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::None )
      code += FormatString( csExportedCtorTemplate
                          , ExportCSMethodArguments( methodDesc ) );
    else
      code += FormatString( csExportedMethodTemplate
                          , ExportCSArgumentType( methodDesc.returnValue )
                          , methodDesc.name
                          , ExportCSMethodArguments( methodDesc ) );

  return code;
}

void WriteCSClass( const ClassDesc& desc, std::filesystem::path path, const wchar_t* customBaseName )
{
  if ( desc.nameSpace.empty() )
    return;

  auto scriptedBase = GetScriptedBase( desc );
  if ( !scriptedBase )
    return;

  auto fileContents = FormatString( csFileTemplate
                                  , ChangeNamespaceSeparator( desc.nameSpace, L"." )
                                  , ExportClassQualifier( desc )
                                  , desc.name
                                  , ExportBaseName( *scriptedBase, customBaseName )
                                  , desc.name
                                  , ExportCSMethods( desc ) );

  path.append( ChangeNamespaceSeparator( desc.nameSpace, L"/" ) );

  std::filesystem::create_directories( path );

  path.append( desc.name + L".cs" );

  FILE* csFileHandle = nullptr;
  if ( fopen_s( &csFileHandle, path.string().data(), "wt" ) == 0 )
  {
    fwprintf_s( csFileHandle, L"%s", fileContents.data() );
    fclose( csFileHandle );
  }
}

std::wstring ExportClassInclude( const ClassDesc& desc )
{
  if ( desc.methods.empty() )
    return L"";

  return L"#include \"" + desc.header + L"\"\n";
}

std::wstring ExportClassGetter( const ClassDesc& desc )
{
  return FormatString( cppClassGetterTemplate
                     , desc.nameSpace + L"::" + desc.name
                     , ChangeNamespaceSeparator( desc.nameSpace, L"." )
                     , desc.name );
}

std::wstring ExportClassGetters()
{
  std::wstring code;
  for ( auto& iter : classes )
    if ( !iter.second.nameSpace.empty() )
      if ( GetScriptedBase( iter.second ) )
        code += ExportClassGetter( iter.second );
  return code;
}

std::wstring ExportClassIncludes()
{
  std::wstring code;
  for ( auto& iter : classes )
    if ( !iter.second.nameSpace.empty() )
      if ( GetScriptedBase( iter.second ) )
        code += ExportClassInclude( iter.second );
  return code;
}

std::wstring ExportCPPMethodArguments( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : methodDesc.arguments )
    code += ExportCPPArgument( argDesc ) + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportCPPMethodArgumentNames( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code;
  for ( auto& argDesc : methodDesc.arguments )
  {
    if ( argDesc.type.kind == TypeDesc::Kind::String )
    {
      code += argDesc.name + L" ? mono_string_chars( " + argDesc.name + L" ) : nullptr";
    }
    else if ( argDesc.type.kind == TypeDesc::Kind::Class )
    {
      if ( argDesc.isReference )
        code += L"*";
      code += L"reinterpret_cast< " + argDesc.type.name + L"* >( EasyMono::LoadNativePointer( " + argDesc.name + L" ) )";
    }
    else
      code += argDesc.name;
    code += L", ";
  }
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportNativeThisPrefix( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::String )
    return L"auto monoRetStr =";
  else
    return L"return";
}

std::wstring ExportNativeThisPostfix( const ClassDesc::MethodDesc& methodDesc )
{
  return methodDesc.returnValue.type.kind == TypeDesc::Kind::Class ? L"->GetOrCreateMonoObject()" : L"";
}

std::wstring ExportCPPReturnStringHandling( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::String )
  {
    return L"      return monoRetStr ? mono_string_new_utf16( mono_get_root_domain(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;";
  }
  return L"";
}

std::wstring ExportClassBindings( const ClassDesc& desc )
{
  if ( desc.methods.empty() )
    return L"";

  std::wstring code;

  // Write Ctor wrappers first
  for ( auto& methodDesc : desc.methods )
  {
    if ( methodDesc.returnValue.type.kind != TypeDesc::Kind::None )
      continue;

    code += FormatString( cppCtorTemplate
                        , methodDesc.name
                        , ExportCPPMethodArguments( methodDesc )
                        , desc.nameSpace
                        , desc.name
                        , ExportCPPMethodArgumentNames( methodDesc ) );
  }

  for ( auto& methodDesc : desc.methods )
  {
    if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::None )
      continue;

    code += FormatString( cppMethodTemplate
                        , ExportCPPArgumentType( methodDesc.returnValue )
                        , methodDesc.name
                        , methodDesc.arguments.empty() ? L"" : L", "
                        , ExportCPPMethodArguments( methodDesc )
                        , desc.nameSpace
                        , desc.name
                        , ExportNativeThisPrefix( methodDesc )
                        , methodDesc.name
                        , ExportCPPMethodArgumentNames( methodDesc )
                        , ExportNativeThisPostfix( methodDesc )
                        , ExportCPPReturnStringHandling( methodDesc ) );
  }

  return code;
}

std::wstring ExportClassRegistrationName( const ClassDesc& desc )
{
  return ChangeNamespaceSeparator( desc.nameSpace + L"::" + desc.name, L"." );
}

std::wstring ExportClassWrapperName( const ClassDesc& desc )
{
  return ChangeNamespaceSeparator( desc.nameSpace + L"::" + desc.name, L"__" );
}

std::wstring ExportClassMethodRegistration( const ClassDesc& desc, const ClassDesc::MethodDesc& methodDesc )
{
  bool isCtor = methodDesc.returnValue.type.kind == TypeDesc::Kind::None;
  return FormatString( cppMethodRegistrationTemplate
                     , ExportClassRegistrationName( desc )
                     , isCtor ? L"Ctor" : methodDesc.name
                     , ExportClassWrapperName( desc )
                     , methodDesc.name );
}

std::wstring ExportClassMethodRegistrations( const ClassDesc& desc )
{
  std::wstring code;
  for ( auto& methodDesc : desc.methods )
    code += ExportClassMethodRegistration( desc, methodDesc );
  return code;
}

std::wstring ExportClassMethods()
{
  std::wstring code;
  for ( auto& iter : classes )
    if ( !iter.second.nameSpace.empty() )
      if ( GetScriptedBase( iter.second ) )
        code += FormatString( cppClassTemplate
                            , ExportClassWrapperName( iter.second )
                            , ExportClassBindings( iter.second )
                            , ExportClassMethodRegistrations( iter.second ) );

  return code;
}

void WriteCPPBindings( std::filesystem::path path )
{
  std::wstring code = FormatString( cppFileTemplate
                                  , ExportClassIncludes()
                                  , ExportClassMethods()
                                  , ExportClassGetters() );

  auto folder = path;
  folder.remove_filename();
  std::filesystem::create_directories( folder );

  FILE* cppFileHandle = nullptr;
  if ( fopen_s( &cppFileHandle, path.string().data(), "wt" ) == 0 )
  {
    fwprintf_s( cppFileHandle, L"%s", code.data() );
    fclose( cppFileHandle );
  }
}

int main( int argc, char* argv[] )
{
  if ( argc < 4 )
  {
    std::cout << "Invalid number of parameters." << std::endl << std::endl;
    std::cout << "For parsing a single header file:" << std::endl;
    std::cout << "CStoCPPInteropGenerator.exe HeaderFilePath GeneratedCppFilePath GeneratedCSFolderPath" << std::endl << std::endl;
    std::cout << "For parsing header files in a folder recursively:" << std::endl;
    std::cout << "CStoCPPInteropGenerator.exe SourceFolderPath GeneratedCppFilePath GeneratedCSFolderPath" << std::endl;
    return -1;
  }

  std::filesystem::path source = argv[ 1 ];
  std::filesystem::path cppDestination = argv[ 2 ];
  std::filesystem::path csDestination = argv[ 3 ];
  
  auto customScriptedBaseName = W( argv[ 4 ] );

  if ( std::filesystem::exists( cppDestination ) && std::filesystem::is_directory( cppDestination ) )
  {
    std::cout << "The GeneratedCppFilePath should not be a folder!" << std::endl;
    return -1;
  }

  if ( std::filesystem::exists( csDestination ) && !std::filesystem::is_directory( csDestination ) )
  {
    std::cout << "The GeneratedCSFolderPath should not be a file!" << std::endl;
    return -1;
  }

  if ( std::filesystem::is_directory( source ) )
  {
    for ( auto& entry : std::filesystem::recursive_directory_iterator( source ) )
      if ( entry.is_regular_file() && entry.path().has_extension() && entry.path().extension() == ".h" )
        parseHeader( entry.path(), (const char**)&argv[ 5 ], argc - 5 );
  }
  else
    parseHeader( source, (const char**)&argv[ 5 ], argc - 5 );

  for ( auto& iter : classes )
    WriteCSClass( iter.second, csDestination, customScriptedBaseName.data() );

  WriteCPPBindings( cppDestination );

  return 0;
}
