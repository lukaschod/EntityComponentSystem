#pragma once

#include <type_traits>

#define avoid_alias __restrict
//#define avoid_alias

template <class T>
struct is_cwrite : std::bool_constant<false> {};

template <class T>
struct is_cwrite<T& avoid_alias> : std::bool_constant<true> {};

template <class T>
struct is_cread : std::bool_constant<false> {};

template <class T>
struct is_cread<const T& avoid_alias> : std::bool_constant<true> {};

template <class T>
struct remove_cwrite {
    using type = T;
};

template <class T>
struct remove_cwrite<const T& avoid_alias> {
    using type = T;
};

template <class T>
struct remove_cwrite<T& avoid_alias> {
    using type = T;
};

template <typename T>
struct function_traits
    : public function_traits<decltype(&T::operator())>
{};
// For generic types, directly use the result of the signature of its 'operator()'

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const>
    // we specialize for pointers to member function
{
    enum { arity = sizeof...(Args) };
    // arity is the number of arguments.

    typedef ReturnType result_type;

    template <size_t i>
    struct arg
    {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };

    typedef typename arg<0>::type arg0;
    //typedef typename arg<1>::type arg1;
    //typedef typename remove_cwrite<arg<0>::type>::type arg0_raw;
};

template <typename T>
struct lambda_traits : public lambda_traits<decltype(&T::operator())> {};
// For generic types, directly use the result of the signature of its 'operator()'

template <typename ClassType, typename ReturnType, class Arg0>
struct lambda_traits<ReturnType(ClassType::*)(Arg0) const>
    // we specialize for pointers to member function
{
    enum { arg_count = 1 };
    typedef typename remove_cwrite<Arg0>::type arg0_type;
    typedef is_cwrite<Arg0> arg0_cwrite_type;
    typedef is_cread<Arg0> arg0_cread_type;
    typedef Arg0 arg0_raw_type;
};

template <typename ClassType, typename ReturnType, class Arg0, class Arg1>
struct lambda_traits<ReturnType(ClassType::*)(Arg0, Arg1) const>
    // we specialize for pointers to member function
{
    enum { arg_count = 2 };
    typedef typename remove_cwrite<Arg0>::type arg0_type;
    typedef typename remove_cwrite<Arg1>::type arg1_type;
    typedef is_cwrite<Arg0> arg0_cwrite_type;
    typedef is_cwrite<Arg1> arg1_cwrite_type;
    typedef is_cread<Arg0> arg0_cread_type;
    typedef is_cread<Arg1> arg1_cread_type;
    typedef Arg0 arg0_raw_type;
};

template <typename ClassType, typename ReturnType, class Arg0, class Arg1, class Arg2>
struct lambda_traits<ReturnType(ClassType::*)(Arg0, Arg1, Arg2) const>
    // we specialize for pointers to member function
{
    enum { arg_count = 3 };
    typedef typename remove_cwrite<Arg0>::type arg0_type;
    typedef typename remove_cwrite<Arg1>::type arg1_type;
    typedef typename remove_cwrite<Arg2>::type arg2_type;
    typedef is_cwrite<Arg0> arg0_cwrite_type;
    typedef is_cwrite<Arg1> arg1_cwrite_type;
    typedef is_cwrite<Arg2> arg2_cwrite_type;
    typedef is_cread<Arg0> arg0_cread_type;
    typedef is_cread<Arg1> arg1_cread_type;
    typedef is_cread<Arg2> arg2_cread_type;
    typedef Arg0 arg0_raw_type;
};