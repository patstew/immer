//
// immer - immutable data structures for C++
// Copyright (C) 2016, 2017 Juan Pedro Bolivar Puente
//
// This file is part of immer.
//
// immer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// immer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with immer.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

#include <immer/memory_policy.hpp>
#include <immer/detail/hamts/champ.hpp>
#include <immer/detail/hamts/champ_iterator.hpp>

#include <functional>

namespace immer {

/*!
 * Immutable unordered mapping of values from type `K` to type `T`.
 *
 * @tparam K    The type of the keys.
 * @tparam T    The type of the values to be stored in the container.
 * @tparam Hash The type of a function object capable of hashing
 *              values of type `T`.
 * @tparam Equal The type of a function object capable of comparing
 *              values of type `T`.
 * @tparam MemoryPolicy Memory management policy. See @ref
 *              memory_policy.
 *
 * @rst
 *
 * This cotainer provides a good trade-off between cache locality,
 * search, update performance and structural sharing.  It does so by
 * storing the data in contiguous chunks of :math:`2^{B}` elements.
 * When storing big objects, the size of these contiguous chunks can
 * become too big, damaging performance.  If this is measured to be
 * problematic for a specific use-case, it can be solved by using a
 * `immer::box` to wrap the type `T`.
 *
 * **Example**
 *   .. literalinclude:: ../example/map/intro.cpp
 *      :language: c++
 *      :start-after: intro/start
 *      :end-before:  intro/end
 *
 * @endrst
 *
 */
template <typename K,
          typename T,
          typename Hash          = std::hash<K>,
          typename Equal         = std::equal_to<K>,
          typename MemoryPolicy  = default_memory_policy,
          detail::hamts::bits_t B = default_bits>
class map
{
    using value_t = std::pair<K, T>;

    struct project_value
    {
        const T& operator() (const value_t& v) const noexcept
        {
            return v.second;
        }
    };

    struct default_value
    {
        const T& operator() () const
        {
            static T v{};
            return v;
        }
    };

    struct error_value
    {
        const T& operator() () const
        {
            throw std::out_of_range{"key not found"};
        }
    };

    struct hash_key
    {
        auto operator() (const value_t& v)
        { return Hash{}(v.first); }

        auto operator() (const K& v)
        { return Hash{}(v); }
    };

    struct equal_key
    {
        auto operator() (const value_t& a, const value_t& b)
        { return Equal{}(a.first, b.first) && a.second == b.second; }

        auto operator() (const value_t& a, const K& b)
        { return Equal{}(a.first, b); }
    };

    using impl_t = detail::hamts::champ<
        value_t, hash_key, equal_key, MemoryPolicy, B>;

public:
    using key_type = K;
    using mapped_type = T;
    using value_type = std::pair<K, T>;
    using size_type = detail::hamts::size_t;
    using diference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Equal;
    using reference = const value_type&;
    using const_reference = const value_type&;

    using iterator         = detail::hamts::champ_iterator<
        value_t, hash_key, equal_key, MemoryPolicy, B>;
    using const_iterator   = iterator;

    /*!
     * Default constructor.  It creates a set of `size() == 0`.  It
     * does not allocate memory and its complexity is @f$ O(1) @f$.
     */
    map() = default;

    /*!
     * Returns an iterator pointing at the first element of the
     * collection. It does not allocate memory and its complexity is
     * @f$ O(1) @f$.
     */
    iterator begin() const { return {impl_}; }

    /*!
     * Returns an iterator pointing just after the last element of the
     * collection. It does not allocate and its complexity is @f$ O(1) @f$.
     */
    iterator end() const { return {impl_, typename iterator::end_t{}}; }

    /*!
     * Returns the number of elements in the container.  It does
     * not allocate memory and its complexity is @f$ O(1) @f$.
     */
    size_type size() const { return impl_.size; }

    /*!
     * Returns `1` when the key `k` is contained in the map or `0`
     * otherwise. It won't allocate memory and its complexity is
     * *effectively* @f$ O(1) @f$.
     */
    size_type count(const K& k) const
    { return impl_.template get<detail::constantly<size_type, 1>,
                                detail::constantly<size_type, 0>>(k); }

    /*!
     * Returns a `const` reference to the values associated to the key
     * `k`.  If the key is not contained in the map, it returns a
     * default constructed value.  It does not allocate memory and its
     * complexity is *effectively* @f$ O(1) @f$.
     */
    const T& operator[] (const K& k) const
    { return impl_.template get<project_value, default_value>(k); }

    /*!
     * Returns a `const` reference to the values associated to the key
     * `k`.  If the key is not contained in the map, throws an
     * `std::out_of_range` error.  It does not allocate memory and its
     * complexity is *effectively* @f$ O(1) @f$.
     */
    const T& at(const K& k) const
    { return impl_.template get<project_value, error_value>(k); }

    /*!
     * Returns whether the sets are equal.
     */
    bool operator==(const map& other) const
    { return impl_.equals(other.impl_); }
    bool operator!=(const map& other) const
    { return !(*this == other); }

    /*!
     * Returns a map containing the association `value`.  If the key is
     * already in the map, it replaces its association in the map.
     * It may allocate memory and its complexity is *effectively* @f$
     * O(1) @f$.
     *
     * @rst
     *
     * **Example**
     *   .. literalinclude:: ../example/map/map.cpp
     *      :language: c++
     *      :dedent: 8
     *      :start-after: insert/start
     *      :end-before:  insert/end
     *
     * @endrst
     */
    map insert(value_type value) const
    { return impl_.add(std::move(value)); }

    /*!
     * Returns a map containing the association `(k, v)`.  If the key
     * is already in the map, it replaces its association in the map.
     * It may allocate memory and its complexity is *effectively* @f$
     * O(1) @f$.
     *
     * @rst
     *
     * **Example**
     *   .. literalinclude:: ../example/map/map.cpp
     *      :language: c++
     *      :dedent: 8
     *      :start-after: set/start
     *      :end-before:  set/end
     *
     * @endrst
     */
    map set(key_type k, mapped_type v) const
    { return impl_.add({std::move(k), std::move(v)}); }

    /*!
     * Returns a map without the key `k`.  If the key is not
     * associated in the map it returns the same map.  It may allocate
     * memory and its complexity is *effectively* @f$ O(1) @f$.
     *
     * @rst
     *
     * **Example**
     *   .. literalinclude:: ../example/map/map.cpp
     *      :language: c++
     *      :dedent: 8
     *      :start-after: erase/start
     *      :end-before:  erase/end
     *
     * @endrst
     */
    map erase(const K& k) const
    { return impl_.sub(k); }

    // Semi-private
    const impl_t& impl() const { return impl_; }

private:
    map(impl_t impl)
        : impl_(std::move(impl))
    {}

    impl_t impl_ = impl_t::empty;
};

} // namespace immer
