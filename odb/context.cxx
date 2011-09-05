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
      db (options.database ()),
      keyword_set (data_->keyword_set_),
      include_regex (data_->include_regex_),
      embedded_schema (ops.generate_schema () &&
                       ops.schema_format ().count (schema_format::embedded)),
      top_object (data_->top_object_),
      cur_object (data_->cur_object_)
{
  assert (current_ == 0);
  current_ = this;

  for (size_t i (0); i < sizeof (keywords) / sizeof (char*); ++i)
    data_->keyword_set_.insert (keywords[i]);

  for (strings::const_iterator i (ops.include_regex ().begin ());
       i != ops.include_regex ().end (); ++i)
    data_->include_regex_.push_back (regexsub (*i));
}

context::
context ()
  : data_ (current ().data_),
    os (current ().os),
    unit (current ().unit),
    options (current ().options),
    db (current ().db),
    keyword_set (current ().keyword_set),
    include_regex (current ().include_regex),
    embedded_schema (current ().embedded_schema),
    top_object (current ().top_object),
    cur_object (current ().cur_object)
{
}

context* context::current_;

bool context::
null (semantics::data_member& m)
{
  semantics::type& t (m.type ());

  if (object_pointer (t))
  {
    // By default pointers can be null.
    //
    if (m.count ("null"))
      return true;

    if (!m.count ("not-null"))
    {
      if (t.count ("null"))
        return true;

      if (!t.count ("not-null"))
      {
        return true;
      }
    }

    return false;
  }
  else
  {
    // Everything else by default is not null.
    //
    if (m.count ("null"))
      return true;

    if (!m.count ("not-null"))
    {
      if (t.count ("null"))
        return true;

      if (!t.count ("not-null"))
      {
        // Check if this type is a wrapper.
        //
        if (t.get<bool> ("wrapper"))
        {
          // First see if it is null by default.
          //
          if (t.get<bool> ("wrapper-null-handler") &&
              t.get<bool> ("wrapper-null-default"))
            return true;

          // Otherwise, check the wrapped type.
          //
          if (t.get<semantics::type*> ("wrapper-type")->count ("null"))
            return true;
        }
      }
    }

    return false;
  }
}

bool context::
null (semantics::data_member& m, string const& kp)
{
  if (kp.empty ())
    return null (m);

  semantics::type& c (m.type ());
  semantics::type& t (member_type (m, kp));

  if (object_pointer (t))
  {
    if (m.count (kp + "-null"))
      return true;

    if (!m.count (kp + "-not-null"))
    {
      if (c.count (kp + "-null"))
        return true;

      if (!c.count (kp + "-not-null"))
      {
        if (t.count ("null"))
          return true;

        if (!t.count ("not-null"))
        {
          return true;
        }
      }
    }

    return false;
  }
  else
  {
    if (m.count (kp + "-null"))
      return true;

    if (!m.count (kp + "-not-null"))
    {
      if (c.count (kp + "-null"))
        return true;

      if (!c.count (kp + "-not-null"))
      {
        if (t.count ("null"))
          return true;

        if (!t.count ("not-null"))
        {
          // Check if this type is a wrapper.
          //
          if (t.get<bool> ("wrapper"))
          {
            // First see if it is null by default.
            //
            if (t.get<bool> ("wrapper-null-handler") &&
                t.get<bool> ("wrapper-null-default"))
              return true;

            // Otherwise, check the wrapped type.
            //
            if (t.get<semantics::type*> ("wrapper-type")->count ("null"))
              return true;
          }
        }
      }
    }

    return false;
  }
}

context::class_kind_type context::
class_kind (semantics::class_& c)
{
  if (object (c))
    return class_object;
  else if (view (c))
    return class_view;
  else if (composite (c))
    return class_composite;
  else
    return class_other;
}

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

  string const key (key_prefix + "-tree-type");

  if (m.count (key))
    return *indirect_value<semantics::type*> (m, key);

  // "See throught" wrappers.
  //
  semantics::type& t (m.type ());

  if (semantics::type* wt = wrapper (t))
    return *indirect_value<semantics::type*> (*wt, key);
  else
    return *indirect_value<semantics::type*> (t, key);
}

bool context::
composite_ (semantics::class_& c)
{
  bool r (true);

  //@@ This is bad. Did we add new value pragmas and forgot to
  //   account for them here?
  //
  r = r && c.count ("value");
  r = r && !c.count ("table");
  r = r && !c.count ("type");
  r = r && !c.count ("id-type");
  r = r && !c.count ("value-type");
  r = r && !c.count ("index-type");
  r = r && !c.count ("key-type");
  r = r && !c.count ("value-column");
  r = r && !c.count ("index-column");
  r = r && !c.count ("key-column");
  r = r && !c.count ("id-column");
  r = r && !c.count ("default");
  r = r && !c.count ("null");
  r = r && !c.count ("not-null");
  r = r && !c.count ("value-null");
  r = r && !c.count ("value-not-null");
  r = r && !c.count ("options");
  r = r && !c.count ("value-options");
  r = r && !c.count ("index-options");
  r = r && !c.count ("key-options");
  r = r && !c.count ("id-options");
  r = r && !c.count ("unordered");

  c.set ("composite-value", r);
  return r;
}

