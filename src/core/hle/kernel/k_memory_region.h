// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/assert.h"
#include "common/common_types.h"
#include "common/intrusive_red_black_tree.h"

namespace Kernel {

class KMemoryRegion final : public Common::IntrusiveRedBlackTreeBaseNode<KMemoryRegion>,
                            NonCopyable {
    friend class KMemoryLayout;
    friend class KMemoryRegionTree;

public:
    static constexpr int Compare(const KMemoryRegion& lhs, const KMemoryRegion& rhs) {
        if (lhs.GetAddress() < rhs.GetAddress()) {
            return -1;
        } else if (lhs.GetAddress() <= rhs.GetLastAddress()) {
            return 0;
        } else {
            return 1;
        }
    }

    constexpr u64 GetAddress() const {
        return address;
    }

    constexpr u64 GetPairAddress() const {
        return pair_address;
    }

    constexpr u64 GetLastAddress() const {
        return last_address;
    }

    constexpr u64 GetEndAddress() const {
        return GetLastAddress() + 1;
    }

    constexpr std::size_t GetSize() const {
        return GetEndAddress() - GetAddress();
    }

    constexpr u32 GetAttributes() const {
        return attributes;
    }

    constexpr u32 GetType() const {
        return type_id;
    }

    constexpr void SetType(u32 type) {
        ASSERT(this->CanDerive(type));
        type_id = type;
    }

    constexpr bool Contains(uintptr_t address) const {
        ASSERT(this->GetEndAddress() != 0);
        return this->GetAddress() <= address && address <= this->GetLastAddress();
    }

    constexpr bool IsDerivedFrom(u32 type) const {
        return (this->GetType() | type) == this->GetType();
    }

    // constexpr bool HasTypeAttribute(KMemoryRegionAttr attr) const {
    //    return (this->GetType() | attr) == this->GetType();
    //}

    constexpr bool CanDerive(u32 type) const {
        return (this->GetType() | type) == type;
    }

    constexpr void SetPairAddress(u64 a) {
        pair_address = a;
    }

    // constexpr void SetTypeAttribute(KMemoryRegionAttr attr) {
    //    type_id |= attr;
    //}

private:
    constexpr KMemoryRegion() = default;
    constexpr KMemoryRegion(u64 address_, u64 last_address_)
        : address{address_}, last_address{last_address_} {}
    constexpr KMemoryRegion(u64 address_, u64 last_address_, u64 pair_address_, u32 attributes_,
                            u32 type_id_)
        : address(address_), last_address(last_address_), pair_address(pair_address_),
          attributes(attributes_), type_id(type_id_) {}
    constexpr KMemoryRegion(u64 address_, u64 last_address_, u32 attributes_, u32 type_id_)
        : KMemoryRegion(address_, last_address_, std::numeric_limits<uintptr_t>::max(), attributes_,
                        type_id_) {}

    const u64 address{};
    const u64 last_address{};
    u64 pair_address{};
    u32 attributes{};
    u32 type_id{};
};

class KMemoryRegionTree final : NonCopyable {
public:
    struct DerivedRegionExtents {
        const KMemoryRegion* first_region{};
        const KMemoryRegion* last_region{};

        constexpr DerivedRegionExtents() = default;

        constexpr u64 GetAddress() const {
            return this->first_region->GetAddress();
        }

        constexpr u64 GetLastAddress() const {
            return this->last_region->GetLastAddress();
        }

        constexpr u64 GetEndAddress() const {
            return this->GetLastAddress() + 1;
        }

        constexpr size_t GetSize() const {
            return this->GetEndAddress() - this->GetAddress();
        }
    };

private:
    using TreeType =
        Common::IntrusiveRedBlackTreeBaseTraits<KMemoryRegion>::TreeType<KMemoryRegion>;

public:
    using value_type = TreeType::value_type;
    using size_type = TreeType::size_type;
    using difference_type = TreeType::difference_type;
    using pointer = TreeType::pointer;
    using const_pointer = TreeType::const_pointer;
    using reference = TreeType::reference;
    using const_reference = TreeType::const_reference;
    using iterator = TreeType::iterator;
    using const_iterator = TreeType::const_iterator;

private:
    TreeType m_tree{};

public:
    constexpr KMemoryRegionTree() = default;

public:
    KMemoryRegion* FindModifiable(u64 address) {
        if (auto it = this->find(KMemoryRegion(address, address, 0, 0)); it != this->end()) {
            return std::addressof(*it);
        } else {
            return nullptr;
        }
    }

