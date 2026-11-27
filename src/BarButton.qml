import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A compact button with its shortcut beside the label.
Rectangle {
    id: button
    property string text
    property string hint
    property bool primary: false
    // Leaves the shortcut out where space is short, so labels never overlap.
    property bool compact: false
    signal clicked()

    implicitWidth: row.implicitWidth + 20
    Layout.minimumWidth: implicitWidth
    implicitHeight: 30
    radius: Math.min(4, theme.radius)
    color: primary ? (hover.hovered ? Qt.lighter(theme.accent, 1.1) : theme.accent)
                   : (hover.hovered ? theme.colors.lighterBackground : "transparent")
    border.color: primary ? "transparent" : theme.colors.border

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Text {
            text: button.text
            color: button.primary ? theme.accentForeground : theme.colors.foreground
            font.pixelSize: 13
            font.weight: button.primary ? Font.DemiBold : Font.Normal
        }
        Text {
            visible: button.hint.length > 0 && !button.compact
            text: button.hint
            color: button.primary ? theme.accentForeground : theme.colors.mutedForeground
            opacity: button.primary ? 0.65 : 1
            font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: button.clicked() }
}
