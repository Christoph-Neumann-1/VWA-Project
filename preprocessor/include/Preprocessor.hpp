#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <unordered_map>
// I will figure this out later
namespace vwa
{
    class Preprocessor
    {
    public:
        struct Options
        {
            std::vector<std::filesystem::path> searchPath;
            std::vector<std::pair<std::string, std::string>> defines; // TODO upate for use with file
            uint64_t maxExpansionDepth = 100;
        };
        Preprocessor(Options options_) : options(options_) {}

        // TODO: decide how to handle file names/line numbers for inserted macros. Oh and implement file names in the first place
        struct File
        {
            struct Node
            {
                std::string str;
                int line = -1;
                // This is far from optimal, but who cares
                std::shared_ptr<std::string> fileName;
                Node *prev;
                std::unique_ptr<Node> next;
            };
            std::unique_ptr<Node> head{new Node{}};
            Node *m_end{head.get()};
            struct iterator
            {
                using iterator_category = std::bidirectional_iterator_tag;
                using value_type = Node;
                using difference_type = std::ptrdiff_t;
                using pointer = Node *;
                using reference = Node &;

                Node &operator*() const
                {
                    if (!current->next)
                        throw std::out_of_range("Cannot dereference end");
                    return *current;
                }
                Node *operator->() const
                {
                    if (!current->next)
                        throw std::out_of_range("Cannot dereference end");
                    return current;
                }
                iterator &operator++()
                {
                    // If the end of the file is reached, just keep returning end
                    current = current->next ? current->next.get() : current;
                    return *this;
                }
                iterator &operator--()
                {
                    current = current->prev;
                    if (current == nullptr)
                        throw std::out_of_range("File iterator out of range: Tried accessing line before beginning of file");
                    return *this;
                }
                iterator operator++(int)
                {
                    iterator tmp = *this;
                    ++*this;
                    return tmp;
                }
                iterator operator--(int)
                {
                    iterator tmp = *this;
                    --*this;
                    return tmp;
                }
                iterator operator+(int n) const
                {
                    iterator tmp = *this;
                    while (n--)
                        ++tmp;
                    return tmp;
                }
                iterator operator-(int n) const
                {
                    iterator tmp = *this;
                    while (n--)
                        --tmp;
                    return tmp;
                }
                iterator &operator+=(int n)
                {
                    while (n--)
                        ++*this;
                    return *this;
                }
                // Only accepts positive numbers
                iterator &operator-=(int n)
                {
                    while (n--)
                        --*this;
                    return *this;
                }
                iterator(Node *current_) : current(current_) {}
                iterator(const iterator &old) : current(old.current) {}
                iterator &operator=(const iterator &old)
                {
                    this->current = old.current;
                    return *this;
                }

                bool operator==(const iterator other) const { return current == other.current; }
                bool operator!=(const iterator other) const { return current != other.current; }

                Node *get() const { return current; }

            private:
                Node *current;
            };

            struct charIterator
            {
                iterator it;
                size_t pos;
                charIterator(iterator it_, size_t pos_ = 0) : it(it_), pos(pos_) {}
                charIterator &operator++()
                {
                    ++pos;
                    if (pos >= it->str.size())
                    {
                        ++it;
                        pos = 0;
                    }
                    return *this;
                }
                charIterator operator++(int)
                {
                    charIterator tmp = *this;
                    ++*this;
                    return tmp;
                }
                charIterator &operator+=(uint n)
                {
                    while (n--)
                        ++*this;
                    return *this;
                }
                charIterator operator+(uint n) const
                {
                    charIterator tmp = *this;
                    tmp += n;
                    return tmp;
                }
                char operator*() const { return it->str[pos]; }
            };

