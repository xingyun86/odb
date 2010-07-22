// file      : odb/mysql/inline.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/mysql/common.hxx>
#include <odb/mysql/inline.hxx>

namespace
{
  struct class_: traversal::class_, context
  {
    class_ (context& c)
        : context (c)
    {
    }

    virtual void
    traverse (type& c)
    {
      if (c.file () != unit.file ())
        return;

      if (!c.count ("object"))
        return;

      string const& type (c.fq_name ());
      string traits ("access::object_traits< " + type + " >");

      id_member t (*this);
      t.traverse (c);
      semantics::data_member& id (*t.member ());

      os << "// " << c.name () << endl
         << "//" << endl
         << endl;

      // id ()
      //
      os << "inline" << endl
         << traits << "::id_type" << endl
         << traits << "::" << endl
         << "id (const object_type& obj)"
         << "{"
         << "return obj." << id.name () << ";" << endl
         << "}";
    }
  };
}

namespace mysql
{
  void
  generate_inline (context& ctx)
  {
    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::namespace_ ns;
    class_ c (ctx);

    unit >> unit_defines >> ns;
    unit_defines >> c;

    traversal::defines ns_defines;

    ns >> ns_defines >> ns;
    ns_defines >> c;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}