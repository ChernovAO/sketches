/// @file
/// @brief precompiled headers

#include <memory>

namespace allocators
{

template<class _Ty>
class MyAllocator
     : public std::_Allocator_base<_Ty>
{	// generic allocator for objects of class _Ty
public:
     typedef std::allocator<_Ty> other;

     typedef std::_Allocator_base<_Ty> _Mybase;
     typedef typename _Mybase::value_type value_type;

     typedef value_type *pointer;
     typedef const value_type *const_pointer;
     typedef void *void_pointer;
     typedef const void *const_void_pointer;

     typedef value_type& reference;
     typedef const value_type& const_reference;

     typedef size_t size_type;
     typedef ptrdiff_t difference_type;

     typedef false_type propagate_on_container_copy_assignment;
     typedef false_type propagate_on_container_move_assignment;
     typedef false_type propagate_on_container_swap;

     MyAllocator<_Ty> select_on_container_copy_construction() const
     { // return this allocator
          return (*this);
     }

     template<class _Other>
     struct rebind
     { // convert this type to allocator<_Other>
          typedef MyAllocator< _Other > other;
     };

     pointer address(reference _Val) const _NOEXCEPT
     { // return address of mutable _Val
          return ( allocator_.address( _Val ) );
     }

     const_pointer address(const_reference _Val) const _NOEXCEPT
     { // return address of nonmutable _Val
          return ( allocator_.address( _Val ) );
     }

     MyAllocator() _THROW0()
     {
     }

     MyAllocator(const MyAllocator<_Ty>&) _THROW0()
     {
          // construct by copying (do nothing)
     }

     template<class _Other>
     MyAllocator( const MyAllocator<_Other>& ) _THROW0()
     {
          // construct from a related allocator (do nothing)
     }

     template<class _Other>
     MyAllocator<_Ty>& operator=( const MyAllocator<_Other>& )
     {
          // assign from a related allocator (do nothing)
          return (*this);
     }

     void deallocate(pointer _Ptr, size_type size )
     {
          // deallocate object at _Ptr, ignore size
          allocator_.deallocate( _Ptr, size );
     }

     pointer allocate(size_type _Count)
     {
          // allocate array of _Count elements
          return allocator_.allocate( _Count );
     }

     pointer allocate(size_type _Count, const void * ptr )
     {
          // allocate array of _Count elements, ignore hint
          return allocator_.allocate( _Count, ptr );
     }

     void construct(_Ty *_Ptr)
     {
          // default construct object at _Ptr
          allocator_.construct( _Ptr );
     }

     void construct(_Ty *_Ptr, const _Ty& _Val)
     {
          // construct object at _Ptr with value _Val
          allocator_.construct( _Ptr, _Val );
     }

     template<class _Objty, class... _Types>
     void construct(_Objty *_Ptr, _Types&&... _Args)
     {
          allocator_.construct( _Ptr, _Args );
     }

     template<class _Uty>
     void destroy(_Uty *_Ptr)
     {
          // destroy object at _Ptr
          _Ptr->~_Uty();
     }

     size_t max_size() const _THROW0()
     {
          // estimate maximum array size
          return allocator_.max_size();
     }

private:
     std::allocator< _Ty > allocator_;
};

}