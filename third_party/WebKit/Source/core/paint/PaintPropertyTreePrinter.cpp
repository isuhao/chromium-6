// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreePrinter.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"

#include <iomanip>
#include <sstream>

#ifndef NDEBUG

namespace blink {
namespace {

template <typename PropertyTreeNode>
class PropertyTreePrinterTraits;

template <typename PropertyTreeNode>
class PropertyTreePrinter {
public:
    void showTree(const FrameView& frameView)
    {
        if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
            LOG(ERROR) << "This is for slimmingPaintV2 only";
            return;
        }
        collectPropertyNodes(frameView);
        showAllPropertyNodes(nullptr);
    }

    void showPath(const PropertyTreeNode* node)
    {
        for (const PropertyTreeNode* n = node; n; n = n->parent())
            addPropertyNode(n, "");
        showAllPropertyNodes(nullptr);
    }

    void addPropertyNode(const PropertyTreeNode* node, String debugInfo)
    {
        m_nodeToDebugString.set(node, debugInfo);
    }

private:
    using Traits = PropertyTreePrinterTraits<PropertyTreeNode>;

    void collectPropertyNodes(const FrameView& frameView)
    {
        Traits::addFrameViewProperties(frameView, *this);
        if (LayoutView* layoutView = frameView.layoutView())
            collectPropertyNodes(*layoutView);
        for (Frame* child = frameView.frame().tree().firstChild(); child; child = child->tree().nextSibling()) {
            if (!child->isLocalFrame())
                continue;
            if (FrameView* childView = toLocalFrame(child)->view())
                collectPropertyNodes(*childView);
        }
    }

    void collectPropertyNodes(const LayoutObject& object)
    {
        if (const ObjectPaintProperties* paintProperties = object.objectPaintProperties())
            Traits::addObjectPaintProperties(object, *paintProperties, *this);
        for (LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling())
            collectPropertyNodes(*child);
    }

    void showAllPropertyNodes(const PropertyTreeNode* node, unsigned indent = 0)
    {
        if (node) {
            StringBuilder stringBuilder;
            for (unsigned i = 0; i < indent; i++)
                stringBuilder.append(' ');
            if (m_nodeToDebugString.contains(node))
                stringBuilder.append(m_nodeToDebugString.get(node));
            stringBuilder.append(String::format(" %p", node));
            Traits::printNodeAsString(node, stringBuilder);
            fprintf(stderr, "%s\n", stringBuilder.toString().ascii().data());
        }

        for (const auto* childNode : m_nodeToDebugString.keys()) {
            if (childNode->parent() == node)
                showAllPropertyNodes(childNode, indent + 2);
        }
    }

    HashMap<const PropertyTreeNode*, String> m_nodeToDebugString;
};

template <>
class PropertyTreePrinterTraits<TransformPaintPropertyNode> {
public:
    static void addFrameViewProperties(const FrameView& frameView, PropertyTreePrinter<TransformPaintPropertyNode>& printer)
    {
        if (const TransformPaintPropertyNode* rootTransform = frameView.rootTransform())
            printer.addPropertyNode(rootTransform, "RootTransform (FrameView)");
        if (const TransformPaintPropertyNode* preTranslation = frameView.preTranslation())
            printer.addPropertyNode(preTranslation, "PreTranslation (FrameView)");
        if (const TransformPaintPropertyNode* scrollTranslation = frameView.scrollTranslation())
            printer.addPropertyNode(scrollTranslation, "ScrollTranslation (FrameView)");
    }

    static void addObjectPaintProperties(const LayoutObject& object, const ObjectPaintProperties& paintProperties, PropertyTreePrinter<TransformPaintPropertyNode>& printer)
    {
        if (const TransformPaintPropertyNode* paintOffsetTranslation = paintProperties.paintOffsetTranslation())
            printer.addPropertyNode(paintOffsetTranslation, "PaintOffsetTranslation (" + object.debugName() + ")");
        if (const TransformPaintPropertyNode* transform = paintProperties.transform())
            printer.addPropertyNode(transform, "Transform (" + object.debugName() + ")");
        if (const TransformPaintPropertyNode* perspective = paintProperties.perspective())
            printer.addPropertyNode(perspective, "Perspective (" + object.debugName() + ")");
        if (const TransformPaintPropertyNode* svgLocalToBorderBoxTransform = paintProperties.svgLocalToBorderBoxTransform())
            printer.addPropertyNode(svgLocalToBorderBoxTransform, "SvgLocalToBorderBoxTransform (" + object.debugName() + ")");
        if (const TransformPaintPropertyNode* scrollTranslation = paintProperties.scrollTranslation())
            printer.addPropertyNode(scrollTranslation, "ScrollTranslation (" + object.debugName() + ")");
        if (const TransformPaintPropertyNode* scrollbarPaintOffset = paintProperties.scrollbarPaintOffset())
            printer.addPropertyNode(scrollbarPaintOffset, "ScrollbarPaintOffset (" + object.debugName() + ")");
    }

