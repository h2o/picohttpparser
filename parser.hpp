/*
 * Copyright (c) 2020- Carter Mbotho
 *
 * The software is licensed under either the MIT License (below) or the Perl
 * license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "parser.h"

#include <string_view>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

namespace picohttp {
using request_ctx = phr_request_t;
using response_ctx = phr_response_t;

template <typename K, typename V, typename...Args>
struct HeaderLookup {
    using Multimap = std::unordered_multimap<K, V, Args...>;
    template <typename Value>
    HeaderLookup(Multimap& mm, Value&& def)
        : mm(mm),
          def{std::forward<Value>(def)}
    {}

    inline Multimap& operator()() {
        return mm;
    }

    inline const Multimap& operator()() const {
        return mm;
    }

    template <typename Key = K>
    V& operator[](Key&& k) {
        auto it = mm.get().find(k);
        if (it != mm.get().end()) {
            return it->second;
        }
        return def;
    }

    template <typename Key = K>
    const V& operator[](Key&& k) const {
        auto it = mm.get().find(k);
        if (it != mm.get().end()) {
            return it->second;
        }
        return def;
    }

    template <typename I>
    struct WeirdIterator {
        using iterator = typename Multimap::iterator;
        using const_iterator = typename Multimap::const_iterator;
        WeirdIterator(I it, V& def)
            : _def{def}, _bag{it}, it{it}
        {
            for (auto ii = it.first; ii != it.second; ii++) {
                _entries.emplace_back(ii->second);
            }
        }

        const_iterator begin() const { return _bag->first(); }
        const_iterator end() const  { return _bag->second(); }
        iterator begin() { return _bag->first(); }
        iterator end() { return _bag->second(); }

        V& operator*() const { return *it->second; }
        V*operator->() const { return it->second; }
        typename I::first_type operator++() { ++it; return *this; }
        typename I::first_type operator++(int) { it++; return *this; }
        friend bool operator==(const WeirdIterator& a, const WeirdIterator& b) {
            return a.it == b.it;
        }
        friend bool operator!=(const WeirdIterator& a, const WeirdIterator& b) {
            return a.it != b.it;
        }

        V& operator[](int index) {
            if (index >= size()) {
                return _def;
            }
            return _entries[index];
        }

        const V& operator[](int index) const {
            if (index >= size()) {
                return _def;
            }
            return _entries[index];
        }

        inline size_t size() const {
            return _entries.size();
        }

      private:
        std::vector<std::reference_wrapper<V>> _entries;
        V& _def;
        I _bag;
        typename I::first_type it;
    };

    template <typename Key = K>
    auto operator()(Key&& k)
        -> const WeirdIterator<std::pair<typename Multimap::iterator, typename Multimap::iterator>>
    {
        return {mm.get().find(k), def};
    }

    template <typename Key = K>
    auto operator()(Key&& k) const
        -> const WeirdIterator<std::pair<typename Multimap::iterator, typename Multimap::iterator>>
    {
        return {mm.get().find(k), def};
    }

private:
    std::reference_wrapper<Multimap> mm;
    V def;
};

namespace internal {
template <typename T> struct parser : public T {
    virtual int onHeader(std::string_view name, std::string_view value) = 0;
    virtual int onBodyPart(std::string_view part) = 0;
    virtual void reset() = 0;
    virtual ~parser() = default;
};

struct request_parser : public parser<request_ctx> {
    virtual int onUrl(std::string_view path) = 0;
    virtual void reset() override {
        method = M_UNKNOWN;
        content_length = 0;
        flags = 0;
        major_version = 1;
        minor_version = -1;
    }
    virtual ~request_parser() = default;
};

struct response_parser : public parser<response_ctx> {
    virtual int onStatusText(std::string_view path) = 0;
    virtual void reset() override {
        minor_version = -1;
        status = 0;
        flags = 0;
        content_length = 0;
        major_version = 1;
    }
    virtual ~response_parser() = default;
};
}

template <typename T>
    requires (std::is_same_v<request_ctx,  T> or
              std::is_same_v<response_ctx, T>)
struct Parser;

template<>
struct Parser<request_ctx> : public virtual internal::request_parser {};

template<>
struct Parser<response_ctx> : public virtual internal::response_parser {};

template <typename T>
struct DefaultParser : public virtual Parser<T> {
    int onHeader(std::string_view name, std::string_view value) override {
        headers.emplace(name, value);
        return 0;
    }

    int onBodyPart(std::string_view part) override {
        body.reserve(part.size());
        body.append(part);
        return 0;
    }

    virtual void reset() override {
        Parser<T>::reset();
        body = {};
        headers.clear();
    }

    std::string_view operator[](std::string_view name) {
        auto it = headers.find(name);
        if (it != headers.end()) {
            return it->second;
        }
        return {};
    }

    std::string_view operator()(std::string_view name, int index = 0) {
        auto its = headers.equal_range(name);
        if (its.first == its.second)
            return {};
        auto it = its.first;
        for (int i = 0; (i <= index) and (it != its.second); it++,i++) {
            if (i == index)
                return it->second;
        }
        return {};
    }

    std::string body;
    std::unordered_multimap<std::string_view, std::string_view> headers;
};

struct DefaultResponseParser : public virtual DefaultParser<response_ctx> {
    int onStatusText(std::string_view path) override {
        statusText = path;
        return 0;
    }

    void reset() override {
        DefaultParser<response_ctx>::reset();
        statusText = {};
    }
    std::string_view statusText;
};

struct DefaultRequestParser : public virtual DefaultParser<request_ctx> {
    int onUrl(std::string_view path) override {
        url = path;
        return 0;
    }

    void reset() override {
        DefaultParser<request_ctx>::reset();
        url = {};
    }

    std::string_view url;
};

using OnHeader = std::function<int(std::string_view name, std::string_view value)>;
using OnData   = std::function<int(std::string_view)>;

struct ResponseParserCb {
    template <class P> ResponseParserCb(P& p)
    {
        onHeader       = std::bind(&P::onHeader,
                             &p, std::placeholders::_1, std::placeholders::_2);
        onBodyPart     =  std::bind(&P::onBodyPart,
                             &p, std::placeholders::_1);
        onStatusText   =  std::bind(&P::onStatusText,
                             &p, std::placeholders::_1);
        p.cbp = this;
    }

    OnHeader  onHeader{nullptr};
    OnData    onStatusText{nullptr};
    OnData    onBodyPart{nullptr};

    static int cbOnHeader(void *d, const char *name, size_t nl, const char* val, size_t vl) {
        auto cb = static_cast<ResponseParserCb *>(d);
        if (cb and cb->onHeader) {
            return cb->onHeader({name, nl}, {val, vl});
        }
        return -1;
    }

    static int cbOnBodyPart(void *d, const char *part, size_t len) {
        auto cb = static_cast<ResponseParserCb *>(d);
        if (cb and cb->onBodyPart) {
            return cb->onBodyPart({part, len});
        }
        return -1;
    }

    inline static int cbOnStatusText(void *d, const char* data, size_t len) {
        auto cb = static_cast<ResponseParserCb *>(d);
        if (cb and cb->onStatusText) {
            return cb->onStatusText({data, len});
        }
        return -1;
    }

    static constexpr phr_response_cb_t Instance{cbOnStatusText, cbOnHeader, cbOnBodyPart};

  private:
    ResponseParserCb() = default;
    ResponseParserCb(const ResponseParserCb &) = delete;
    ResponseParserCb(ResponseParserCb &&) = delete;
};

struct RequestParserCb {
    template <class P> RequestParserCb(P& p)
    {
        onHeader = std::bind(&P::onHeader,
                           &p, std::placeholders::_1, std::placeholders::_2);
        onBodyPart   =  std::bind(&P::onBodyPart,
                             &p, std::placeholders::_1);
        onUrl        =  std::bind(&P::onUrl,
                             &p, std::placeholders::_1);

        p.cbp = this;
    }

    OnHeader  onHeader{nullptr};
    OnData    onUrl{nullptr};
    OnData    onBodyPart{nullptr};

    static int cbOnHeader(void *d, const char *name, size_t nl, const char* val, size_t vl) {
        auto cb = static_cast<RequestParserCb *>(d);
        if (cb and cb->onHeader) {
            return cb->onHeader({name, nl}, {val, vl});
        }
        return -1;
    }

    static int cbOnBodyPart(void *d, const char *part, size_t len) {
        auto cb = static_cast<RequestParserCb *>(d);
        if (cb and cb->onBodyPart) {
            return cb->onBodyPart({part, len});
        }
        return -1;
    }

    inline static int cbOnUrl(void *d, const char* data, size_t len) {
        auto cb = static_cast<RequestParserCb *>(d);
        if (cb and cb->onUrl) {
            return cb->onUrl({data, len});
        }
        return -1;
    }

    static constexpr phr_request_cb_t Instance{cbOnUrl, cbOnHeader, cbOnBodyPart};

  private:
    RequestParserCb() = default;
    RequestParserCb(const RequestParserCb &) = delete;
    RequestParserCb(RequestParserCb &&) = delete;
};

template <typename K, typename V, typename... Args>
struct IsolatedHeaderParser : public HeaderLookup<K, V, Args...> {
    template <typename Value>
    IsolatedHeaderParser(Value&& value)
        : HeaderLookup<K, V, Args...>(headers, std::forward<Value>(value))
    {}

    std::unordered_multimap<K, V, Args...> headers;

    static int cbOnHeader(void *data, const char *name, size_t nl, const char* value, size_t vl) {
        auto p = reinterpret_cast<IsolatedHeaderParser<K, V, Args...> *>(data);
        if (p == nullptr) {
            return -1;
        }
        p->headers.template emplace(K{name, nl}, V{value, vl});
        return 0;
    }
};

}