    const KMemoryRegion* Find(u64 address) const {
        if (auto it = this->find(KMemoryRegion(address, address, 0, 0)); it != this->cend()) {
            return std::addressof(*it);
        } else {
            return nullptr;
        }
    }

    const KMemoryRegion* FindByType(u32 type_id) const {
        for (auto it = this->cbegin(); it != this->cend(); ++it) {
            if (it->GetType() == type_id) {
                return std::addressof(*it);
            }
        }
        return nullptr;
    }

    const KMemoryRegion* FindByTypeAndAttribute(u32 type_id, u32 attr) const {
        for (auto it = this->cbegin(); it != this->cend(); ++it) {
            if (it->GetType() == type_id && it->GetAttributes() == attr) {
                return std::addressof(*it);
            }
        }
        return nullptr;
    }

    const KMemoryRegion* FindFirstDerived(u32 type_id) const {
        for (auto it = this->cbegin(); it != this->cend(); it++) {
            if (it->IsDerivedFrom(type_id)) {
                return std::addressof(*it);
            }
        }
        return nullptr;
    }

    const KMemoryRegion* FindLastDerived(u32 type_id) const {
        const KMemoryRegion* region = nullptr;
        for (auto it = this->begin(); it != this->end(); it++) {
            if (it->IsDerivedFrom(type_id)) {
                region = std::addressof(*it);
            }
        }
        return region;
    }

    DerivedRegionExtents GetDerivedRegionExtents(u32 type_id) const {
        DerivedRegionExtents extents;

        ASSERT(extents.first_region == nullptr);
        ASSERT(extents.last_region == nullptr);

        for (auto it = this->cbegin(); it != this->cend(); it++) {
            if (it->IsDerivedFrom(type_id)) {
                if (extents.first_region == nullptr) {
                    extents.first_region = std::addressof(*it);
                }
                extents.last_region = std::addressof(*it);
            }
        }

        ASSERT(extents.first_region != nullptr);
        ASSERT(extents.last_region != nullptr);

        return extents;
    }

public:
    void InsertDirectly(u64 address, u64 last_address, u32 attr = 0, u32 type_id = 0);
    bool Insert(u64 address, size_t size, u32 type_id, u32 new_attr = 0, u32 old_attr = 0);

    VAddr GetRandomAlignedRegion(size_t size, size_t alignment, u32 type_id);

    VAddr GetRandomAlignedRegionWithGuard(size_t size, size_t alignment, u32 type_id,
                                          size_t guard_size) {
        return this->GetRandomAlignedRegion(size + 2 * guard_size, alignment, type_id) + guard_size;
    }

public:
    // Iterator accessors.
    iterator begin() {
        return m_tree.begin();
    }

    const_iterator begin() const {
        return m_tree.begin();
    }

    iterator end() {
        return m_tree.end();
    }

    const_iterator end() const {
        return m_tree.end();
    }

    const_iterator cbegin() const {
        return this->begin();
    }

    const_iterator cend() const {
        return this->end();
    }

    iterator iterator_to(reference ref) {
        return m_tree.iterator_to(ref);
    }

    const_iterator iterator_to(const_reference ref) const {
        return m_tree.iterator_to(ref);
    }

    // Content management.
    bool empty() const {
        return m_tree.empty();
    }

    reference back() {
        return m_tree.back();
    }

    const_reference back() const {
        return m_tree.back();
    }

    reference front() {
        return m_tree.front();
    }

    const_reference front() const {
        return m_tree.front();
    }

    iterator insert(reference ref) {
        return m_tree.insert(ref);
    }

    iterator erase(iterator it) {
        return m_tree.erase(it);
    }

    iterator find(const_reference ref) const {
        return m_tree.find(ref);
    }

    iterator nfind(const_reference ref) const {
        return m_tree.nfind(ref);
    }
};

} // namespace Kernel