    static void printNodeAsString(const TransformPaintPropertyNode* node, StringBuilder& stringBuilder)
    {
        stringBuilder.append(" transform=");

        TransformationMatrix::DecomposedType decomposition;
        if (!node->matrix().decompose(decomposition)) {
            stringBuilder.append("degenerate");
            return;
        }

        stringBuilder.append(String::format("translation=%f,%f,%f", decomposition.translateX, decomposition.translateY, decomposition.translateZ));
        if (node->matrix().isIdentityOrTranslation())
            return;

        stringBuilder.append(String::format(", scale=%f,%f,%f", decomposition.scaleX, decomposition.scaleY, decomposition.scaleZ));
        stringBuilder.append(String::format(", skew=%f,%f,%f", decomposition.skewXY, decomposition.skewXZ, decomposition.skewYZ));
        stringBuilder.append(String::format(", quaternion=%f,%f,%f,%f", decomposition.quaternionX, decomposition.quaternionY, decomposition.quaternionZ, decomposition.quaternionW));
        stringBuilder.append(String::format(", perspective=%f,%f,%f,%f", decomposition.perspectiveX, decomposition.perspectiveY, decomposition.perspectiveZ, decomposition.perspectiveW));
    }
};

template <>
class PropertyTreePrinterTraits<ClipPaintPropertyNode> {
public:
    static void addFrameViewProperties(const FrameView& frameView, PropertyTreePrinter<ClipPaintPropertyNode>& printer)
    {
        if (const ClipPaintPropertyNode* rootClip = frameView.rootClip())
            printer.addPropertyNode(rootClip, "RootClip (FrameView)");
        if (const ClipPaintPropertyNode* contentClip = frameView.contentClip())
            printer.addPropertyNode(contentClip, "ContentClip (FrameView)");
    }

    static void addObjectPaintProperties(const LayoutObject& object, const ObjectPaintProperties& paintProperties, PropertyTreePrinter<ClipPaintPropertyNode>& printer)
    {
        if (const ClipPaintPropertyNode* cssClip = paintProperties.cssClip())
            printer.addPropertyNode(cssClip, "CssClip (" + object.debugName() + ")");
        if (const ClipPaintPropertyNode* cssClipFixedPosition = paintProperties.cssClipFixedPosition())
            printer.addPropertyNode(cssClipFixedPosition, "CssClipFixedPosition (" + object.debugName() + ")");
        if (const ClipPaintPropertyNode* overflowClip = paintProperties.overflowClip())
            printer.addPropertyNode(overflowClip, "OverflowClip (" + object.debugName() + ")");
    }

    static void printNodeAsString(const ClipPaintPropertyNode* node, StringBuilder& stringBuilder)
    {
        stringBuilder.append(String::format(" localTransformSpace=%p ", node->localTransformSpace()));
        stringBuilder.append(String::format("rect=%f,%f,%f,%f",
            node->clipRect().rect().x(), node->clipRect().rect().y(),
            node->clipRect().rect().width(), node->clipRect().rect().height()));
    }
};

template <>
class PropertyTreePrinterTraits<EffectPaintPropertyNode> {
public:
    static void addFrameViewProperties(const FrameView& frameView, PropertyTreePrinter<EffectPaintPropertyNode>& printer)
    {
        if (const EffectPaintPropertyNode* rootEffect = frameView.rootEffect())
            printer.addPropertyNode(rootEffect, "RootEffect (FrameView)");
    }

    static void addObjectPaintProperties(const LayoutObject& object, const ObjectPaintProperties& paintProperties, PropertyTreePrinter<EffectPaintPropertyNode>& printer)
    {
        if (const EffectPaintPropertyNode* effect = paintProperties.effect())
            printer.addPropertyNode(effect, "Effect (" + object.debugName() + ")");
    }

