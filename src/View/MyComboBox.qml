import QtQuick 2.12
import QtQuick.Controls 2.12

ComboBox {
    id: root

    property color textColor: "white"        // 文字颜色
    property color bgColor: "#333333"        // 背景颜色
    property color borderColor: "#555555"    // 边框颜色
    property color highlightColor: "#666666" // 高亮选项颜色
    property color popupBgColor: "#444444"   // 下拉菜单背景颜色

    // **主显示文本**
    contentItem: Text {
        text: root.displayText   // 修复文本显示问题
        font: root.font
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // **主背景样式**
    background: Rectangle {
        color: root.bgColor
        border.color: root.borderColor
        radius: 4
    }

    delegate: ItemDelegate {
        width: root.width
        text: modelData  // 确保正确显示 model 数据
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            color: highlighted ? root.highlightColor : root.popupBgColor
        }
    }

}
