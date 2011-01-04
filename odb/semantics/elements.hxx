// file      : odb/semantics/elements.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_ELEMENTS_HXX
#define ODB_SEMANTICS_ELEMENTS_HXX

#include <map>
#include <list>
#include <vector>
#include <string>
#include <cstdlib> // std::abort
#include <cstddef> // std::size_t
#include <utility> // std::pair
#include <cassert>

#include <cutl/fs/path.hxx>
#include <cutl/container/graph.hxx>
#include <cutl/container/pointer-iterator.hxx>
#include <cutl/compiler/context.hxx>

#include <odb/gcc-fwd.hxx>

namespace semantics
{
  using namespace cutl;

  using std::size_t;
  using std::string;

  using container::graph;
  using container::pointer_iterator;

  using compiler::context;

  //
  //
  using fs::path;
  using fs::invalid_path;

  //
  //
  class access
  {
  public:
    enum value { public_, protected_, private_ };

    access (value v)
        : value_ (v)
    {
    }

    operator value () const
    {
      return value_;
    }

    char const* string () const;

  private:
    value value_;
  };

  //
  //
  class node;
  class edge;
  class unit;

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

  public:
    tree
    tree_node () const
    {
      return tree_node_;
    }

  public:
    path const&
    file () const
    {
      return file_;
    }

    size_t
    line () const
    {
      return line_;
    }

    size_t
    column () const
    {
      return column_;
    }

  public:
    template <typename X>
    bool
    is_a () const
    {
      return dynamic_cast<X const*> (this) != 0;
    }

  public:
    node (path const& file, size_t line, size_t column, tree);

    // Sink functions that allow extensions in the form of one-way
    // edges.
    //
    void
    add_edge_right (edge&)
    {
    }

  protected:
    // For virtual inheritance. Should never be actually called.
    //
    node ();

  protected:
    typedef semantics::unit unit_type;

    unit_type const&
    unit () const
    {
      return *unit_;
    }

    unit_type&
    unit ()
    {
      return *unit_;
    }

  private:
    friend class semantics::unit;

    void
    unit (unit_type& u)
    {
      unit_ = &u;
    }

  private:
    tree tree_node_;
    unit_type* unit_;