    static void printNodeAsString(const EffectPaintPropertyNode* node, StringBuilder& stringBuilder)
    {
        stringBuilder.append(String::format(" opacity=%f", node->opacity()));
    }
};

class PaintPropertyTreeGraphBuilder {
public:
    PaintPropertyTreeGraphBuilder() { }

    void generateTreeGraph(const FrameView& frameView, StringBuilder& stringBuilder)
    {
        m_layout.str("");
        m_properties.str("");
        m_ownerEdges.str("");
        m_properties << std::setprecision(2) << std::setiosflags(std::ios_base::fixed);

        writeFrameViewNode(frameView, nullptr);

        stringBuilder.append("digraph {\n");
        stringBuilder.append("graph [rankdir=BT];\n");
        stringBuilder.append("subgraph cluster_layout {\n");
        std::string layoutStr = m_layout.str();
        stringBuilder.append(layoutStr.c_str(), layoutStr.length());
        stringBuilder.append("}\n");
        stringBuilder.append("subgraph cluster_properties {\n");
        std::string propertiesStr = m_properties.str();
        stringBuilder.append(propertiesStr.c_str(), propertiesStr.length());
        stringBuilder.append("}\n");
        std::string ownersStr = m_ownerEdges.str();
        stringBuilder.append(ownersStr.c_str(), ownersStr.length());
        stringBuilder.append("}\n");
    }

private:
    static String getTagName(Node* n)
    {
        if (n->isDocumentNode())
            return "";
        if (n->getNodeType() == Node::kCommentNode)
            return "COMMENT";
        return n->nodeName();
    }

    static void writeParentEdge(const void* node, const void* parent, const char* color, std::ostream& os)
    {
        os << "n" << node << " -> " << "n" << parent;
        if (color)
            os << " [color=" << color << "]";
        os << ";" << std::endl;
    }

    void writeOwnerEdge(const void* node, const void* owner)
    {
        std::ostream& os = m_ownerEdges;
        os << "n" << owner << " -> " << "n" << node << " [color=" << s_layoutNodeColor << ", arrowhead=dot, constraint=false];" << std::endl;
    }

    void writeTransformEdge(const void* node, const void* transform)
    {
        std::ostream& os = m_properties;
        os << "n" << node << " -> " << "n" << transform << " [color=" << s_transformNodeColor << "];" << std::endl;
    }

    void writePaintPropertyNode(const TransformPaintPropertyNode& node, const void* owner, const char* label)
    {
        std::ostream& os = m_properties;
        os << "n" << &node
            << " [color=" << s_transformNodeColor
            << ", fontcolor=" << s_transformNodeColor
            << ", shape=box, label=\"" << label << "\\n";

        const FloatPoint3D& origin = node.origin();
        os << "origin=(";
        os << origin.x() << "," << origin.y() << "," << origin.z() << ")\\n";

        os << "flattensInheritedTransform=" << (node.flattensInheritedTransform() ? "true" : "false") << "\\n";
        os << "renderingContextID=" << node.renderingContextID() << "\\n";

        const TransformationMatrix& matrix = node.matrix();
        os << "[" << std::setw(8) << matrix.m11() << "," << std::setw(8) << matrix.m12() << "," << std::setw(8) << matrix.m13() << "," << std::setw(8) << matrix.m14() << "\\n";
        os << std::setw(9) << matrix.m21() << "," << std::setw(8) << matrix.m22() << "," << std::setw(8) << matrix.m23() << "," << std::setw(8) << matrix.m24() << "\\n";
        os << std::setw(9) << matrix.m31() << "," << std::setw(8) << matrix.m32() << "," << std::setw(8) << matrix.m33() << "," << std::setw(8) << matrix.m34() << "\\n";
        os << std::setw(9) << matrix.m41() << "," << std::setw(8) << matrix.m42() << "," << std::setw(8) << matrix.m43() << "," << std::setw(8) << matrix.m44() << "]";

        TransformationMatrix::DecomposedType decomposition;
        if (!node.matrix().decompose(decomposition))
            os << "\\n(degenerate)";

        os << "\"];" << std::endl;

        if (owner)
            writeOwnerEdge(&node, owner);
        if (node.parent())
            writeParentEdge(&node, node.parent(), s_transformNodeColor, os);
    }

