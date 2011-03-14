// file      : odb/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cctype> // std::toupper, std::is{alpha,upper,lower}
#include <cassert>

#include <odb/context.hxx>
#include <odb/common.hxx>

using namespace std;

namespace
{
  char const* keywords[] =
  {
    "NULL",
    "and",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "class",
    "compl",
    "const",
    "const_cast",
    "continue",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "end_eq",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "not",
    "not_eq",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq"
  };
}

context::
~context ()
{
  if (current_ == this)
    current_ = 0;
}

context::
context (ostream& os_,
         semantics::unit& u,
         options_type const& ops,
         data_ptr d)
    : data_ (d ? d : data_ptr (new (shared) data (os_))),
      os (data_->os_),
      unit (u),
      options (ops),
      keyword_set (data_->keyword_set_),
      embedded_schema (ops.generate_schema () &&
                       ops.schema_format ().count (schema_format::embedded)),
      object (data_->object_)
{
  assert (current_ == 0);
  current_ = this;

  for (size_t i (0); i < sizeof (keywords) / sizeof (char*); ++i)
    data_->keyword_set_.insert (keywords[i]);
}

context::
context ()
  : data_ (current ().data_),
    os (current ().os),
    unit (current ().unit),
    options (current ().options),
    keyword_set (current ().keyword_set),
    embedded_schema (current ().embedded_schema),
    object (current ().object)
{
}

context* context::current_;

string context::
upcase (string const& s)
{
  string r;
  string::size_type n (s.size ());

  r.reserve (n);

  for (string::size_type i (0); i < n; ++i)
    r.push_back (toupper (s[i]));

  return r;
}

void context::
diverge (streambuf* sb)
{
  data_->os_stack_.push (data_->os_.rdbuf ());
  data_->os_.rdbuf (sb);
}

void context::
restore ()
{
  data_->os_.rdbuf (data_->os_stack_.top ());
  data_->os_stack_.pop ();
}

semantics::type& context::
member_type (semantics::data_member& m, string const& key_prefix)
{
  if (key_prefix.empty ())
    return m.type ();

  string const key ("tree-" + key_prefix + "-type");

  if (m.count (key))
    return *indirect_value<semantics::type*> (m, key);

  return *indirect_value<semantics::type*> (m.type (), key);
}

bool context::
comp_value_ (semantics::class_& c)
{
  bool r (true);

  //@@ This is bad. Did I add new value pragmas and forgot to
  //   account for them here?
  //
  r = r && c.count ("value");
  r = r && !c.count ("table");
  r = r && !c.count ("type");
  r = r && !c.count ("value-type");
  r = r && !c.count ("index-type");
  r = r && !c.count ("key-type");
  r = r && !c.count ("value-column");
  r = r && !c.count ("index-column");
  r = r && !c.count ("key-column");
  r = r && !c.count ("id-column");
  r = r && !c.count ("not-null");
  r = r && !c.count ("unordered");

  c.set ("composite-value", r);
  return r;
}

string context::
table_name (semantics::class_& t) const
{
  if (t.count ("table"))
    return t.get<string> ("table");
  else
    return t.name ();
}

string context::
table_name (semantics::data_member& m, table_prefix const& p) const
{
  // If a custom table name was specified, then ignore the top-level
  // table prefix.
  //
  if (m.count ("table"))
  {
    string const& name (m.get<string> ("table"));
    return p.level == 1 ? name : p.prefix + name;
  }

  return p.prefix + public_name_db (m);
}

string context::
column_name (semantics::data_member& m) const
{
  if (m.count ("column"))
    return m.get<string> ("column");
  else if (m.type ().count ("column"))
    return m.type ().get<string> ("column");
  else
    return public_name_db (m);
}

string context::
column_name (semantics::data_member& m, string const& p, string const& d) const
{
  string key (p + "-column");
  if (m.count (key))
    return m.get<string> (key);
  else if (m.type ().count (key))
    return m.type ().get<string> (key);
  else
    return d;
}

string context::
column_type (semantics::data_member& m, string const& kp)
{
  return kp.empty ()
    ? m.get<string> ("column-type")
    : indirect_value<string> (m, kp + "-column-type");
}

