import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Shown until a database is open: open one, reopen a recent one, or start a
// scratch database in memory.
Item {
    id: welcome

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, welcome.width - 64)
        spacing: 18

        Label {
            text: qsTr("Open a SQLite database")
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: theme.colors.brightForeground
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Pick a file, drop one onto this window, or start a scratch database to try something out.")
            color: theme.colors.lightForeground
            font.pixelSize: 14
        }

        RowLayout {
            spacing: 10
            BarButton {
                primary: true
                text: qsTr("Open database…")
                hint: "Ctrl+O"
                implicitHeight: 36
                onClicked: backend.chooseDatabase()
            }
            BarButton {
                text: qsTr("Scratch database")
                implicitHeight: 36
                onClicked: backend.openScratch()
            }
        }

        Label {
            visible: backend.recent.length > 0
            Layout.topMargin: 18
            text: qsTr("Recent")
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            color: theme.colors.mutedForeground
        }

        Repeater {
            model: backend.recent
            delegate: Rectangle {
                id: entry
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 48
                radius: Math.min(4, theme.radius)
                color: entryHover.hovered ? theme.colors.lighterBackground : "transparent"
                opacity: modelData.exists ? 1 : 0.5

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 6
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: entry.modelData.name
                            color: theme.colors.foreground
                            font.pixelSize: 14
                        }
                        Text {
                            Layout.fillWidth: true
                            text: entry.modelData.exists ? entry.modelData.folder : qsTr("Missing: %1").arg(entry.modelData.folder)
                            color: theme.colors.mutedForeground
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }
                    }
                    ToolButton {
                        text: "×"
                        font.pixelSize: 16
                        visible: entryHover.hovered
                        Material.foreground: theme.colors.mutedForeground
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove from this list")
                        onClicked: backend.forgetRecent(entry.modelData.path)
                    }
                }
                HoverHandler { id: entryHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        if (entry.modelData.exists)
                            backend.openDatabase(entry.modelData.path)
                    }
                }
            }
        }
    }
}