            void append(std::string str, int line = -1, std::shared_ptr<std::string> fileName = nullptr)
            {
                auto node = std::make_unique<Node>();
                node->str = std::move(str);
                node->line = line == -1 ? m_end->prev ? m_end->prev->line + 1 : 1 : line;
                node->fileName = fileName ? std::move(fileName) : head->fileName;
                node->prev = m_end->prev;
                node->next = std::move(node->prev ? node->prev->next : head);
                m_end->prev = node.get();
                *(node->prev ? &node->prev->next : &head) = std::move(node);
            }
            void append(iterator it)
            {
                auto node = std::make_unique<Node>();
                node->str = it->str;
                node->line = it->line;
                node->fileName = it->fileName;
                node->prev = m_end->prev;
                node->next = std::move(node->prev ? node->prev->next : head);
                node->next->prev = node.get();
                *(node->prev ? &node->prev->next : &head) = std::move(node);
            }
            void append(const Node &node)
            {
                auto node_ = std::make_unique<Node>();
                node_->str = node.str;
                node_->line = node.line;
                node_->fileName = node.fileName;
                node_->prev = m_end->prev;
                node_->next = std::move(node_->prev ? node_->prev->next : head);
                node_->next->prev = node_.get();
                *(node_->prev ? &node_->prev->next : &head) = std::move(node_);
            }
            void operator+=(std::string str) { append(str); }
            void operator+=(iterator it) { append(it); }
            void operator+=(const Node &node) { append(node); }

            std::string toString()
            {
                std::string ret;
                for (auto node = head.get(); node != nullptr; node = node->next.get())
                {
                    ret += node->str += '\n';
                }
                ret.pop_back(); // Removes the last newline
                return ret;
            }

            File(std::istream &input)
            {
                std::string line;
                while (std::getline(input, line))
                {
                    append(line);
                }
            }
            File(File &&other)
            {
                head = std::move(other.head);
                m_end = other.m_end;
                other.m_end = nullptr;
            }
            File &operator=(File &&other)
            {
                head = std::move(other.head);
                m_end = other.m_end;
                other.m_end = nullptr;
                return *this;
            }
            File() {}

            iterator begin() { return iterator(head.get()); }
            iterator end() { return iterator(m_end); }

            File copy(iterator begin, iterator end) const
            {
                File ret;
                while (begin != end)
                    ret += begin++;
                return ret;
            }
            // TODO: copy constructor for file and node
            //  Inserts a node after pos
            void insertAfter(iterator pos, iterator node)
            {
                if (!pos->next)
                {
                    append(node);
                    return;
                }
                auto n = std::make_unique<Node>();
                n->str = node->str;
                n->line = node->line;
                n->fileName = node->fileName;
                n->prev = pos.get();
                n->next = std::move(pos->next);
                n->next->prev = n.get();
                //.get() gives a raw pointer without checking if it is end. This is necessary since after moving the node would appear to be end
                pos.get()->next = std::move(n);
            }
            // TODO: insert before
            void insertAfter(iterator pos, iterator begin, iterator end)
            {
                while (begin != end)
                {
                    insertAfter(pos, begin++);
                    ++pos;
                }
            }
            void insertBefore(iterator pos, iterator node)
            {
                auto n = std::make_unique<Node>();
                n->prev = pos.get()->prev;
                n->next = std::move(n->prev ? pos->prev->next : head);
                n->str = node->str;
                n->line = node->line;
                n->fileName = node->fileName;
                pos.get()->prev = n.get();
                *(n->prev ? &n->prev->next : &head) = std::move(n);
            }
            void insertBefore(iterator pos, iterator begin, iterator end)
            {
                while (begin != end)
                    insertBefore(pos, begin++);
            }

            iterator erase(iterator pos)
            {
                if (pos.get() == m_end)
                    return m_end;
                iterator ret{pos + 1};
                pos->next->prev = pos->prev;
                if (pos->prev)
                    pos->prev->next = std::move(pos->next);
                else
                    head = std::move(pos->next);
                return ret;
            }
            iterator erase(iterator begin, iterator end)
            {
                while (begin != end)
                    begin = erase(begin);
                return begin;
            }
        };
        File process(std::istream &input);

    private:
        const Options options;
    };
}
