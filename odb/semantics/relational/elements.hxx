// file      : odb/semantics/relational/elements.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_ELEMENTS_HXX
#define ODB_SEMANTICS_RELATIONAL_ELEMENTS_HXX

#include <map>
#include <list>
#include <vector>
#include <string>
#include <cassert>

#include <cutl/container/graph.hxx>
#include <cutl/container/pointer-iterator.hxx>
#include <cutl/compiler/context.hxx>

#include <odb/semantics/relational/name.hxx>

namespace semantics
{
  namespace relational
  {
    using namespace cutl;

    using std::string;

    using container::graph;
    using container::pointer_iterator;

    using compiler::context;

    //
    //
    class node;
    class edge;

    //
    //
    class edge: public context
    {
    public:
      virtual
      ~edge () {}

    public:
      template <typename X>
      bool
      is_a () const
      {
        return dynamic_cast<X const*> (this) != 0;
      }
    };

    //
    //
    class node: public context
    {
    public:
      virtual
      ~node () {}

      // Return name of the node.
      //
      virtual string
      kind () const = 0;

    public:
      template <typename X>
      bool
      is_a () const
      {
        return dynamic_cast<X const*> (this) != 0;
      }

      // Sink functions that allow extensions in the form of one-way
      // edges.
      //
    public:
      void
      add_edge_right (edge&)
      {
      }
    };

    //
    //
    template <typename N>
    class scope;

    template <typename N>
    class nameable;

    //
    //
    template <typename N>
    class names: public edge
    {
    public:
      typedef N name_type;
      typedef relational::scope<N> scope_type;
      typedef relational::nameable<N> nameable_type;

      name_type const&
      name () const
      {
        return name_;
      }

      scope_type&
      scope () const
      {
        return *scope_;
      }

      nameable_type&
      nameable () const
      {
        return *nameable_;
      }

    public:
      names (name_type const& name): name_ (name) {}

      void
      set_left_node (scope_type& n)
      {
        scope_ = &n;
      }

      void
      set_right_node (nameable_type& n)
      {
        nameable_ = &n;
      }

    protected:
      name_type name_;
      scope_type* scope_;
      nameable_type* nameable_;
    };

    typedef names<uname> unames;
    typedef names<qname> qnames;

    //
    //
    template <typename N>
    class nameable: public virtual node
    {
    public:
      typedef N name_type;
      typedef relational::names<N> names_type;
      typedef relational::scope<N> scope_type;

      name_type const&
      name () const
      {
        return named_->name ();
      }

      scope_type&
      scope () const
      {
        return named ().scope ();
      }

      names_type&
      named () const
      {
        return *named_;
      }

    public:
      // Id identifies the C++ node (e.g., a class or a data member) that
      // this model node corresponds to. The ids are not necessarily unique
      // (e.g., there can be a foreign key and an index with the same id that
      // correspond to a container member). However, in any given scope, the
      // {id,typeid} must be unique. This becomes important when we try to
      // find correspondance between nodes during model diff'ing.
      //
      nameable (string const& id): id_ (id), named_ (0) {}

      void
      add_edge_right (names_type& e)
      {
        assert (named_ == 0);
        named_ = &e;
      }

      using node::add_edge_right;

    private:
      string id_;
      names_type* named_;
    };

    typedef nameable<uname> unameable;
    typedef nameable<qname> qnameable;


    //
    //
    struct duplicate_name
    {
      template <typename N>
      duplicate_name (relational::scope<N>&,
                      relational::nameable<N>& orig,
                      relational::nameable<N>& dup);

      node& scope;
      node& orig;
      node& dup;

      string name;
    };

    template <typename N>
    class scope: public virtual node
    {
    protected:
      typedef N name_type;
      typedef relational::names<N> names_type;
      typedef relational::nameable<N> nameable_type;

      typedef std::list<names_type*> names_list;
      typedef std::map<name_type, typename names_list::iterator> names_map;
      typedef
      std::map<names_type const*, typename names_list::iterator>
      names_iterator_map;

    public:
      typedef pointer_iterator<typename names_list::iterator> names_iterator;
      typedef
      pointer_iterator<typename names_list::const_iterator>
      names_const_iterator;

    public:
      // Iteration.
      //
      names_iterator
      names_begin ()
      {
        return names_.begin ();
      }

      names_iterator
      names_end ()
      {
        return names_.end ();
      }

      names_const_iterator
      names_begin () const
      {
        return names_.begin ();
      }

      names_const_iterator
      names_end () const
      {
        return names_.end ();
      }

      // Find.
      //
      names_iterator
      find (name_type const&);

      names_const_iterator
      find (name_type const&) const;

      names_iterator
      find (names_type const&);

      names_const_iterator
      find (names_type const&) const;

    public:
      scope ()
          : first_key_ (names_.end ())
      {
      }

      void
      add_edge_left (names_type&);

    private:
      names_list names_;
      names_map names_map_;
      names_iterator_map iterator_map_;

      typename names_list::iterator first_key_;
    };

    typedef scope<uname> uscope;
    typedef scope<qname> qscope;
  }
}

#include <odb/semantics/relational/elements.txx>

#endif // ODB_SEMANTICS_RELATIONAL_ELEMENTS_HXX