string context::
database_type_impl (semantics::type& t,
                    string const& type,
                    semantics::context& ctx,
                    column_type_flags f)
{
  // @@ If I handle additional qualifiers (e.g., NOT NULL, AUTO_INCREMENT)
  // separately, then I won't need to pass custom type anymore.
  //
  if (!type.empty ())
    return type;

  // Don't use the name hint here so that we get the primary name (e.g.,
  // ::std::string) instead of a user typedef (e.g., my_string).
  //
  string const& name (t.fq_name ());
  type_map_type::const_iterator i (data_->type_map_.find (name));

  if (i != data_->type_map_.end ())
  {
    string r (ctx.count ("id") ? i->second.id_type : i->second.type);

    if ((f & ctf_default_null) == 0)
      r += " NOT NULL";

    return r;
  }

  return string ();
}

static string
public_name_impl (semantics::data_member& m)
{
  string s (m.name ());
  size_t n (s.size ());

  // Do basic processing: remove trailing and leading underscores
  // as well as the 'm_' prefix.
  //
  // @@ What if the resulting names conflict?
  //
  size_t b (0), e (n - 1);

  if (n > 2 && s[0] == 'm' && s[1] == '_')
    b += 2;

  for (; b <= e && s[b] == '_'; b++) ;
  for (; e >= b && s[e] == '_'; e--) ;

  return b > e ? s : string (s, b, e - b + 1);
}

string context::
public_name_db (semantics::data_member& m) const
{
  return public_name_impl (m);
}

string context::
public_name (semantics::data_member& m) const
{
  return escape (public_name_impl (m));
}

string context::
flat_name (string const& fq)
{
  string r;
  r.reserve (fq.size ());

  for (string::size_type i (0), n (fq.size ()); i < n; ++i)
  {
    char c (fq[i]);

    if (c == ':')
    {
      if (!r.empty ())
        r += '_';
      ++i; // Skip the second ':'.
    }
    else
      r += c;
  }

  return r;
}

