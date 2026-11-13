import QtQuick
import QtQuick.Controls

// A one-pixel line between panes with a wider grip, so it's easy to drag.
Rectangle {
    id: divider
    implicitWidth: 1
    implicitHeight: 1
    color: SplitHandle.pressed || SplitHandle.hovered ? theme.accent : theme.colors.border

    containmentMask: Item {
        width: Math.max(divider.width, 9)
        height: Math.max(divider.height, 9)
        x: (divider.width - width) / 2
        y: (divider.height - height) / 2
    }
}
