// Copyright (c) 2014-2019, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

/*! \file variant.h
 *
 * \brief for dealing with variants
 *
 * \detailed Variant: OOP Union
 */
#pragma once

#include <variant>
#include <type_traits>
#include "serialization.h"

/*! \struct variant_serialization_traits
 * 
 * \brief used internally to contain a variant's traits/possible types
 *
 * \detailed see the macro VARIANT_TAG in serialization.h:140
 */
template <class Archive, class T>
struct variant_serialization_traits
{
};

/*! \struct variant_reader
 *
 * \brief reads a variant
 */
template <class Archive, class Variant, std::size_t Index = 0>
struct variant_reader
{
  typedef typename Archive::variant_tag_type variant_tag_type;

  static inline bool read(Archive &ar, Variant &v, variant_tag_type t)
  {
    using CurrentType = std::variant_alternative_t<Index, Variant>;
    
    if(variant_serialization_traits<Archive, CurrentType>::get_tag() == t) {
      CurrentType x;
      if(!::do_serialize(ar, x))
      {
        ar.stream().setstate(std::ios::failbit);
        return false;
      }
      v = std::move(x);
    } else if constexpr (Index + 1 < std::variant_size_v<Variant>) {
      // Recursive call for next variant type
      return variant_reader<Archive, Variant, Index + 1>::read(ar, v, t);
    } else {
      ar.stream().setstate(std::ios::failbit);
      return false;
    }
    return true;
  }
};

template <template <bool> class Archive, typename... Ts>
struct serializer<Archive<false>, std::variant<Ts...>>
{
  typedef std::variant<Ts...> variant_type;
  typedef typename Archive<false>::variant_tag_type variant_tag_type;

  static bool serialize(Archive<false> &ar, variant_type &v) {
    variant_tag_type t;
    ar.begin_variant();
    ar.read_variant_tag(t);
    return variant_reader<Archive<false>, variant_type>::read(ar, v, t);
  }
};

template <template <bool> class Archive, typename... Ts>
struct serializer<Archive<true>, std::variant<Ts...>>
{
  typedef std::variant<Ts...> variant_type;

  struct visitor {
    Archive<true> &ar;

    visitor(Archive<true> &a) : ar(a) { }

    template <class T>
    bool operator()(T &rv) const
    {
      ar.begin_variant();
      ar.write_variant_tag(variant_serialization_traits<Archive<true>, T>::get_tag());
      if(!::do_serialize(ar, rv))
      {
        ar.stream().setstate(std::ios::failbit);
        return false;
      }
      ar.end_variant();
      return true;
    }
  };

  static bool serialize(Archive<true> &ar, variant_type &v) {
    return std::visit(visitor(ar), v);
  }
};
