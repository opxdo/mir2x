#pragma once
#include <memory>
#include <string>
#include <ostream>
#include <cstddef>
#include <variant>
#include <unordered_map>
#include <type_traits>
#include <sol/sol.hpp>

// c++ internal types <----> luaVar <----> lua types as sol::object
//    lua_Integer         std::variant     sol::object
//    bool                                 sol::as_table_t<...>
//    double
//    std::string
//    std::unordered_map

namespace luaf
{
    template<typename... Ts> struct luaVarDispatcher: Ts...
    {
        using Ts::operator()...;
    };

    struct luaNil
    {
        char placeholder = 0;

        bool operator == (const luaNil &) const
        {
            return true;
        }

        template<typename Archive> void serialize(Archive & ar)
        {
            ar(placeholder);
        }
    };
}

namespace luaf
{
    class luaVarWrapper;
    namespace _details
    {
        struct _luaVarWrapperHash
        {
            size_t operator() (const luaVarWrapper &) const noexcept;
        };
    }
}

namespace luaf
{
    class luaVarWrapper;
    using luaArray = std::vector<luaVarWrapper>;
    using luaTable = std::unordered_map<luaVarWrapper, luaVarWrapper, _details::_luaVarWrapperHash>; // TODO using incomplete type, is it UB ?

    using luaVar = std::variant<
        // default initialized as nil
        luaNil,

        luaArray, // optional
        luaTable,

        // try integer then decimal
        // lua integer is always a number
        lua_Integer,
        double,
        bool,
        std::string>;
}

namespace luaf
{
    namespace _details
    {
        bool isArray(const  sol::table    &);
        bool isArray(const luaf::luaTable &);
    }
}

namespace luaf
{
    // luaVarWrapper behaves exactly same as luaVar
    // use it to avoid type dependency, C++ can not recursively define types
    class luaVarWrapper final
    {
        private:
            friend struct _details::_luaVarWrapperHash;

        private:
            std::unique_ptr<luaVar> m_ptr; // don't share underlaying variable

        public:
            /**/  luaVarWrapper() = default; // default initialized as luaNil
            /**/ ~luaVarWrapper() = default;

        public:
            template<typename T> luaVarWrapper(T t)
                : m_ptr(std::make_unique<luaVar>(std::move(t)))
            {}

        public:
            luaVarWrapper(luaNil) // no need to allocate memory if holding luaNil
                : luaVarWrapper()
            {}

        public:
            luaVarWrapper(luaVar v): m_ptr(std::visit(luaVarDispatcher
            {
                [](luaNil) -> std::unique_ptr<luaVar>
                {
                    return nullptr;
                },

                [](auto &&arg) -> std::unique_ptr<luaVar>
                {
                    return std::make_unique<luaVar>(std::move(arg));
                },
            }, std::move(v))){}

        public:
            luaVarWrapper              (const luaVarWrapper & );
            luaVarWrapper              (      luaVarWrapper &&);
            luaVarWrapper & operator = (      luaVarWrapper   );

        public:
            operator luaVar () const
            {
                if(m_ptr){
                    return *m_ptr; // coping
                }
                else{
                    return luaNil{};
                }
            }

        public:
            /* */ luaVar &get()       { return *m_ptr; }
            const luaVar &get() const { return *m_ptr; }

        public:
            bool operator == (const luaVarWrapper &) const;
            bool operator == (const luaVar        &) const;
            bool operator == (const luaNil        &) const;

        public:
            template<typename Archive> void serialize(Archive & ar)
            {
                ar(m_ptr);
            }
    };
}

namespace luaf
{
    std::string quotedLuaString(const std::string &);

    // sol is overly flexible to create sol::object
    // don't use generic template
    //
    //   template<typename T> sol::object buildLuaObj(sol::state_view, T)
    //
    // implement all possible types explicitly instead

    sol::object buildLuaObj(sol::state_view, luaNil);
    sol::object buildLuaObj(sol::state_view, luaVar);
    sol::object buildLuaObj(sol::state_view, luaVarWrapper);

    sol::object buildLuaObj(sol::state_view sv, lua_Integer);
    sol::object buildLuaObj(sol::state_view sv, double);
    sol::object buildLuaObj(sol::state_view sv, bool);
    sol::object buildLuaObj(sol::state_view sv, std::string);

    template<typename T> luaVar buildLuaVar(T t)
    {
        return luaVar(std::move(t));
    }

    template<template<typename> typename C, typename T, typename... Args> luaVar buildLuaVar(C<T, Args...> varList)
    {
        luaArray array;
        for(auto &v: varList){
            array.push_back(std::move(v));
        }
        return array;
    }

    luaVar buildLuaVar(luaVarWrapper);
    luaVar buildLuaVar(const sol::object &);

    std::vector<luaVar> vargBuildLuaVarList(const sol::variadic_args             &, size_t = 0, std::optional<size_t> = std::nullopt);
    std::vector<luaVar>  pfrBuildLuaVarList(const sol::protected_function_result &, size_t = 0, std::optional<size_t> = std::nullopt);
}

std::ostream & operator << (std::ostream &, const sol::object &);
std::ostream & operator << (std::ostream &, const sol::variadic_args &);
std::ostream & operator << (std::ostream &, const sol::protected_function_result &);
std::ostream & operator << (std::ostream &, const luaf::luaNil &);
std::ostream & operator << (std::ostream &, const luaf::luaVarWrapper &);
