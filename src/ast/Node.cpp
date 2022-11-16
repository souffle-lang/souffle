/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */
#include "ast/Node.h"
#include <utility>

namespace souffle::ast {
Node::Node(NodeKind kind, SrcLocation loc) : Kind(kind), location(std::move(loc)) {}

/** Set source location for the Node */
void Node::setSrcLoc(SrcLocation l) {
    location = std::move(l);
}

bool Node::operator==(const Node& other) const {
    if (this == &other) {
        return true;
    }

    if constexpr (/* we ignore annotations in equality test */ false) {
        const bool has_annotes = ((bool)annotations) == true && !annotations->empty();
        const bool other_has_annotes = ((bool)other.annotations) == true && !other.annotations->empty();
        if (has_annotes != other_has_annotes) {
            return false;
        }

        if (has_annotes && (*annotations != *(other.annotations))) {
            return false;
        }
    }

    return this->Kind == other.Kind && equal(other);
}

Own<Node> Node::cloneImpl() const {
    auto Res = Own<Node>(cloning());
    Res->setAnnotationsFrom(*this);
    return Res;
}

/** Apply the mapper to all child nodes */
void Node::apply(const NodeMapper& /* mapper */) {}

Node::ConstChildNodes Node::getChildNodes() const {
    return ConstChildNodes(getChildren(), detail::RefCaster());
}

Node::ChildNodes Node::getChildNodes() {
    return ChildNodes(getChildren(), detail::ConstCaster());
}

std::ostream& operator<<(std::ostream& out, const Node& node) {
    node.print(out);
    return out;
}

// bool Node::equal(const Node& other) const {
//     return this == &other;
// }

Node::NodeVec Node::getChildren() const {
    return {};
}

Node::NodeKind Node::getKind() const {
    return Kind;
}

const SrcLocation& Node::getSrcLoc() const {
    return location;
}

std::string Node::extloc() const {
    return location.extloc();
}

bool Node::operator!=(const Node& other) const {
    return !(*this == other);
}

AnnotationList& Node::ensureAnnotations() {
    if (!annotations) {
        annotations = std::make_unique<AnnotationList>();
    }
    return *annotations;
}

void Node::addAnnotation(Annotation annote) {
    ensureAnnotations().emplace_back(std::move(annote));
}

void Node::addAnnotations(AnnotationList annotes) {
    auto& annotations = ensureAnnotations();
    annotations.splice(annotations.end(), annotes);
}

void Node::prependAnnotation(Annotation annote) {
    ensureAnnotations().emplace_front(std::move(annote));
}

void Node::prependAnnotations(AnnotationList annotes) {
    auto& annotations = ensureAnnotations();
    annotations.splice(annotations.begin(), annotes);
}

void Node::setAnnotationsFrom(const Node& other) {
    if (other.annotations && !other.annotations->empty()) {
        annotations = std::make_unique<AnnotationList>(*other.annotations);
    } else {
        annotations.release();
    }
}

void Node::setAnnotations(AnnotationList annotes) {
    if (!annotes.empty()) {
        annotations = std::make_unique<AnnotationList>(std::move(annotes));
    } else {
        annotations.release();
    }
}

void Node::setAnnotations(std::unique_ptr<AnnotationList> annotes) {
    if (annotes && !annotes->empty()) {
        annotations = std::move(annotes);
    } else {
        annotations.release();
    }
}

void Node::stealAnnotationsFrom(Node& other) {
    annotations = std::move(other.annotations);
}

void Node::eachAnnotation(const std::function<void(const Annotation&)>& f) const {
    if (!annotations) {
        return;
    }

    for (const auto& annote : *annotations) {
        f(annote);
    }
}

void Node::eachAnnotation(
        const QualifiedName& label, const std::function<void(const TokenStream&)>& f) const {
    if (!annotations) {
        return;
    }

    for (const auto& annote : *annotations) {
        if (annote.getLabel() == label) {
            f(annote.getTokens());
        }
    }
}

void Node::eachAnnotation(const QualifiedName& label, const std::function<void(const Annotation&)>& f) const {
    if (!annotations) {
        return;
    }

    for (const auto& annote : *annotations) {
        if (annote.getLabel() == label) {
            f(annote);
        }
    }
}

void Node::eachAnnotation(const std::function<void(const QualifiedName&, const TokenStream&)>& f) const {
    if (!annotations) {
        return;
    }

    for (const auto& annote : *annotations) {
        f(annote.getLabel(), annote.getTokens());
    }
}

std::size_t Node::countAnnotations(const QualifiedName& label) const {
    if (!annotations) {
        return 0;
    }

    return std::count_if(annotations->cbegin(), annotations->cend(),
            [&](const Annotation& a) -> bool { return a.getLabel() == label; });
}

const Annotation& Node::getAnnotation(const QualifiedName& label) const {
    if (!annotations) {
        throw std::out_of_range("No such annotation");
    }

    auto it = std::find_if(annotations->cbegin(), annotations->cend(),
            [&](const Annotation& a) -> bool { return a.getLabel() == label; });
    if (it == annotations->cend()) {
        throw std::out_of_range("No such annotation");
    }

    return *it;
}

AnnotationList* Node::getAnnotations() {
    if (annotations) {
        return annotations.get();
    } else {
        return nullptr;
    }
}

const AnnotationList* Node::getAnnotations() const {
    if (annotations) {
        return annotations.get();
    } else {
        return nullptr;
    }
}

/// print annotations of this node, except documentation
void Node::printAnnotations(std::ostream& os) const {
    eachAnnotation([&](const Annotation& annotation) {
        if (annotation.getKind() == Annotation::Kind::DocComment) {
            return;
        }
        annotation.printAsOuter(os);
        os << "\n";
    });
}

}  // namespace souffle::ast