    path file_;
    size_t line_;
    size_t column_;
  };

  //
  //
  class scope;
  class nameable;


  //
  //
  class names: public edge
  {
  public:
    typedef semantics::scope scope_type;
    typedef semantics::access access_type;

    string const&
    name () const
    {
      return name_;
    }

    scope_type&
    scope () const
    {
      return *scope_;
    }

    // Return true if the entity that this edge names is a global scope.
    // In this case calling scope() is undefined behavior.
    //
    bool
    global_scope () const
    {
      return scope_ == 0;
    }

    nameable&
    named () const
    {
      return *named_;
    }

    access_type
    access () const
    {
      return access_;
    }

  public:
    names (string const& name, access_type access = access_type::public_)
        : name_ (name), access_ (access)
    {
    }

    void
    set_left_node (scope_type& n)
    {
      scope_ = &n;
    }

    void
    set_right_node (nameable& n)
    {
      named_ = &n;
    }

  protected:
    scope_type* scope_;
    nameable* named_;
    string name_;
    access_type access_;
  };

  //
  // Declarations and definitions.
  //

  class declares: public names
  {
  public:
    declares (string const& name, access_type access = access_type::public_)
        : names (name, access)
    {
    }
  };

  class defines: public declares
  {
  public:
    defines (string const& name, access_type access = access_type::public_)
        : declares (name, access)
    {
    }
  };

  class type;
  class typedefs: public declares
  {
  public:
    typedef semantics::type type_type;

    type_type&
    type () const;

  public:
    typedefs (string const& name, access_type access = access_type::public_)
        : declares (name, access)
    {
    }
  };

  //
  //
  class nameable: public virtual node
  {
    typedef std::vector<names*> names_list;

  public:
    typedef semantics::scope scope_type;

    // Return true if this type is unnamed and no literal name, such as
    // template-id or derived type declarator, can be used instead.
    //
    bool
    anonymous () const
    {
      if (defined_ != 0 || !named_.empty ())
        return false;

      return anonymous_ ();
    }

    // Return true if the node itself or any of the scopes up to the
    // global scope is anonymous. For a named class nested in an unnamed
    // class, anonymous() will return false and fq_anonymous() will
    // return true.
    //
    bool
    fq_anonymous () const;

    // As above but use the hint to select the first outer scope. If
    // hint is 0, use the defines edge.
    //
    bool
    fq_anonymous (names* hint) const;

    // Return the node's unqualifed name. If the node has a name, then
    // return it, preferring the defines edge. Otherwise, return a
    // literal name, e.g., template-id or a derived type declarator.
    // Finally, if the type is anonymous, return <anonymous> string.
    //
    string
    name () const
    {
      if (defined_ != 0)
        return defined_->name ();

      if (!named_.empty ())
        return named_[0]->name ();

      return name_ ();
    }

    // Return the node's fully-qualifed name.
    //
    virtual string
    fq_name () const;

    // As above but use the hint to select the first outer scope. If hint
    // is 0, use the defines edge.
    //
    virtual string
    fq_name (names* hint) const;

    // Return true if the type is named.
    //
    bool
    named_p () const
    {
      return defined_ != 0 || !named_.empty ();
    }

    scope_type&
    scope () const
    {
      return named ().scope ();
    }

    names&
    named () const
    {
      return defined_ != 0 ? *defined_ : *named_[0];
    }

  public:
    nameable ()
        : defined_ (0)
    {
    }

    void
    add_edge_right (defines& e)
    {
      assert (defined_ == 0);
      defined_ = &e;
    }

    void
    add_edge_right (names& e)
    {
      named_.push_back (&e);
    }

    using node::add_edge_right;

  private:
    bool
    anonymous_ () const;

    string
    name_ () const;

  private:
    defines* defined_;
    names_list named_;
  };


  //
  //
  class scope: public virtual nameable
  {
  protected:
    typedef std::list<names*> names_list;
    typedef std::map<names*, names_list::iterator> list_iterator_map;
    typedef std::map<string, names_list> names_map;

  public:
    typedef pointer_iterator<names_list::iterator> names_iterator;
    typedef pointer_iterator<names_list::const_iterator> names_const_iterator;

    typedef
    std::pair<names_const_iterator, names_const_iterator>
    names_iterator_pair;

  public:
    bool
    global_scope () const
    {
      return named ().global_scope ();
    }

    scope&
    scope_ () const
    {
      return nameable::scope ();
    }

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

    virtual names_iterator_pair
    find (string const& name) const;

    names_iterator
    find (names&);

  public:
    scope (path const& file, size_t line, size_t column, tree tn)
        : node (file, line, column, tn)
    {
    }

    void
    add_edge_left (names&);

    using nameable::add_edge_right;

  protected:
    scope ()
    {
    }

  private:
    names_list names_;
    list_iterator_map iterator_map_;
    names_map names_map_;
  };

  //
  //
  class belongs;
  class qualifies;

  class type: public virtual nameable
  {
    typedef std::vector<qualifies*> qualified;

  public:
    typedef pointer_iterator<qualified::const_iterator> qualified_iterator;

    qualified_iterator
    qualified_begin () const
    {
      return qualified_.begin ();
    }

    qualified_iterator
    qualified_end () const
    {
      return qualified_.end ();
    }

  public:
    void
    add_edge_right (belongs&)
    {
    }

    void
    add_edge_right (qualifies& e)
    {
      qualified_.push_back (&e);
    }

    using nameable::add_edge_right;

  private:
    qualified qualified_;
  };

  //
  //
  class instance;

  class belongs: public edge
  {
  public:
    typedef semantics::type type_type;
    typedef semantics::instance instance_type;

    type_type&
    type () const
    {
      return *type_;
    }

    instance_type&
    instance () const
    {
      return *instance_;
    }

  public:
    void
    hint (names& hint)
    {
      hint_ = &hint;
    }

    names*
    hint () const
    {
      return hint_;
    }

  public:
    belongs ()
        : hint_ (0)
    {
    }

    void
    set_left_node (instance_type& n)
    {
      instance_ = &n;
    }

    void
    set_right_node (type_type& n)
    {
      type_ = &n;
    }

  private:
    type_type* type_;
    instance_type* instance_;
    names* hint_;
  };

  //
  //
  class instance: public virtual node
  {
  public:
    typedef semantics::type type_type;
    typedef semantics::belongs belongs_type;

    type_type&
    type () const
    {
      return belongs_->type ();
    }

    belongs_type&
    belongs () const
    {
      return *belongs_;
    }

  public:
    void
    add_edge_left (belongs_type& e)
    {
      belongs_ = &e;
    }

  protected:
    instance ()
    {
    }

  private:
    belongs_type* belongs_;
  };

  // Data member for class and union types.
  //
  class data_member: public nameable, public instance
  {
  public:
    data_member (path const& file, size_t line, size_t column, tree tn)
        : node (file, line, column, tn)
    {
    }
  };

  // Unsupported type.
  //
  class unsupported_type: public type
  {
  public:
    string const&
    type_name () const
    {
      return type_name_;
    }

  public:
    unsupported_type (path const& file,
                      size_t line,
                      size_t column,
                      tree tn,
                      string const& type_name)
        : node (file, line, column, tn), type_name_ (type_name)
    {
    }

  private:
    string const type_name_;
  };
}

#include <odb/semantics/elements.ixx>

#endif // ODB_SEMANTICS_ELEMENTS_HXX
