#pragma once

#include "Util.h"
#include <deque>
#include <map>
#include <set>
#include <vector>

namespace souffle {

namespace index {

/** A class mapping an object to an index. */
template <typename Object>
class ObjectToIndex {
protected:
    /** The table mapping from an object to an index. */
    std::map<Object, size_t> objectToIndex;

public:
    /** Check if there is an index for the given object. */
    const bool has(const Object& object) const {
        return objectToIndex.find(object) != objectToIndex.end();
    }

    /** Get the index for the given object. */
    const size_t getIndex(const Object& object) const {
        assert(this->has(object));
        return objectToIndex.at(object);
    }

    /** Set the index for the given object. */
    virtual void setIndex(const Object& object, const size_t index) {
        objectToIndex[object] = index;
    }

    /** Remove the index for the given object. */
    virtual void removeIndex(const Object& object) {
        if (!this->has(object)) return;
        objectToIndex.erase(object);
    }
};

/** A class mapping an index to an object. */
template <typename Object>
class IndexToObject {
protected:
    /** The table mapping from an index to an object. */
    std::vector<Object> indexToObject;

public:
    /** Check if there is an object for the given index. */
    const bool hasIndex(const size_t index) const {
        return index >= 0 && index < indexToObject.size() && indexToObject.at(index) != nullptr;
    }

    /** Get the object for the given index. */
    const Object& get(const size_t index) const {
        assert(this->hasIndex(index));
        return indexToObject.at(index);
    }

    /* Set the object for the given index. */
    virtual void set(const size_t index, const Object& object) {
        assert(index <= indexToObject.size());
        if (index == indexToObject.size())
            indexToObject.push_back(object);
        else
            indexToObject[index] = object;
    }

    /* Remove the object for the given index. */
    virtual void remove(const size_t index) {
        if (!this->hasIndex(index)) return;
        indexToObject[index] = nullptr;
        while (indexToObject[indexToObject.size() - 1] == nullptr)
            indexToObject.erase(indexToObject.end() - 1);
    }
};

/** A class mapping an index to a collection of objects. */
template <typename Object, template <typename...> class Container>
class IndexToObjects {
private:
    /** A set of indices pending erasure. */
    std::set<size_t, std::greater<size_t>> pending;

protected:
    /** The table mapping from an index to a collection of objects. */
    std::vector<Container<Object>> indexToObject;

public:
    /** Check if there is a collection of objects for the given index. */
    const bool hasIndex(const size_t index) const {
        return index >= 0 && index < indexToObject.size();
    }

    /** Get the collection of objects for the given index. */
    const Container<Object>& get(const size_t index) const {
        this->hasIndex(index);
        return indexToObject.at(index);
    }

    /* Set the collection of objects for the given index. */
    virtual void set(const size_t index, const Container<Object>& objects = Container<Object>()) {
        assert(index <= indexToObject.size());
        if (index == indexToObject.size())
            indexToObject.push_back(objects);
        else
            indexToObject[index] = objects;
    }

    /* Remove the collection of objects for the given index. */
    virtual void remove(const size_t index) {
        if (!this->hasIndex(index)) return;
        indexToObject.at(index).clear();
        pending.insert(index);
        for (const size_t current : pending) {
            if (current != indexToObject.size() - 1) return;
            indexToObject.erase(indexToObject.begin() + current);
        }
    }
};

/** A class mapping between index and an object. */
template <typename Object>
class IndexTable : ObjectToIndex<Object>, IndexToObject<Object> {
public:
    /** Set the index for the given object. */
    virtual void setIndex(const Object& object, const size_t index) {
        ObjectToIndex<Object>::setIndex(object, index);
        IndexToObject<Object>::set(index, object);
    }

    /* Set the object for the given index. */
    virtual void set(const size_t index, const Object& object) {
        IndexToObject<Object>::set(index, object);
        ObjectToIndex<Object>::setIndex(index, object);
    }

    /** Remove the index for the given object. */
    virtual void removeIndex(const Object& object) {
        size_t index = this->getIndex(object);
        ObjectToIndex<Object>::removeIndex(object);
        IndexToObject<Object>::remove(index);
    }