string context::
escape (string const& name) const
{
  typedef string::size_type size;

  string r;
  size n (name.size ());

  // In most common cases we will have that many characters.
  //
  r.reserve (n);

  for (size i (0); i < n; ++i)
  {
    char c (name[i]);

    if (i == 0)
    {
      if (!((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_'))
        r = (c >= '0' && c <= '9') ? "cxx_" : "cxx";
    }

    if (!((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_'))
      r += '_';
    else
      r += c;
  }

  if (r.empty ())
    r = "cxx";

  // Custom reserved words.
  //
  /*
  reserved_name_map_type::const_iterator i (reserved_name_map.find (r));

  if (i != reserved_name_map.end ())
  {
    if (!i->second.empty ())
      return i->second;
    else
      r += L'_';
  }
  */

  // Keywords
  //
  if (keyword_set.find (r) != keyword_set.end ())
  {
    r += '_';

    // Re-run custom words.
    //
    /*
    i = reserved_name_map.find (r);

    if (i != reserved_name_map.end ())
    {
      if (!i->second.empty ())
        return i->second;
      else
        r += L'_';
    }
    */
  }

  return r;
}

static string
charlit (unsigned int u)
{
  string r ("\\x");
  bool lead (true);

  for (short i (7); i >= 0; --i)
  {
    unsigned int x ((u >> (i * 4)) & 0x0F);

    if (lead)
    {
      if (x == 0)
        continue;

      lead = false;
    }

    r += static_cast<char> (x < 10 ? ('0' + x) : ('A' + x - 10));
  }

  return r;
}

static string
strlit_ascii (string const& str)
{
  string r;
  string::size_type n (str.size ());

  // In most common cases we will have that many chars.
  //
  r.reserve (n + 2);

  r += '"';

  bool escape (false);

  for (string::size_type i (0); i < n; ++i)
  {
    unsigned int u (static_cast<unsigned int> (str[i]));

    // [128 - ]     - unrepresentable
    // 127          - \x7F
    // [32  - 126]  - as is
    // [0   - 31]   - \X or \xXX
    //

    if (u < 32 || u == 127)
    {
      switch (u)
      {
      case '\n':
        {
          r += "\\n";
          break;
        }
      case '\t':
        {
          r += "\\t";
          break;
        }
      case '\v':
        {
          r += "\\v";
          break;
        }
      case '\b':
        {
          r += "\\b";
          break;
        }
      case '\r':
        {
          r += "\\r";
          break;
        }
      case '\f':
        {
          r += "\\f";
          break;
        }
      case '\a':
        {
          r += "\\a";
          break;
        }
      default:
        {
          r += charlit (u);
          escape = true;
          break;
        }
      }
    }
    else if (u < 127)
    {
      if (escape)
      {
        // Close and open the string so there are no clashes.
        //
        r += '"';
        r += '"';

        escape = false;
      }

      switch (u)
      {
      case '"':
        {
          r += "\\\"";
          break;
        }
      case '\\':
        {
          r += "\\\\";
          break;
        }
      default:
        {
          r += static_cast<char> (u);
          break;
        }
      }
    }
    else
    {
      // @@ Unrepresentable character.
      //
      r += '?';
    }
  }

  r += '"';

  return r;
}

string context::
strlit (string const& str)
{
  return strlit_ascii (str);
}

namespace
{
  struct column_count_impl: object_members_base
  {
    column_count_impl (bool out)
        : out_ (out), count_ (0)
    {
    }

    virtual void
    traverse (semantics::class_& c)
    {
      char const* key (out_ ? "out-column-count" : "in-column-count");

      if (c.count (key))
        count_ += c.get<size_t> (key);
      else
      {
        size_t n (count_);
        object_members_base::traverse (c);
        c.set (key, count_ - n);
      }
    }

    virtual void
    simple (semantics::data_member& m)
    {
      if (out_ || !context::inverse (m))
        count_++;
    }

  private:
    bool out_;
    size_t count_;
  };
}

size_t context::
in_column_count (semantics::class_& c)
{
  if (!c.count ("in-column-count"))
  {
    column_count_impl t (false);
    t.traverse (c);
  }

  return c.get<size_t> ("in-column-count");
}

size_t context::
out_column_count (semantics::class_& c)
{
  if (!c.count ("out-column-count"))
  {
    column_count_impl t (true);
    t.traverse (c);
  }

  return c.get<size_t> ("out-column-count");
}

namespace
{
  // Find id member.
  //
  struct id_member_impl: traversal::class_
  {
    id_member_impl ()
    {
      *this >> names_ >> member_;
    }

    virtual void
    traverse (semantics::class_& c)
    {
      member_.m_ = 0;
      names (c);
      c.set ("id-member", member_.m_);
    }

  private:
    struct member: traversal::data_member
    {
      virtual void
      traverse (semantics::data_member& m)
      {
        if (m.count ("id"))
          m_ = &m;
      }

      semantics::data_member* m_;
    };

    member member_;
    traversal::names names_;
  };
}

semantics::data_member& context::
id_member (semantics::class_& c)
{
  if (!c.count ("id-member"))
  {
    id_member_impl t;
    t.traverse (c);
  }

  return *c.get<semantics::data_member*> ("id-member");
}

namespace
{
  struct has_a_impl: object_members_base
  {
    has_a_impl (unsigned short flags)
        : r_ (false), flags_ (flags)
    {
    }

    bool
    result () const
    {
      return r_;
    }

    virtual void
    simple (semantics::data_member& m)
    {
      r_ = r_ || context::is_a (m, flags_);
    }

    virtual void
    container (semantics::data_member& m)
    {
      // We don't cross the container boundaries (separate table).
      //
      r_ = r_ || context::is_a (
        m,
        flags_ & (context::test_container |
                  context::test_straight_container |
                  context::test_inverse_container),
        context::container_vt (m.type ()),
        "value");
    }

  private:
    bool r_;
    unsigned short flags_;
  };
}

bool context::
is_a (semantics::data_member& m,
      unsigned short f,
      semantics::type& t,
      string const& kp)
{
  bool r (false);

  if (f & test_pointer)
  {
    r = r || object_pointer (t);
  }

  if (f & test_eager_pointer)
  {
    r = r || (object_pointer (t) && !lazy_pointer (t));
  }

  if (f & test_lazy_pointer)
  {
    r = r || (object_pointer (t) && lazy_pointer (t));
  }

  if (f & test_container)
  {
    r = r || container (m.type ());
  }

  if (f & test_straight_container)
  {
    r = r || (container (m.type ()) && !inverse (m, kp));
  }

  if (f & test_inverse_container)
  {
    r = r || (container (m.type ()) && inverse (m, kp));
  }

  return r;
}

bool context::
has_a (semantics::type& t, unsigned short flags)
{
  has_a_impl impl (flags);
  impl.dispatch (t);
  return impl.result ();
}


// namespace
//

void namespace_::
traverse (type& ns)
{
  // Only traverse namespaces from the main file.
  //
  if (ns.file () == unit.file ())
  {
    string name (ns.name ());

    if (name.empty ())
      os << "namespace";
    else
      os << "namespace " << name;

    os << "{";

    traversal::namespace_::traverse (ns);

    os << "}";
  }
}