string context::
table_name (semantics::class_& t) const
{
  string name (options.table_prefix ());

  if (t.count ("table"))
    name += t.get<string> ("table");
  else
    name += t.name ();

  return name;
}

string context::
table_name (semantics::data_member& m, table_prefix const& p) const
{
  // The table prefix passed as the second argument must include
  // the table prefix specified with the --table-prefix option.
  //
  string const& gp (options.table_prefix ());
  assert (p.prefix.compare (0, gp.size (), gp) == 0);

  string name;

  // If a custom table name was specified, then ignore the top-level
  // table prefix.
  //
  if (m.count ("table"))
  {
    if (p.level != 1)
      name = p.prefix;
    else
      name = gp;

    name += m.get<string> ("table");
  }
  else
  {
    name = p.prefix;
    name += public_name_db (m);
  }

  return name;
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
column_options (semantics::data_member& m)
{
  // Accumulate options from both type and member.
  //
  semantics::type& t (m.type ());

  string r;

  if (t.count ("options"))
  {
    strings const& o (t.get<strings> ("options"));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  if (m.count ("options"))
  {
    strings const& o (m.get<strings> ("options"));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  return r;
}

string context::
column_options (semantics::data_member& m, string const& kp)
{
  if (kp.empty ())
    return column_options (m);

  string k (kp + "-options");

  // Accumulate options from type, container, and member.
  //
  semantics::type& c (m.type ());
  semantics::type& t (member_type (m, kp));

  string r;

  if (t.count ("options"))
  {
    strings const& o (t.get<strings> ("options"));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  if (c.count (k))
  {
    strings const& o (c.get<strings> (k));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  if (m.count (k))
  {
    strings const& o (m.get<strings> (k));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  return r;
}

string context::
database_type_impl (semantics::type& t, semantics::names* hint, bool id)
{
  type_map_type::const_iterator end (data_->type_map_.end ()), i (end);

  // First check the hinted name. This allows us to handle things like
  // size_t which is nice to map to the same type irrespective of the
  // actual type. Since this type can be an alias for the one we are
  // interested in, go into nested hints.
  //
  for (; hint != 0 && i == end; hint = hint->hint ())
  {
    i = data_->type_map_.find (t.fq_name (hint));
  }

  // If the hinted name didn't work, try the primary name (e.g.,
  // ::std::string) instead of a user typedef (e.g., my_string).
  //
  if (i == end)
    i = data_->type_map_.find (t.fq_name ());

  if (i != end)
    return id ? i->second.id_type : i->second.type;
  else
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
    traverse_simple (semantics::data_member& m)
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
    traverse_simple (semantics::data_member& m)
    {
      r_ = r_ || context::is_a (m, flags_);
    }

    virtual void
    traverse_container (semantics::data_member& m, semantics::type& c)
    {
      // We don't cross the container boundaries (separate table).
      //
      r_ = r_ || context::is_a (
        m,
        flags_ & (context::test_container |
                  context::test_straight_container |
                  context::test_inverse_container),
        context::container_vt (c),
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
    r = r || container_wrapper (m.type ());
  }

  if (f & test_straight_container)
  {
    r = r || (container_wrapper (m.type ()) && !inverse (m, kp));
  }

  if (f & test_inverse_container)
  {
    r = r || (container_wrapper (m.type ()) && inverse (m, kp));
  }

  return r;
}

bool context::
has_a (semantics::class_& c, unsigned short flags)
{
  has_a_impl impl (flags);
  impl.dispatch (c);
  return impl.result ();
}

string context::
process_include_path (string const& ip, bool prefix, char open)
{
  bool t (options.include_regex_trace ());
  string p (prefix ? options.include_prefix () : string ());

  if (!p.empty () && p[p.size () - 1] != '/')
    p.append ("/");

  string path (p + ip), r;

  if (t)
    cerr << "include: '" << path << "'" << endl;

  bool found (false);

  for (regex_mapping::const_iterator i (include_regex.begin ());
       i != include_regex.end (); ++i)
  {
    if (t)
      cerr << "try: '" << i->regex () << "' : ";

    if (i->match (path))
    {
      r = i->replace (path);
      found = true;

      if (t)
        cerr << "'" << r << "' : ";
    }

    if (t)
      cerr << (found ? '+' : '-') << endl;

    if (found)
      break;
  }

  if (!found)
    r = path;

  // Add brackets or quotes unless the path already has them.
  //
  if (!r.empty () && r[0] != '"' && r[0] != '<')
  {
    bool b (open == '<' || (open == '\0' && options.include_with_brackets ()));
    char op (b ? '<' : '"'), cl (b ? '>' : '"');
    r = op + r + cl;
  }

  return r;
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
