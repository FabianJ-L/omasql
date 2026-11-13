import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every field of the selected row, one under the other, with JSON indented
// and long text in full, so wide rows read without scrolling sideways.
Rectangle {
    id: inspector
    color: theme.colors.darkBackground

    property int row: -1
    property int column: -1
    signal closeRequested()

    readonly property var fields: (backend.results.revision, row >= 0 ? backend.results.rowDetails(row) : [])

    onColumnChanged: if (column >= 0 && column < fields.length) list.positionViewAtIndex(column, ListView.Contain)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 4
            Layout.topMargin: 6
            Label {
                Layout.fillWidth: true
                text: qsTr("Row %1").arg(inspector.row + 1)
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                color: theme.colors.mutedForeground
            }
            ToolButton {
                text: qsTr("Copy")
                font.pixelSize: 12
                Material.foreground: theme.colors.mutedForeground
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Copy the row, tab-separated")
                onClicked: backend.copy(backend.results.rowText(inspector.row))
            }
            ToolButton {
                text: "×"
                font.pixelSize: 16
                Material.foreground: theme.colors.mutedForeground
                onClicked: inspector.closeRequested()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: inspector.fields
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: field
                required property var modelData
                required property int index
                readonly property bool focused: index === inspector.column
                width: ListView.view.width
                height: content.implicitHeight + 16
                color: focused ? Qt.alpha(theme.colors.selection, 0.7) : "transparent"

                Rectangle {
                    visible: field.focused
                    width: 2
                    height: parent.height
                    color: theme.accent
                }

                ColumnLayout {
                    id: content
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    anchors.leftMargin: 14
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: field.modelData.name
                            color: field.focused ? theme.accent : theme.colors.lightForeground
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            // The stored kind, and the declared type when it says something else.
                            readonly property string kind: field.modelData.json ? "json" : field.modelData.kind
                            readonly property string declared: field.modelData.type.toLowerCase()
                            text: kind + (declared.length && declared !== kind ? " · " + declared : "")
                            color: theme.colors.mutedForeground
                            font.family: win.mono
                            font.pixelSize: 10
                        }
                    }
                    TextEdit {
                        Layout.fillWidth: true
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        text: field.modelData.value
                        font.family: win.mono
                        font.pixelSize: field.modelData.kind === "blob" ? 11 : 13
                        font.italic: field.modelData.kind === "null"
                        selectionColor: theme.colors.selection
                        color: ({
                            "null": theme.colors.mutedForeground, "integer": theme.colors.orange,
                            "real": theme.colors.orange, "blob": theme.colors.magenta
                        })[field.modelData.kind] || (field.modelData.json ? theme.colors.cyan : theme.colors.foreground)
                    }
                }
            }
        }
    }
}