    void writePaintPropertyNode(const ClipPaintPropertyNode& node, const void* owner, const char* label)
    {
        std::ostream& os = m_properties;
        os << "n" << &node
            << " [color=" << s_clipNodeColor
            << ", fontcolor=" << s_clipNodeColor
            << ", shape=box, label=\"" << label << "\\n";

        LayoutRect rect(node.clipRect().rect());
        if (IntRect(rect) == LayoutRect::infiniteIntRect())
            os << "(infinite)";
        else
            os << "(" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
        os << "\"];" << std::endl;

        if (owner)
            writeOwnerEdge(&node, owner);
        if (node.parent())
            writeParentEdge(&node, node.parent(), s_clipNodeColor, os);
        if (node.localTransformSpace())
            writeTransformEdge(&node, node.localTransformSpace());
    }

    void writePaintPropertyNode(const EffectPaintPropertyNode& node, const void* owner, const char* label)
    {
        std::ostream& os = m_properties;
        os << "n" << &node << " [shape=diamond, label=\"" << label << "\\n";
        os << "opacity=" << node.opacity() << "\"];" << std::endl;

        if (owner)
            writeOwnerEdge(&node, owner);
        if (node.parent())
            writeParentEdge(&node, node.parent(), s_effectNodeColor, os);
    }

    void writeObjectPaintPropertyNodes(const LayoutObject& object)
    {
        const ObjectPaintProperties* properties = object.objectPaintProperties();
        if (!properties)
            return;
        const TransformPaintPropertyNode* paintOffset = properties->paintOffsetTranslation();
        if (paintOffset)
            writePaintPropertyNode(*paintOffset, &object, "paintOffset");
        const TransformPaintPropertyNode* transform = properties->transform();
        if (transform)
            writePaintPropertyNode(*transform, &object, "transform");
        const TransformPaintPropertyNode* perspective = properties->perspective();
        if (perspective)
            writePaintPropertyNode(*perspective, &object, "perspective");
        const TransformPaintPropertyNode* svgLocalToBorderBox = properties->svgLocalToBorderBoxTransform();
        if (svgLocalToBorderBox)
            writePaintPropertyNode(*svgLocalToBorderBox, &object, "svgLocalToBorderBox");
        const TransformPaintPropertyNode* scrollTranslation = properties->scrollTranslation();
        if (scrollTranslation)
            writePaintPropertyNode(*scrollTranslation, &object, "scrollTranslation");
        const TransformPaintPropertyNode* scrollbarPaintOffset = properties->scrollbarPaintOffset();
        if (scrollbarPaintOffset)
            writePaintPropertyNode(*scrollbarPaintOffset, &object, "scrollbarPaintOffset");
        const EffectPaintPropertyNode* effect = properties->effect();
        if (effect)
            writePaintPropertyNode(*effect, &object, "effect");
        const ClipPaintPropertyNode* cssClip = properties->cssClip();
        if (cssClip)
            writePaintPropertyNode(*cssClip, &object, "cssClip");
        const ClipPaintPropertyNode* cssClipFixedPosition = properties->cssClipFixedPosition();
        if (cssClipFixedPosition)
            writePaintPropertyNode(*cssClipFixedPosition, &object, "cssClipFixedPosition");
        const ClipPaintPropertyNode* overflowClip = properties->overflowClip();
        if (overflowClip) {
            writePaintPropertyNode(*overflowClip, &object, "overflowClip");
            if (object.isLayoutView() && overflowClip->parent())
                writePaintPropertyNode(*overflowClip->parent(), nullptr, "rootClip");
        }
    }

    void writeFrameViewPaintPropertyNodes(const FrameView& frameView)
    {
        TransformPaintPropertyNode* rootTransform = frameView.rootTransform();
        if (rootTransform)
            writePaintPropertyNode(*rootTransform, &frameView, "rootTransform");
        ClipPaintPropertyNode* rootClip = frameView.rootClip();
        if (rootClip)
            writePaintPropertyNode(*rootClip, &frameView, "rootClip");
        EffectPaintPropertyNode* rootEffect = frameView.rootEffect();
        if (rootEffect)
            writePaintPropertyNode(*rootEffect, &frameView, "rootEffect");
        TransformPaintPropertyNode* preTranslation = frameView.preTranslation();
        if (preTranslation)
            writePaintPropertyNode(*preTranslation, &frameView, "preTranslation");
        TransformPaintPropertyNode* scrollTranslation = frameView.scrollTranslation();
        if (scrollTranslation)
            writePaintPropertyNode(*scrollTranslation, &frameView, "scrollTranslation");
        ClipPaintPropertyNode* contentClip = frameView.contentClip();
        if (contentClip)
            writePaintPropertyNode(*contentClip, &frameView, "contentClip");
    }

