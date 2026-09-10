#pragma once

#include <utility>
#include <vector>

namespace Meridian::Common
{
    // Temporary view of the engine-owned linked list. All links are restored,
    // including during stack unwinding and nested dispatch. No nodes are copied.
    template <class Node>
    class ScopedInputFilter
    {
    public:
        template<class Predicate>
        ScopedInputFilter(Node* original, Predicate consumed) : m_head(original)
        {
            for(auto* node=original;node;node=node->next)
                m_links.emplace_back(node,node->next);
            std::vector<Node*> keep;
            for(auto [node,next]:m_links) {
                (void)next;
                if(!consumed(node)) keep.push_back(node);
            }
            // Evaluate/allocation finishes before editing links. A predicate
            // exception cannot leave an incompletely constructed list view.
            if(keep.size()==m_links.size()) return;
            Node* tail=nullptr;
            m_head=nullptr;
            for(auto* node:keep) {
                if(tail) tail->next=node; else m_head=node;
                tail=node;
            }
            if(tail) tail->next=nullptr;
        }
        ~ScopedInputFilter() { for(auto [node,next]:m_links) node->next=next; }
        ScopedInputFilter(const ScopedInputFilter&)=delete;
        ScopedInputFilter& operator=(const ScopedInputFilter&)=delete;
        Node* Head() const { return m_head; }
    private:
        Node* m_head;
        std::vector<std::pair<Node*,Node*>> m_links;
    };

    /// Selects the batch forwarded to an earlier-installed input hook. A batch
    /// already consumed by Meridian must be hidden before that hook can inspect
    /// it; otherwise the original pointer is preserved byte-for-byte.
    template <class InputList>
    constexpr InputList SelectForwardedInput(
        bool a_consumed,
        InputList a_original,
        InputList a_empty) noexcept
    {
        return a_consumed ? a_empty : a_original;
    }
}
