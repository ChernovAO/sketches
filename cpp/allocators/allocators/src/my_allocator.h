/// @file
/// @brief My allocator for tests allocator redefinition.

#ifndef __MY_ALLOCATOR_H_15_02_2016_1655__
#define __MY_ALLOCATOR_H_15_02_2016_1655__

#include <memory>

namespace details
{

// generic allocator for objects of class _Ty
template<class _Ty>
class MyAllocator : public std::_Allocator_base<_Ty>
{
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

     typedef std::false_type propagate_on_container_copy_assignment;
     typedef std::false_type propagate_on_container_move_assignment;
     typedef std::false_type propagate_on_container_swap;

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
          //MyZeroMemory< _Ty >( _Ptr, size );

          //_Ty nullVal = { 0 };

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
          allocator_.construct( _Ptr, std::forward< _Types >( _Args ) ... );
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

//template < class _Ty >
//void MyZeroMemory( typename MyAllocator< _Ty >::pointer _ptr, typename MyAllocator< _Ty >::size_type size )
//{
//}

/// non empty specification for unsigned char
//template <>
//void MyZeroMemory< unsigned char >( MyAllocator< unsigned char >::pointer _ptr, MyAllocator< unsigned char >::size_type size )
//{
//     unsigned char nullValue = { 0 };
//     volatile unsigned char* p = _ptr;
//     for ( MyAllocator< unsigned char >::size_type index = 0; index < size; ++index )
//     {
//          p[ index ] = nullValue;
//     }
//}

}

#endif // !__MY_ALLOCATOR_H_15_02_2016_1655__
