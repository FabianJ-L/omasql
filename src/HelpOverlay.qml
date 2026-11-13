import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut on one screen.
Rectangle {
    id: overlay
    color: Qt.alpha(theme.colors.darkerBackground, 0.92)
    z: 100
    signal closeRequested()

    TapHandler { onTapped: overlay.closeRequested() }

    readonly property var shortcuts: [
        ["Ctrl+Enter", qsTr("Run the statement under the cursor, or the selection")],
        ["Ctrl+Shift+Enter", qsTr("Run everything in the editor")],
        ["Esc", qsTr("Cancel a running query, close lists")],
        ["Ctrl+Space", qsTr("Suggest tables, columns, keywords")],
        ["Tab", qsTr("Insert a suggestion (Enter too, once picked with the arrows)")],
        ["Ctrl+/", qsTr("Comment or uncomment lines")],
        ["Tab / Shift+Tab", qsTr("Indent or outdent lines")],
        ["Ctrl+Z / Ctrl+Shift+Z", qsTr("Undo, redo")],
        ["Ctrl+H", qsTr("Query history")],
        ["Ctrl+K", qsTr("Filter tables and columns")],
        ["Ctrl+1 / 2 / 3", qsTr("Focus tables, editor, results")],
        ["Arrows, Ctrl+C", qsTr("Move through results, copy a cell")],
        ["Ctrl+Shift+C", qsTr("Copy results as CSV")],
        ["Ctrl+E", qsTr("Export results as CSV")],
        ["Ctrl+R", qsTr("Reload the schema")],
        ["Ctrl+O", qsTr("Open a database")],
        ["Ctrl+W", qsTr("Close the database")],
        ["F1 / ?", qsTr("Show or hide this help")],
        ["Ctrl+Q", qsTr("Quit")]
    ]

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(640, overlay.width - 48)
        height: content.implicitHeight + 56
        radius: Math.min(8, theme.radius)
        color: theme.colors.lighterBackground
        border.color: theme.colors.border

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 28
            spacing: 16
            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: theme.colors.brightForeground
            }
            GridLayout {
                id: grid
                columns: 2
                columnSpacing: 24
                rowSpacing: 7
                Repeater {
                    model: overlay.shortcuts.length * 2
                    delegate: Text {
                        required property int index
                        readonly property var entry: overlay.shortcuts[Math.floor(index / 2)]
                        Layout.fillWidth: index % 2 === 1
                        text: entry[index % 2]
                        color: index % 2 ? theme.colors.foreground : theme.accent
                        font.family: index % 2 ? Qt.application.font.family : win.mono
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
