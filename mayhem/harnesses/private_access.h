// Test-only access to private members of upstream classes, without editing upstream headers.
// Explicit template instantiation is exempt from access checks ([temp.spec]/6), so instantiating
// PrivateAccess<Tag, &Class::member> defines a friend get(Tag) returning that member's pointer.
#pragma once

template<typename Tag, typename Tag::type Member>
struct PrivateAccess
{
    friend typename Tag::type get(Tag)
    {
        return Member;
    }
};

#define MAYHEM_PRIVATE_ACCESS(TagName, MemberType, MemberPtr)  \
    struct TagName                                              \
    {                                                           \
        using type = MemberType;                                \
        friend type get(TagName);                               \
    };                                                          \
    template struct PrivateAccess<TagName, MemberPtr>