    void writeLayoutObjectNode(const LayoutObject& object)
    {
        std::ostream& os = m_layout;
        os << "n" << &object
            << " [color=" << s_layoutNodeColor
            << ", fontcolor=" << s_layoutNodeColor
            << ", label=\"" << object.name();
        Node* node = object.node();
        if (node) {
            os << "\\n" << getTagName(node).utf8().data();
            if (node->isElementNode() && toElement(node)->hasID())
                os << "\\nid=" << toElement(node)->getIdAttribute().utf8().data();
        }
        os << "\"];" << std::endl;
        const void* parent = object.isLayoutView() ? (const void*) toLayoutView(object).frameView() : (const void*) object.parent();
        writeParentEdge(&object, parent, s_layoutNodeColor, os);
        writeObjectPaintPropertyNodes(object);
        for (const LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling())
            writeLayoutObjectNode(*child);
        if (object.isLayoutPart() && toLayoutPart(object).widget() && toLayoutPart(object).widget()->isFrameView()) {
            FrameView* frameView = static_cast<FrameView*>(toLayoutPart(object).widget());
            writeFrameViewNode(*frameView, &object);
        }
    }

    void writeFrameViewNode(const FrameView& frameView, const void* parent)
    {
        std::ostream& os = m_layout;
        os << "n" << &frameView
            << " [color=" << s_layoutNodeColor
            << ", fontcolor=" << s_layoutNodeColor
            << ", shape=doublecircle"
            << ", label=FrameView];" << std::endl;

        writeFrameViewPaintPropertyNodes(frameView);
        if (parent)
            writeParentEdge(&frameView, parent, s_layoutNodeColor, os);
        LayoutView* layoutView = frameView.layoutView();
        if (layoutView)
            writeLayoutObjectNode(*layoutView);
    }

private:
    static const char* s_layoutNodeColor;
    static const char* s_transformNodeColor;
    static const char* s_clipNodeColor;
    static const char* s_effectNodeColor;

    std::ostringstream m_layout;
    std::ostringstream m_properties;
    std::ostringstream m_ownerEdges;
};

const char* PaintPropertyTreeGraphBuilder::s_layoutNodeColor = "black";
const char* PaintPropertyTreeGraphBuilder::s_transformNodeColor = "green";
const char* PaintPropertyTreeGraphBuilder::s_clipNodeColor = "blue";
const char* PaintPropertyTreeGraphBuilder::s_effectNodeColor = "black";

} // namespace {
} // namespace blink

void showTransformPropertyTree(const blink::FrameView& rootFrame)
{
    blink::PropertyTreePrinter<blink::TransformPaintPropertyNode>().showTree(rootFrame);
}

void showClipPropertyTree(const blink::FrameView& rootFrame)
{
    blink::PropertyTreePrinter<blink::ClipPaintPropertyNode>().showTree(rootFrame);
}

void showEffectPropertyTree(const blink::FrameView& rootFrame)
{
    blink::PropertyTreePrinter<blink::EffectPaintPropertyNode>().showTree(rootFrame);
}

void showPaintPropertyPath(const blink::TransformPaintPropertyNode* node)
{
    blink::PropertyTreePrinter<blink::TransformPaintPropertyNode>().showPath(node);
}

void showPaintPropertyPath(const blink::ClipPaintPropertyNode* node)
{
    blink::PropertyTreePrinter<blink::ClipPaintPropertyNode>().showPath(node);
}

void showPaintPropertyPath(const blink::EffectPaintPropertyNode* node)
{
    blink::PropertyTreePrinter<blink::EffectPaintPropertyNode>().showPath(node);
}

String paintPropertyTreeGraph(const blink::FrameView& frameView)
{
    blink::PaintPropertyTreeGraphBuilder builder;
    StringBuilder stringBuilder;
    builder.generateTreeGraph(frameView, stringBuilder);
    return stringBuilder.toString();
}

#endif