    /* Remove the object for the given index. */
    virtual void remove(const size_t index) {
        const Object& object = this->get(object);
        IndexToObject<Object>::remove(index);
        ObjectToIndex<Object>::removeIndex(object);
    }
};

/** An abstract class mapping between index and a collection of objects. */
template <typename Object, template <typename...> class Container>
class CollectionIndexTable : public ObjectToIndex<Object>, public IndexToObjects<Object, Container> {
public:
    /** Set the index for the given object. */
    void setIndex(const Object& object, const size_t index) {
        assert(index <= this->indexToObject.size());
        if (this->hasIndex(index))
            for (const Object& object : this->get(index)) ObjectToIndex<Object>::removeIndex(object);
        else
            this->indexToObject.push_back(Container<Object>());
        this->append(index, object);
    }

    /** Set the collection of objects for the given index. */
    void set(const size_t index, const Container<Object>& objects = Container<Object>()) {
        for (const Object& object : objects) ObjectToIndex<Object>::setIndex(object, index);
        IndexToObjects<Object, Container>::set(index, objects);
    }

    /** Remove the index for the given object. */
    void removeIndex(const Object& object) {
        size_t index = this->getIndex(object);
        ObjectToIndex<Object>::removeIndex(object);
        IndexToObjects<Object, Container>::remove(index);
    }

    /* Remove the collection of objects for the given index. */
    void remove(const size_t index) {
        const Container<Object> objects = this->get(index);
        IndexToObjects<Object, Container>::remove(index);
        for (const Object& object : objects) ObjectToIndex<Object>::removeIndex(object);
    }

    /** Append an object to the collection at the given index. */
    virtual void append(const size_t index, const Object& object) = 0;

    /** Append the collection of objects to the collection at the given index. */
    template <template <typename...> class T>
    void append(const size_t index, const T<Object>& objects = T<Object>()) {
        if (objects.empty) this->append(index, T<Object>());
        for (const auto& object : objects) this->append(index, object);
    }

    /** Move the collection of objects at the 'from' index, appending them to the collection at the 'to'
     * index. */
    void moveAppend(const size_t fromIndex, const size_t toIndex) {
        if (!this->hasIndex(fromIndex)) return;
        assert(this->hasIndex(toIndex));
        for (auto object : this->indexToObject.at(fromIndex)) this->append(toIndex, object);
        this->remove(fromIndex);
    }

    /** Prepend an object to the collection at the given index. */
    virtual void prepend(const size_t index, const Object& object) = 0;

    /** Prepend the collection of objects to the collection at the given index. */
    template <template <typename...> class T>
    void prepend(const size_t index, const T<Object>& objects = T<Object>()) {
        if (objects.empty) this->prepend(index, T<Object>());
        for (const auto& object : objects) this->prepend(index, object);
    }

    /** Move the collection of objects at the 'from' index, prepending them to the collection at the 'to'
     * index. */
    void movePrepend(const size_t fromIndex, const size_t toIndex) {
        if (!this->hasIndex(fromIndex)) return;
        assert(this->hasIndex(toIndex));
        for (auto object : this->indexToObject.at(fromIndex)) this->prepend(toIndex, object);
        this->remove(fromIndex);
    }
};

/** A class mapping between index and a set of objects. */
template <typename Object>
class SetTable : public CollectionIndexTable<Object, std::set> {
public:
    /** Insert the object into the set at the given index. */
    void append(const size_t index, const Object& object) {
        assert(index <= this->indexToObject.size());
        if (index == this->indexToObject.size()) this->indexToObject.push_back(std::set<Object>());
        this->indexToObject[index].insert(object);
        this->objectToIndex[object] = index;
    }

    /** Insert the object into the set at the given index. */
    void prepend(const size_t index, const Object& object) {
        this->append(index, object);
    }
};

/** A class mapping between index and a sequence of objects. */
template <typename Object>
class SeqTable : public CollectionIndexTable<Object, std::deque> {
public:
    /** Append the object to the sequence at the given index. */
    void append(const size_t index, const Object& object) {
        assert(index <= this->indexToObject.size());
        if (index == this->indexToObject.size()) this->indexToObject.push_back(std::deque<Object>());
        this->indexToObject[index].push_back(object);
        this->objectToIndex[object] = index;
    }

    /** Prepend the object to the sequence at the given index. */
    void prepend(const size_t index, const Object& object) {
        assert(index <= this->indexToObject.size());
        if (index == this->indexToObject.size()) this->indexToObject.push_back(std::deque<Object>());
        this->indexToObject[index].push_front(object);
        this->objectToIndex[object] = index;
    }
};
}
}