import QtQuick

// A compact one-line search box with its shortcut shown while empty.
Rectangle {
    id: field
    property alias text: input.text
    property string placeholder
    property string hint
    signal accepted()
    signal escaped()
    signal upPressed()
    signal downPressed()

    function selectAll() { input.selectAll() }
    function forceActiveFocus() { input.forceActiveFocus() }

    implicitHeight: 32
    radius: Math.min(4, theme.radius)
    color: Qt.alpha(theme.colors.background, 0.6)
    border.color: input.activeFocus ? theme.accent : theme.colors.border

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        color: theme.colors.foreground
        selectionColor: theme.colors.selection
        selectedTextColor: theme.colors.brightForeground
        font.pixelSize: 13
        clip: true
        selectByMouse: true
        Keys.onReturnPressed: field.accepted()
        Keys.onEnterPressed: field.accepted()
        Keys.onUpPressed: field.upPressed()
        Keys.onDownPressed: field.downPressed()
        Keys.onShortcutOverride: event => { if (event.key === Qt.Key_Escape) event.accepted = true }
        Keys.onEscapePressed: field.escaped()
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: hintText.visible ? hintText.left : parent.right
        anchors.rightMargin: hintText.visible ? 8 : 10
        visible: input.text.length === 0
        text: field.placeholder
        elide: Text.ElideRight
        color: theme.colors.mutedForeground
        font.pixelSize: 13
    }
    // The shortcut only shows when the placeholder fits beside it.
    TextMetrics { id: placeholderWidth; text: field.placeholder; font.pixelSize: 13 }
    Text {
        id: hintText
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 10
        visible: input.text.length === 0 && field.hint.length > 0
                 && field.width >= placeholderWidth.advanceWidth + implicitWidth + 28
        text: field.hint
        color: theme.colors.mutedForeground
        font.pixelSize: 11
    }
}
