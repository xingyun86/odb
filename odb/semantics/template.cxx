// file      : odb/semantics/template.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/template.hxx>

namespace semantics
{
  // type info
  //
  namespace
  {
    struct init
    {
      init ()
      {
        using compiler::type_info;

        // template_
        //
        {
          type_info ti (typeid (template_));
          ti.add_base (typeid (nameable));
          insert (ti);
        }

        // instantiates
        //
        {
          type_info ti (typeid (instantiates));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // instantiation
        //
        {
          type_info ti (typeid (instantiation));
          ti.add_base (typeid (node));
          insert (ti);
        }

        // type_template
        //
        {
          type_info ti (typeid (type_template));
          ti.add_base (typeid (template_));
          insert (ti);
        }

        // type_instantiation
        //
        {
          type_info ti (typeid (type_instantiation));
          ti.add_base (typeid (type));
          ti.add_base (typeid (instantiation));
          insert (ti);
        }

      }
    } init_;
  }
}